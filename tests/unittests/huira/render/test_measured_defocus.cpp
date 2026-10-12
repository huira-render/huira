#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A 64 x 64 sensor of 10 um pixels behind a 50 mm lens at f/4, whose pixel edges are at
/// integers, so that sensor coordinates and pixel coordinates agree. Focused at 10 m, a point at
/// a range r is blurred by a disc of BLUR_SCALE |1/10 - 1/r| px: 3.125 px for a star, 1.5625 px
/// at 20 m.
constexpr int SIZE = 64;
constexpr double PITCH = 10e-6;
constexpr double FOCAL = 50e-3;
constexpr double FSTOP = 4.0;
constexpr double FOCUS = 10.0;
constexpr double BLUR_SCALE = FOCAL * (FOCAL / (2.0 * FSTOP)) / PITCH;

double blur_at(double range)
{
    return BLUR_SCALE * std::abs(1.0 / FOCUS - 1.0 / range);
}

/// The measured PSF: a round Gaussian, sigma 0.7 px, point-sampled four times a pixel out to
/// RADIUS pixels.
constexpr double SIGMA = 0.7;
constexpr int RADIUS = 6;
constexpr int PER_PIXEL = 4;

Image<RGB> measurement()
{
    const int side = 2 * PER_PIXEL * RADIUS + 1;
    Image<RGB> data(side, side);
    for (int j = 0; j < side; ++j) {
        for (int i = 0; i < side; ++i) {
            const double x = static_cast<double>(i - PER_PIXEL * RADIUS) / PER_PIXEL;
            const double y = static_cast<double>(j - PER_PIXEL * RADIUS) / PER_PIXEL;
            data(i, j) =
                RGB{static_cast<float>(std::exp(-0.5 * (x * x + y * y) / (SIGMA * SIGMA)))};
        }
    }
    return data;
}

/// The Gaussian's light between a and b along an axis, for a source at c.
double gaussian_between(double c, double a, double b)
{
    const double scale = 1.0 / (std::sqrt(2.0) * SIGMA);
    return 0.5 * (std::erf((b - c) * scale) - std::erf((a - c) * scale));
}

/// The integral over c from -half to half of gaussian_between(c, a, b), exactly: with
/// E(z) = z erf(z) + exp(-z^2) / sqrt(pi), the integral of erf((b - c) k) dc is -E((b - c) k) / k.
double chord_between(double half, double a, double b)
{
    const double k = 1.0 / (std::sqrt(2.0) * SIGMA);
    const auto E = [](double z) {
        return z * std::erf(z) + std::exp(-z * z) / std::sqrt(PI<double>());
    };
    const auto F = [&](double edge) { return (E((edge + half) * k) - E((edge - half) * k)) / k; };
    return 0.5 * (F(b) - F(a));
}

/// The light in the pixel whose center is (dx, dy) from the source, of the Gaussian blurred by a
/// disc of radius blur: exact across each chord of the disc, and by Gauss-Legendre quadrature
/// along it, in the angle whose sine is the height over the radius.
double blurred_pixel(double dx, double dy, double blur)
{
    if (blur == 0.0) {
        return gaussian_between(0.0, dx - 0.5, dx + 0.5) *
               gaussian_between(0.0, dy - 0.5, dy + 0.5);
    }
    constexpr std::array<double, 8> NODES{-0.9602898564975363,
                                          -0.7966664774136267,
                                          -0.5255324099163290,
                                          -0.1834346424956498,
                                          0.1834346424956498,
                                          0.5255324099163290,
                                          0.7966664774136267,
                                          0.9602898564975363};
    constexpr std::array<double, 8> WEIGHTS{0.1012285362903763,
                                            0.2223810344533745,
                                            0.3137066458778873,
                                            0.3626837833783620,
                                            0.3626837833783620,
                                            0.3137066458778873,
                                            0.2223810344533745,
                                            0.1012285362903763};
    constexpr int PANELS = 48;
    double sum = 0.0;
    for (int p = 0; p < PANELS; ++p) {
        const double t0 = -0.5 * PI<double>() + PI<double>() * p / PANELS;
        const double half = 0.5 * PI<double>() / PANELS;
        for (std::size_t n = 0; n < NODES.size(); ++n) {
            const double theta = t0 + half * (1.0 + NODES[n]);
            const double height = blur * std::sin(theta);
            sum += WEIGHTS[n] * half * blur * std::cos(theta) *
                   gaussian_between(height, dy - 0.5, dy + 0.5) *
                   chord_between(blur * std::cos(theta), dx - 0.5, dx + 0.5);
        }
    }
    return sum / (PI<double>() * blur * blur);
}

/// The blurred Gaussian on the frame, for a source moving from (x, y) to (x + length, y), at an
/// even pace, or still with no length: its mean over the path.
Image<RGB> expected_on_frame(double x, double y, double blur, double length = 0.0)
{
    const int steps = length > 0.0 ? 200 : 1;
    const double reach = blur + 6.0 * SIGMA + 1.0;
    Image<RGB> image(SIZE, SIZE, RGB{0.f});
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            double value = 0.0;
            for (int k = 0; k < steps; ++k) {
                const double sx = x + length * (k + 0.5) / steps;
                const double dx = i + 0.5 - sx;
                const double dy = j + 0.5 - y;
                if (std::abs(dx) < reach && std::abs(dy) < reach) {
                    value += blurred_pixel(dx, dy, blur) / steps;
                }
            }
            image(i, j) = RGB{static_cast<float>(value)};
        }
    }
    return image;
}

/// Renders one unresolved source at a range with the measured PSF, moving along x from (x, y) to
/// (x + length, y) on the sensor over the one-second exposure, or still with no length.
Image<RGB> render_source(
    double range, double x, double y, double length = 0.0, PSFFocus focus = PSFFocus::InFocus)
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.set_pixel_convention(PixelConvention::colmap());
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, units::Meter(PITCH));
    camera_model.set_focal_length(units::Meter(FOCAL));
    camera_model.set_fstop(static_cast<float>(FSTOP));
    camera_model.set_focus_distance(units::Meter(FOCUS));
    camera_model.set_measured_psf(
        measurement(), static_cast<float>(PER_PIXEL), PSFSampling::PointSampled, focus);
    camera_model.set_sensor_read_noise(10.f);
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1e12)));
    const double focal_pixels = FOCAL / PITCH;
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

/// The most a pixel's green channel differs between two images, each scaled to its sum, over
/// the brightest pixel of the second.
double worst_difference(const Image<RGB>& image, const Image<RGB>& expected)
{
    double image_sum = 0.0;
    double expected_sum = 0.0;
    double peak = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        image_sum += static_cast<double>(image[i][1]);
        expected_sum += static_cast<double>(expected[i][1]);
        peak = std::max(peak, static_cast<double>(expected[i][1]));
    }
    double worst = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        const double a = static_cast<double>(image[i][1]) / image_sum;
        const double b = static_cast<double>(expected[i][1]) / expected_sum;
        worst = std::max(worst, std::abs(a - b));
    }
    return worst / (peak / expected_sum);
}

} // namespace

TEST_CASE("A still source with a measured PSF is blurred by the defocus at its own range",
          "[render][unresolved][psf][measured][defocus]")
{
    // A star-like object far away, blurred 3.125 px; one at 20 m, blurred 1.5625 px; and one
    // at the focus, sharp. Before, all three got a star's blur, from 16 subpixel positions.
    const double x = 30.27;
    const double y = 33.61;
    for (const double range : {1e9, 20.0, 10.0}) {
        INFO("range " << range << " m, blur " << blur_at(range) << " px");
        const Image<RGB> image = render_source(range, x, y);
        CHECK(worst_difference(image, expected_on_frame(x, y, blur_at(range))) < 2e-4);
    }
}

TEST_CASE("A moving source with a measured PSF is blurred by the defocus at its own range",
          "[render][unresolved][psf][measured][defocus][streak]")
{
    // Drawn from the blurred PSF as its path's line integral. Before, it was drawn at points
    // along its path every 0.71 px, 0.0078 of the peak off at the ends.
    const double x = 25.37;
    const double y = 31.81;
    const double length = 11.43;
    for (const double range : {1e9, 20.0}) {
        INFO("range " << range << " m, blur " << blur_at(range) << " px");
        const Image<RGB> image = render_source(range, x, y, length);
        CHECK(worst_difference(image, expected_on_frame(x, y, blur_at(range), length)) < 2e-4);
    }
}

TEST_CASE("A measured PSF that includes the defocus is not blurred again",
          "[render][unresolved][psf][measured][defocus]")
{
    const double x = 30.27;
    const double y = 33.61;
    const Image<RGB> image = render_source(1e9, x, y, 0.0, PSFFocus::IncludesDefocus);
    CHECK(worst_difference(image, expected_on_frame(x, y, 0.0)) < 2e-4);
}

TEST_CASE("psf_image gives a measured PSF blurred by the defocus at a range",
          "[cameras][psf][measured][defocus]")
{
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch({SIZE, SIZE}, units::Meter(PITCH));
    camera.set_focal_length(units::Meter(FOCAL));
    camera.set_fstop(static_cast<float>(FSTOP));
    camera.set_focus_distance(units::Meter(FOCUS));
    camera.set_measured_psf(
        measurement(), static_cast<float>(PER_PIXEL), PSFSampling::PointSampled);
    constexpr int R = 12;
    const float ox = 0.23f;
    const float oy = -0.31f;
    for (const double range : {1e9, 20.0}) {
        INFO("range " << range << " m");
        const Image<RGB> image = camera.psf_image(R, ox, oy, units::Meter(range));
        double worst = 0.0;
        double peak = 0.0;
        for (int j = 0; j < image.height(); ++j) {
            for (int i = 0; i < image.width(); ++i) {
                const double expected = blurred_pixel(static_cast<double>(i - R) - ox,
                                                      static_cast<double>(j - R) - oy,
                                                      blur_at(range));
                peak = std::max(peak, expected);
                worst = std::max(worst, std::abs(static_cast<double>(image(i, j)[1]) - expected));
            }
        }
        CHECK(worst < 1e-4 * peak);
    }
}
