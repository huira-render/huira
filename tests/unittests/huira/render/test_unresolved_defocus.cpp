#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// The Jupiter example's optics, 50 mm at f/3.3 with 8.5 um pixels, on a 64 x 64 sensor whose
/// pixel edges are at integers, so that sensor coordinates and pixel coordinates agree.
constexpr int SIZE = 64;
constexpr double PITCH = 8.5e-6;
constexpr double FSTOP = 3.3;

/// A source this bright stays above the taper over the whole frame, so is drawn in full.
constexpr double BRIGHT = 1e12;

CameraModelHandle<RGB> make_camera(Scene<RGB>& scene, double focal)
{
    auto camera_model = scene.new_camera_model();
    camera_model.set_pixel_convention(PixelConvention::colmap());
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, units::Meter(PITCH));
    camera_model.set_focal_length(units::Meter(focal));
    camera_model.set_fstop(static_cast<float>(FSTOP));
    return camera_model;
}

/// The blur's radius in pixels of a point at a range, with the camera focused at another.
double blur_pixels(double focal, double focus, double range)
{
    return std::abs(1.0 / focus - 1.0 / range) * focal * (focal / (2.0 * FSTOP)) / PITCH;
}

/// Renders one unresolved source at a range, moving along x from (x, y) to (x + length, y) on
/// the sensor over the one-second exposure, or still with no length. Its depth stays the range.
Image<RGB> render_source(Scene<RGB>& scene,
                         CameraModelHandle<RGB>& camera_model,
                         double focal,
                         double range,
                         double x,
                         double y,
                         double length = 0.0)
{
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(BRIGHT)));
    const double focal_pixels = focal / PITCH;
    const double axis = SIZE / 2.0;
    source.set_position(units::Meter((x - axis) / focal_pixels * range),
                        units::Meter((y - axis) / focal_pixels * range),
                        units::Meter(range));
    source.set_velocity(units::MetersPerSecond(length / focal_pixels * range),
                        units::MetersPerSecond(0.0),
                        units::MetersPerSecond(0.0));
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(1.0)};
    // Where the source is, not where its light left it.
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::TRUE_STATE, 3);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

/// The PSF's tables for a source at (x, y), at a range or a star, over the whole frame: pixel
/// (i, j) of the frame is at (i, j) of the result.
Image<RGB> tables_on_frame(CameraModelHandle<RGB>& camera_model,
                           double x,
                           double y,
                           std::optional<units::Meter> range)
{
    const int base_x = static_cast<int>(std::floor(x));
    const int base_y = static_cast<int>(std::floor(y));
    const Image<RGB> table = camera_model.psf_image(
        SIZE, static_cast<float>(x - base_x - 0.5), static_cast<float>(y - base_y - 0.5), range);
    Image<RGB> result(SIZE, SIZE, RGB{0.f});
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            result(i, j) = table(i - base_x + SIZE, j - base_y + SIZE);
        }
    }
    return result;
}

/// The path from (x, y) to (x + length, y) integrated over the PSF's tables for a star, over
/// the whole frame, per unit of light: Gauss-Legendre quadrature, three points per sixty-fourth
/// of a pixel.
Image<RGB> streak_on_frame(CameraModelHandle<RGB>& camera_model, double x, double y, double length)
{
    constexpr std::array<double, 3> NODES{-0.7745966692414834, 0.0, 0.7745966692414834};
    constexpr std::array<double, 3> WEIGHTS{5.0 / 9.0, 8.0 / 9.0, 5.0 / 9.0};
    const int panels = static_cast<int>(std::ceil(64.0 * length));
    std::vector<std::array<double, 3>> sum(static_cast<std::size_t>(SIZE * SIZE));
    for (int k = 0; k < panels; ++k) {
        for (std::size_t n = 0; n < NODES.size(); ++n) {
            const double t = (k + 0.5 + 0.5 * NODES[n]) / panels;
            const double weight = 0.5 * WEIGHTS[n] / panels;
            const Image<RGB> point = tables_on_frame(camera_model, x + t * length, y, std::nullopt);
            for (std::size_t i = 0; i < sum.size(); ++i) {
                for (std::size_t c = 0; c < 3; ++c) {
                    sum[i][c] += weight * static_cast<double>(point[i][c]);
                }
            }
        }
    }
    Image<RGB> result(SIZE, SIZE, RGB{0.f});
    for (std::size_t i = 0; i < sum.size(); ++i) {
        result[i] = RGB{static_cast<float>(sum[i][0]),
                        static_cast<float>(sum[i][1]),
                        static_cast<float>(sum[i][2])};
    }
    return result;
}

/// The most a rendered frame, scaled to the expected one at its brightest pixel, differs from
/// it anywhere, over that brightest pixel's value; per channel, the worst.
double worst_difference(const Image<RGB>& rendered, const Image<RGB>& expected)
{
    double worst = 0.0;
    for (std::size_t c = 0; c < 3; ++c) {
        std::size_t brightest = 0;
        for (std::size_t i = 0; i < expected.size(); ++i) {
            if (expected[i][c] > expected[brightest][c]) {
                brightest = i;
            }
        }
        const double peak = static_cast<double>(expected[brightest][c]);
        const double power = static_cast<double>(rendered[brightest][c]) / peak;
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const double difference =
                static_cast<double>(rendered[i][c]) / power - static_cast<double>(expected[i][c]);
            worst = std::max(worst, std::abs(difference) / peak);
        }
    }
    return worst;
}

} // namespace

TEST_CASE("A still unresolved object is blurred by the defocus at its own range",
          "[render][unresolved][psf][defocus]")
{
    // Focused at 10 m, a star is blurred 4.46 px. An object at 10 m is sharp; at 20 m and 5 m
    // it is blurred 2.23 and 4.46 px, from either side of the focus.
    constexpr double FOCAL = 50e-3;
    constexpr double FOCUS = 10.0;
    const double x = 30.27;
    const double y = 33.61;
    for (const double range : {10.0, 20.0, 5.0}) {
        INFO("range " << range << " m, blur " << blur_pixels(FOCAL, FOCUS, range) << " px");
        Scene<RGB> scene;
        auto camera_model = make_camera(scene, FOCAL);
        camera_model.set_focus_distance(units::Meter(FOCUS));
        const Image<RGB> image = render_source(scene, camera_model, FOCAL, range, x, y);
        const Image<RGB> own = tables_on_frame(camera_model, x, y, units::Meter(range));
        const Image<RGB> star = tables_on_frame(camera_model, x, y, std::nullopt);

        // Drawn as the tables at its range give it, to the renderer's single-precision
        // projection, and not as a star's:
        CHECK(worst_difference(image, own) < 1e-4);
        if (range != 5.0) {
            CHECK(worst_difference(image, star) > 1e-2);
        }
    }
}

TEST_CASE("A moving unresolved object is blurred by the defocus at its own range",
          "[render][unresolved][psf][defocus][streak]")
{
    // At ten times the focal length, so that the light delivered hardly changes across the
    // frame. Focused at 1 km, a star is blurred 4.46 px, and an object at 1 km is sharp; at
    // 2 km it is blurred 2.23 px.
    constexpr double FOCAL = 500e-3;
    constexpr double FOCUS = 1000.0;
    const double x = 25.37;
    const double y = 31.81;
    const double length = 11.43;
    for (const double range : {1000.0, 2000.0}) {
        INFO("range " << range << " m, blur " << blur_pixels(FOCAL, FOCUS, range) << " px");
        Scene<RGB> scene;
        auto camera_model = make_camera(scene, FOCAL);
        camera_model.set_focus_distance(units::Meter(FOCUS));
        const Image<RGB> image = render_source(scene, camera_model, FOCAL, range, x, y, length);

        // The reference: a star's tables, for a camera focused where a star is blurred as much
        // as the object is here (at infinity for none). They are the object's own, and built
        // once, rather than for each point of the path.
        Scene<RGB> reference_scene;
        auto reference = make_camera(reference_scene, FOCAL);
        const double vergence = std::abs(1.0 / FOCUS - 1.0 / range);
        if (vergence > 0.0) {
            reference.set_focus_distance(units::Meter(1.0 / vergence));
        }
        const Image<RGB> expected = streak_on_frame(reference, x, y, length);
        CHECK(worst_difference(image, expected) < 1e-4);
    }
}

TEST_CASE("An unresolved object far enough away is drawn with the stars' blur",
          "[render][unresolved][psf][defocus]")
{
    // Its blur differs from a star's by under the tables' tolerance, so it is drawn from the
    // stars' tables, out of focus or in it.
    constexpr double FOCAL = 50e-3;
    const double x = 30.27;
    const double y = 33.61;
    for (const double focus : {10.0, 0.0}) {
        INFO((focus > 0.0 ? "focused at 10 m" : "focused at infinity"));
        Scene<RGB> scene;
        auto camera_model = make_camera(scene, FOCAL);
        if (focus > 0.0) {
            camera_model.set_focus_distance(units::Meter(focus));
        }
        const Image<RGB> image = render_source(scene, camera_model, FOCAL, 1e9, x, y);
        const Image<RGB> star = tables_on_frame(camera_model, x, y, std::nullopt);
        CHECK(worst_difference(image, star) < 1e-4);
    }
}
