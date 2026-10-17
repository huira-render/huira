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
                         std::size_t temporal_samples = 8)
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
    SceneView<RGB> scene_view(
        scene, exposure, camera, ObservationMode::GEOMETRIC_STATE, temporal_samples);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
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
