#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A 64 x 64 sensor of 10 um pixels behind a 50 mm lens, whose pixel edges are at integers, so
/// that sensor coordinates and pixel coordinates agree.
constexpr int SIZE = 64;
constexpr double PITCH = 10e-6;
constexpr double FOCAL = 50e-3;
constexpr double RANGE = 1e6;

/// The measured PSF: an elliptical Gaussian, sigma 0.7 px across and 0.5 px down, so that its
/// light in any pixel is known exactly; sampled out to RADIUS pixels.
constexpr double SIGMA_X = 0.7;
constexpr double SIGMA_Y = 0.5;
constexpr int RADIUS = 6;

/// The Gaussian's light between a and b along an axis, for a source at 0.
double gaussian_between(double a, double b, double sigma)
{
    const double scale = 1.0 / (std::sqrt(2.0) * sigma);
    return 0.5 * (std::erf(b * scale) - std::erf(a * scale));
}

/// The light in the pixel whose center is (dx, dy) from the source.
double pixel_light(double dx, double dy)
{
    return gaussian_between(dx - 0.5, dx + 0.5, SIGMA_X) *
           gaussian_between(dy - 0.5, dy + 0.5, SIGMA_Y);
}

/// The Gaussian sampled per_pixel times a pixel: its intensity at points, or what a pixel
/// centered at each collects.
Image<RGB> measurement(int per_pixel, PSFSampling sampling)
{
    const int side = 2 * per_pixel * RADIUS + 1;
    Image<RGB> data(side, side);
    for (int j = 0; j < side; ++j) {
        for (int i = 0; i < side; ++i) {
            const double x = static_cast<double>(i - per_pixel * RADIUS) / per_pixel;
            const double y = static_cast<double>(j - per_pixel * RADIUS) / per_pixel;
            const double value =
                sampling == PSFSampling::PointSampled
                    ? std::exp(-0.5 * (x * x / (SIGMA_X * SIGMA_X) + y * y / (SIGMA_Y * SIGMA_Y)))
                    : pixel_light(x, y);
            data(i, j) = RGB{static_cast<float>(value)};
        }
    }
    return data;
}

/// A PSF with wings that fall as the aperture's do, as r^-3, about 1.2 px across: four point
/// samples per pixel out to WINGED_RADIUS pixels.
constexpr int WINGED_RADIUS = 16;

Image<RGB> winged()
{
    constexpr int PER_PIXEL = 4;
    constexpr int SIDE = 2 * PER_PIXEL * WINGED_RADIUS + 1;
    Image<RGB> data(SIDE, SIDE);
    for (int j = 0; j < SIDE; ++j) {
        for (int i = 0; i < SIDE; ++i) {
            const double x = static_cast<double>(i - PER_PIXEL * WINGED_RADIUS) / PER_PIXEL;
            const double y = static_cast<double>(j - PER_PIXEL * WINGED_RADIUS) / PER_PIXEL;
            data(i, j) = RGB{static_cast<float>(std::pow(1.0 + (x * x + y * y) / 0.36, -1.5))};
        }
    }
    return data;
}

/// Renders one still unresolved source of a power at (x, y) on the sensor, with a measured
/// PSF, and returns the received power.
Image<RGB> render_with(double x,
                       double y,
                       double watts,
                       const Image<RGB>& data,
                       int per_pixel,
                       PSFSampling sampling,
                       int radius,
                       bool scatter = false)
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.set_pixel_convention(PixelConvention::colmap());
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, units::Meter(PITCH));
    camera_model.set_focal_length(units::Meter(FOCAL));
    camera_model.set_measured_psf(data, static_cast<float>(per_pixel), sampling, radius);
    camera_model.set_sensor_read_noise(10.f);
    if (scatter) {
        camera_model.set_harvey_shack_scatter(0.1f, 3.0f, 0.5f);
    }
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(watts)));
    const double focal_pixels = FOCAL / PITCH;
    const double axis = SIZE / 2.0;
    source.set_position(units::Meter((x - axis) / focal_pixels * RANGE),
                        units::Meter((y - axis) / focal_pixels * RANGE),
                        units::Meter(RANGE));
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(1.0)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

/// The same with the Gaussian.
Image<RGB> render_at(double x, double y, double watts, int per_pixel, PSFSampling sampling)
{
    return render_with(x, y, watts, measurement(per_pixel, sampling), per_pixel, sampling, RADIUS);
}

/// The green channel's sum.
double total(const Image<RGB>& image)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        sum += static_cast<double>(image[i][1]);
    }
    return sum;
}

/// How a rendered source compares with the Gaussian's light at (x, y): the most a pixel differs,
/// over the brightest's light, and how far the centroid is, both with the image scaled to the
/// light on the frame.
struct Comparison {
    double worst = 0.0;
    double centroid_x = 0.0;
    double centroid_y = 0.0;
};

Comparison compare(const Image<RGB>& image, double x, double y)
{
    double expected_total = 0.0;
    double peak = 0.0;
    std::array<double, 2> expected_moment{};
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            const double light = pixel_light(i + 0.5 - x, j + 0.5 - y);
            expected_total += light;
            peak = std::max(peak, light);
            expected_moment = {expected_moment[0] + light * (i + 0.5),
                               expected_moment[1] + light * (j + 0.5)};
        }
    }
    const double scale = expected_total / total(image);
    Comparison result;
    std::array<double, 2> moment{};
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            const double rendered = scale * static_cast<double>(image(i, j)[1]);
            const double light = pixel_light(i + 0.5 - x, j + 0.5 - y);
            result.worst = std::max(result.worst, std::abs(rendered - light) / peak);
            moment = {moment[0] + rendered * (i + 0.5), moment[1] + rendered * (j + 0.5)};
        }
    }
    result.centroid_x = std::abs(moment[0] - expected_moment[0]) / expected_total;
    result.centroid_y = std::abs(moment[1] - expected_moment[1]) / expected_total;
    return result;
}

} // namespace

TEST_CASE("A still source with a measured PSF is drawn at its exact position",
          "[render][unresolved][psf][measured]")
{
    // Positions across a pixel, and just either side of its edges. Stamps at sixteenths of a
    // pixel put a source up to 1/32 px off, which moves a core this narrow's pixels by several
    // percent of the peak.
    for (const PSFSampling sampling : {PSFSampling::PointSampled, PSFSampling::PixelIntegrated}) {
        const int per_pixel = sampling == PSFSampling::PointSampled ? 8 : 4;
        INFO((sampling == PSFSampling::PointSampled ? "point samples, 8 per pixel"
                                                    : "pixel-integrated samples, 4 per pixel"));
        for (const auto& [x, y] : {std::array<double, 2>{32.5, 32.5},
                                   {32.03, 31.98},
                                   {31.97, 32.27},
                                   {32.31, 32.66},
                                   {32.53, 32.02}}) {
            INFO("source at (" << x << ", " << y << ")");
            const Comparison result = compare(render_at(x, y, 1e3, per_pixel, sampling), x, y);
            CHECK(result.worst < 1e-3);
            CHECK(result.centroid_x < 2e-4);
            CHECK(result.centroid_y < 2e-4);
        }
    }
}

TEST_CASE("A still source with a measured PSF puts all its light that falls on the frame on it",
          "[render][unresolved][psf][measured]")
{
    // Half a pixel and a pixel and a half inside the left edge, and beyond it: the light on the
    // frame is the Gaussian's there, for a bright source and a faint one that fades out within
    // a few pixels. Cropped stamps were scaled up to the whole stamp's light, wherever it fell.
    for (const PSFSampling sampling : {PSFSampling::PointSampled, PSFSampling::PixelIntegrated}) {
        // Eight samples a pixel, so that the light on the frame is not limited by how well the
        // spline follows the Gaussian:
        const int per_pixel = 8;
        INFO((sampling == PSFSampling::PointSampled ? "point samples" : "pixel-integrated"));
        for (const double x : {0.5, 1.5, -0.4}) {
            const double y = 30.3;
            double on_frame = 0.0;
            for (int i = 0; i < SIZE; ++i) {
                on_frame += gaussian_between(i - x, i + 1.0 - x, SIGMA_X);
            }
            const double bright = total(render_at(x, y, 1e3, per_pixel, sampling)) / 1e3;
            const double faint = total(render_at(x, y, 1e-6, per_pixel, sampling)) / 1e-6;
            // The light per watt with the source wholly on the frame, as far from the axis, so
            // that the aperture is seen at the same slant:
            const double off_axis = std::hypot(x - SIZE / 2.0, y - SIZE / 2.0) / std::sqrt(2.0);
            const double whole =
                total(render_at(
                    SIZE / 2.0 + off_axis, SIZE / 2.0 + off_axis, 1e3, per_pixel, sampling)) /
                1e3;
            INFO("source at x = " << x << ": " << on_frame << " of its light on the frame");
            CHECK(std::abs(bright / whole - on_frame) < 2e-5);
            CHECK(std::abs(faint / whole - on_frame) < 2e-5);
        }
    }
}

TEST_CASE("A faint source with a measured PSF keeps its pixels' light where it is drawn",
          "[render][unresolved][psf][measured]")
{
    // A faint source is drawn out to where its pixels read out a thousandth of the read noise,
    // and fades out beyond, with the rest of its light spread over the frame; within that its
    // pixels are the bright source's, scaled, however far its wings reach.
    const double x = 31.27;
    const double y = 32.61;
    const Image<RGB> data = winged();
    const Image<RGB> bright =
        render_with(x, y, 1e3, data, 4, PSFSampling::PointSampled, WINGED_RADIUS);
    const Image<RGB> faint =
        render_with(x, y, 1e-6, data, 4, PSFSampling::PointSampled, WINGED_RADIUS);
    double worst = 0.0;
    double peak = 0.0;
    for (std::size_t i = 0; i < bright.size(); ++i) {
        const double expected = static_cast<double>(bright[i][1]) * 1e-9;
        peak = std::max(peak, expected);
        if (std::hypot(static_cast<double>(i % SIZE) + 0.5 - x,
                       static_cast<double>(i / SIZE) + 0.5 - y) < 1.5) {
            worst = std::max(worst, std::abs(static_cast<double>(faint[i][1]) - expected));
        }
    }
    CHECK(worst < 1e-4 * peak);
}

TEST_CASE("A still source with a measured PSF and scattered light keeps its light",
          "[render][unresolved][psf][measured][scatter]")
{
    // The core takes nine tenths of the light, drawn from the PSF's grid, and the scattered
    // light the rest, from its own tables; a faint source fades out, and what it leaves is
    // spread. What reaches beyond the frame is the scattered light's, as its tables give it.
    const Image<RGB> data = measurement(8, PSFSampling::PointSampled);
    const auto scatter = PsfTables<RGB>::airy(
        4.0, PITCH, PITCH, 4096.0, PsfTables<RGB>::Scatter{1.0, 3.0, 0.5, std::nullopt});
    const double scattered_on_frame =
        scatter.light_in_rectangle(-32.2, SIZE - 32.2, -32.3, SIZE - 32.3)[1];
    REQUIRE(scattered_on_frame < 0.99);
    for (const double watts : {1e3, 1e-6}) {
        INFO(watts << " W");
        const double without =
            total(render_with(32.2, 32.3, watts, data, 8, PSFSampling::PointSampled, RADIUS));
        const double with =
            total(render_with(32.2, 32.3, watts, data, 8, PSFSampling::PointSampled, RADIUS, true));
        CHECK(std::abs(with / without - (0.9 + 0.1 * scattered_on_frame)) < 1e-5);
    }
}
