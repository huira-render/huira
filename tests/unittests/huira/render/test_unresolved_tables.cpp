#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// The Jupiter example's optics, 50 mm at f/3.3 with 8.5 um pixels, on a 64 x 64 sensor whose
/// pixel edges are at integers, so that sensor coordinates and pixel coordinates agree.
constexpr int SIZE = 64;
constexpr double FOCAL_PIXELS = 50e-3 / 8.5e-6;
constexpr double RANGE = 1e6;

CameraModelHandle<RGB> make_camera(Scene<RGB>& scene)
{
    auto camera_model = scene.new_camera_model();
    camera_model.set_pixel_convention(PixelConvention::colmap());
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, 8.5_um);
    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(3.3f);
    return camera_model;
}

/// Renders one still unresolved source at a position on the sensor, with stamps uncropped.
Image<RGB> render_at(Scene<RGB>& scene,
                     CameraModelHandle<RGB>& camera_model,
                     const InstanceHandle<RGB>& camera,
                     const InstanceHandle<RGB>& source,
                     double x,
                     double y)
{
    const double axis = SIZE / 2.0;
    source.set_position(units::Meter((x - axis) / FOCAL_PIXELS * RANGE),
                        units::Meter((y - axis) / FOCAL_PIXELS * RANGE),
                        units::Meter(RANGE));
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_stamp_cropping(false);
    Interval exposure{Time::from_et(0.0), Time::from_et(1.0)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

/// Each channel's sum.
std::array<double, 3> sums(const Image<RGB>& image)
{
    std::array<double, 3> sum{};
    for (std::size_t i = 0; i < image.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            sum[c] += static_cast<double>(image[i][c]);
        }
    }
    return sum;
}

} // namespace

TEST_CASE("A still source is drawn from the PSF's tables at its exact position",
          "[render][unresolved][psf]")
{
    Scene<RGB> scene;
    auto camera_model = make_camera(scene);
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
    const int radius = camera_model.get_psf_radius();

    // Positions across a pixel, including just either side of its edges, where a PSF core
    // smaller than a pixel moves its centroid fastest. Stamps at sixteenths of a pixel put these
    // up to 1/32 px off, and the centroid up to four times as far.
    for (const auto& [x, y] : {std::array<double, 2>{32.5, 32.5},
                               {32.03, 31.98},
                               {31.97, 32.27},
                               {32.31, 32.66},
                               {32.53, 32.02}}) {
        INFO("source at (" << x << ", " << y << ")");
        const Image<RGB> image = render_at(scene, camera_model, camera, source, x, y);

        // The rendered stamp is the PSF's tables around the source, normalized, as psf_image()
        // gives them. Its center pixel is the one the source falls in.
        const int base_x = static_cast<int>(std::floor(x));
        const int base_y = static_cast<int>(std::floor(y));
        const Image<RGB> expected = camera_model.psf_image(
            radius, static_cast<float>(x - base_x - 0.5), static_cast<float>(y - base_y - 0.5));
        const std::array<double, 3> rendered_sum = sums(image);
        const std::array<double, 3> expected_sum = sums(expected);
        for (std::size_t c = 0; c < 3; ++c) {
            double worst = 0.0;
            double peak = 0.0;
            std::array<double, 2> rendered_moment{};
            std::array<double, 2> expected_moment{};
            for (int j = -radius; j <= radius; ++j) {
                for (int i = -radius; i <= radius; ++i) {
                    const double a =
                        static_cast<double>(image(base_x + i, base_y + j)[c]) / rendered_sum[c];
                    const double b =
                        static_cast<double>(expected(i + radius, j + radius)[c]) / expected_sum[c];
                    worst = std::max(worst, std::abs(a - b));
                    peak = std::max(peak, b);
                    rendered_moment = {rendered_moment[0] + a * i, rendered_moment[1] + a * j};
                    expected_moment = {expected_moment[0] + b * i, expected_moment[1] + b * j};
                }
            }
            // The renderer projects in single precision, which moves the source by about 2e-6 px.
            INFO("channel " << c);
            CHECK(worst < 1e-4 * peak);
            CHECK(std::abs(rendered_moment[0] - expected_moment[0]) < 1e-5);
            CHECK(std::abs(rendered_moment[1] - expected_moment[1]) < 1e-5);
        }
    }
}

TEST_CASE("Moving sources are drawn from stamps made from the PSF's tables",
          "[render][unresolved][psf]")
{
    Scene<RGB> scene;
    auto camera_model = make_camera(scene);
    camera_model.use_aperture_psf(8, 4);

    // Bank (bx, by) holds a source bx / 4, by / 4 of a pixel from the center pixel's center,
    // normalized per channel:
    for (const auto& [bx, by] : {std::array<int, 2>{0, 0}, {1, 3}, {3, 2}}) {
        const float u = static_cast<float>(bx) / 4.f;
        const float v = static_cast<float>(by) / 4.f;
        const Image<RGB>& stamp = camera_model.get_psf_kernel(u + 0.125f, v + 0.125f);
        const Image<RGB> expected = camera_model.psf_image(8, u, v);
        const std::array<double, 3> expected_sum = sums(expected);
        REQUIRE(stamp.width() == expected.width());
        double worst = 0.0;
        for (int y = 0; y < stamp.height(); ++y) {
            for (int x = 0; x < stamp.width(); ++x) {
                for (std::size_t c = 0; c < 3; ++c) {
                    worst = std::max(
                        worst,
                        std::abs(static_cast<double>(stamp(x, y)[c]) -
                                 static_cast<double>(expected(x, y)[c]) / expected_sum[c]));
                }
            }
        }
        INFO("bank (" << bx << ", " << by << ")");
        CHECK(worst < 1e-6);
    }
}
