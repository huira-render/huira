#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A 10 ms exposure, made from the time it is centered on.
Interval centered_exposure()
{
    return Interval::from_centered(Time::from_et(0.0), 10_ms);
}

/// The received power of a sphere 1 m across, 1 km in front of the camera, moving sideways at
/// `speed` during the exposure, on a strip of sensor 512 x 32 pixels of 0.1 m at that range. At
/// 1750 m/s it crosses 175 pixels in 10 ms, so a pixel on its path sees it for about 6% of the
/// exposure.
Image<RGB> render_sphere(units::MetersPerSecond speed,
                         int samples_per_pixel,
                         const Interval& exposure = centered_exposure(),
                         std::size_t temporal_samples = 8,
                         bool region_culling = true)
{
    Scene<RGB> scene;
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    auto sphere = scene.root.new_instance(
        scene.add_primitive(scene.add_ellipsoid(0.5_m, 0.5_m, 0.5_m), material));
    sphere.set_position(0_m, 0_m, 1000_m);
    sphere.set_velocity(speed, 0_mps, 0_mps);

    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, 0_m, -1_au);

    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({512, 32}, 10_um);
    camera_model.set_focal_length(100_mm);
    camera_model.enable_diffraction(false);
    camera_model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(camera_model);

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(samples_per_pixel);
    renderer.set_region_culling(region_culling);
    SceneView<RGB> scene_view(
        scene, exposure, camera, ObservationMode::GEOMETRIC_STATE, temporal_samples);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

/// A vertex with a position and a normal.
Vertex<RGB> vertex(const Vec3<float>& position, const Vec3<float>& normal)
{
    Vertex<RGB> v;
    v.position = position;
    v.normal = normal;
    return v;
}

/// The received power of a square 1 m across, facing the camera 1 km away, that turns a third of
/// a turn about the line of sight during a 1 s exposure, rendered with 2 temporal samples. The
/// square is 8 m from its instance's origin, and the instance moves so that the square ends where
/// it starts: halfway through, it is 4 m (40 pixels) from there.
Image<RGB> render_turning_square(bool region_culling)
{
    Scene<RGB> scene;
    const Vec3<float> normal{0.f, 0.f, -1.f};
    const VertexBuffer<RGB> vertices{
        vertex({7.5f, -0.5f, 0.f}, normal),
        vertex({8.5f, -0.5f, 0.f}, normal),
        vertex({8.5f, 0.5f, 0.f}, normal),
        vertex({7.5f, 0.5f, 0.f}, normal),
    };
    auto square_mesh = scene.add_mesh(IndexBuffer{0, 2, 1, 0, 3, 2}, vertices);
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    auto square = scene.root.new_instance(scene.add_primitive(square_mesh, material));
    square.set_position(0_m, 0_m, 1000_m);

    // Turning by 120 degrees carries the square's center from (8, 0) to (-4, 6.93) about the
    // instance's origin, so the instance moves by (12, -6.93) m to bring it back:
    square.set_angular_velocity(units::RadiansPerSecond(0.0),
                                units::RadiansPerSecond(0.0),
                                units::RadiansPerSecond(2.0 * PI<double>() / 3.0));
    square.set_velocity(units::MetersPerSecond(12.0),
                        units::MetersPerSecond(-8.0 * std::sin(2.0 * PI<double>() / 3.0)),
                        0_mps);

    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, 0_m, -1_au);

    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({256, 256}, 10_um);
    camera_model.set_focal_length(100_mm);
    camera_model.enable_diffraction(false);
    camera_model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(camera_model);

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(16);
    renderer.set_region_culling(region_culling);
    Interval exposure{Time::from_et(0.0), Time::from_et(1.0)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE, 2);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

/// The number of pixels that differ between two images.
std::size_t mismatched_pixels(const Image<RGB>& a, const Image<RGB>& b)
{
    std::size_t mismatched = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            if (a[i][c] != b[i][c]) {
                ++mismatched;
                break;
            }
        }
    }
    return mismatched;
}

/// The number of pixels that receive any light.
std::size_t lit_pixels(const Image<RGB>& image)
{
    std::size_t lit = 0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        if (image[i].total() > 0.f) {
            ++lit;
        }
    }
    return lit;
}

double total(const Image<RGB>& image)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        sum += image[i].total();
    }
    return sum;
}

/// The power along the streak's brightest row, without the 10 pixels at either end.
std::vector<double> middle_of_streak(const Image<RGB>& image)
{
    int row = 0;
    double brightest = -1.0;
    for (int y = 0; y < image.height(); ++y) {
        double sum = 0.0;
        for (int x = 0; x < image.width(); ++x) {
            sum += image(x, y).total();
        }
        if (sum > brightest) {
            brightest = sum;
            row = y;
        }
    }
    std::vector<double> values;
    for (int x = 0; x < image.width(); ++x) {
        values.push_back(image(x, row).total());
    }
    const double peak = *std::max_element(values.begin(), values.end());
    int first = 0;
    int last = image.width() - 1;
    while (values[static_cast<std::size_t>(first)] < 0.2 * peak) {
        ++first;
    }
    while (values[static_cast<std::size_t>(last)] < 0.2 * peak) {
        --last;
    }
    return {values.begin() + first + 10, values.begin() + last - 9};
}

double mean(const std::vector<double>& values)
{
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    return sum / static_cast<double>(values.size());
}

/// The column of the image's center of brightness, in pixels.
double centroid_column(const Image<RGB>& image)
{
    double sum = 0.0;
    double moment = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double power = image(x, y).total();
            sum += power;
            moment += power * static_cast<double>(x);
        }
    }
    return moment / sum;
}

double relative_spread(const std::vector<double>& values)
{
    const double m = mean(values);
    double sum = 0.0;
    for (double v : values) {
        sum += (v - m) * (v - m);
    }
    return std::sqrt(sum / static_cast<double>(values.size())) / m;
}

} // namespace

TEST_CASE("A moving object's streak converges with few samples", "[render][motion_blur]")
{
    // Each pixel's samples are spread over the exposure, so every pixel on the streak catches
    // the sphere about as many times as the others. With independent random times, the count
    // varies from pixel to pixel: about 40% at 64 samples, where spread times give about 15%.
    const Image<RGB> moving = render_sphere(1750_mps, 64);
    const std::vector<double> streak = middle_of_streak(moving);
    REQUIRE(streak.size() > 100);
    CHECK(relative_spread(streak) < 0.25);

    // The times still cover the exposure evenly, so the streak is as bright at one end as at
    // the other...
    const std::size_t half = streak.size() / 2;
    const double first_half =
        mean({streak.begin(), streak.begin() + static_cast<std::ptrdiff_t>(half)});
    const double second_half =
        mean({streak.begin() + static_cast<std::ptrdiff_t>(half), streak.end()});
    CHECK(std::abs(first_half / second_half - 1.0) < 0.1);

    // ...and the sphere's light is spread along it, not lost: the frame receives what it does
    // from the sphere held still.
    const Image<RGB> still = render_sphere(0_mps, 64);
    CHECK(std::abs(total(moving) / total(still) - 1.0) < 0.03);
}

TEST_CASE("A pose set by hand refers to the same time with and without motion blur",
          "[render][motion_blur]")
{
    // Held still, the sphere marks the position set by hand. Moving, it crosses 175 pixels during
    // the 10 ms exposure. With motion blur its light is spread along the streak, and without it
    // the sphere is drawn at mid-exposure, which must be the middle of the streak: an image
    // measured with motion blur or without gives the same position. (Within 2 pixels: the
    // streak's centroid varies by a few tenths of a pixel with the samples taken, where the two
    // references differ by 87.5 pixels.)
    const Time t = Time::from_et(0.0);
    const double still = centroid_column(render_sphere(0_mps, 16, Interval::from_start(t, 10_ms)));

    SECTION("An interval made from its start refers to its start")
    {
        // The streak starts at the position set, so its middle is half the crossing on:
        const Interval exposure = Interval::from_start(t, 10_ms);
        const double blurred = centroid_column(render_sphere(1750_mps, 64, exposure, 8));
        const double unblurred = centroid_column(render_sphere(1750_mps, 16, exposure, 1));
        CHECK(std::abs(blurred - (still + 87.5)) < 2.0);
        CHECK(std::abs(unblurred - (still + 87.5)) < 2.0);
    }

    SECTION("An interval made from its center refers to its center")
    {
        // The streak is centered on the position set:
        const Interval exposure = Interval::from_centered(t, 10_ms);
        const double blurred = centroid_column(render_sphere(1750_mps, 64, exposure, 8));
        const double unblurred = centroid_column(render_sphere(1750_mps, 16, exposure, 1));
        CHECK(std::abs(blurred - still) < 2.0);
        CHECK(std::abs(unblurred - still) < 2.0);
    }
}

TEST_CASE("Region culling keeps what a moving object passes through between temporal samples",
          "[render][motion_blur][culling]")
{
    // Region culling skips the parts of the frame that nothing can reach. Between two temporal
    // samples, a moving object is where neither sample put it, and those parts must not be
    // skipped. Each tile's samples are seeded by the tile, so culling must change nothing at
    // all: the frame must come out the same with it as without it.
    SECTION("Moving in a straight line")
    {
        const Image<RGB> still = render_sphere(0_mps, 64);
        for (std::size_t samples : {std::size_t{2}, std::size_t{3}}) {
            INFO(samples << " temporal samples");
            const Image<RGB> culled =
                render_sphere(1750_mps, 64, centered_exposure(), samples, true);
            const Image<RGB> reference =
                render_sphere(1750_mps, 64, centered_exposure(), samples, false);
            CHECK(mismatched_pixels(culled, reference) == 0);

            // ...and the streak keeps all of the sphere's light:
            CHECK(std::abs(total(culled) / total(still) - 1.0) < 0.03);
        }
    }

    SECTION("Turning while it moves")
    {
        // Its positions at the two samples coincide, so only a bound that allows for the turn
        // holds it in between.
        const Image<RGB> culled = render_turning_square(true);
        const Image<RGB> reference = render_turning_square(false);
        REQUIRE(lit_pixels(reference) > 400);
        CHECK(mismatched_pixels(culled, reference) == 0);
    }
}
