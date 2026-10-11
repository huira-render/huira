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

/// A 64 x 64 sensor of 10 um pixels behind a 50 mm lens at f/4, whose pixel edges are at
/// integers, so that sensor coordinates and pixel coordinates agree. Focused at 10 m, a point at
/// 20 m is blurred by a disc of 1.5625 px.
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

/// Harvey-Shack scattered light: a tenth of the light, flat within half a pixel and falling as
/// r^-3 beyond.
constexpr double SCATTERED = 0.1;
constexpr double SLOPE = 3.0;
constexpr double SHOULDER = 0.5;

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

/// The Gaussian's light between a and b along an axis, for a source at 0.
double gaussian_between(double a, double b)
{
    const double scale = 1.0 / (std::sqrt(2.0) * SIGMA);
    return 0.5 * (std::erf(b * scale) - std::erf(a * scale));
}

/// The scattered light's profile integrated over the pixel whose center is (dx, dy) from the
/// source, by 8-point Gauss-Legendre quadrature on each of n x n parts of the pixel.
double scattered_pixel(double dx, double dy)
{
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
    static const detail::ScatterProfile profile(SHOULDER, SLOPE, std::nullopt);
    // Finely near the source, where the shoulder bends the profile:
    const int n = std::hypot(dx, dy) < 3.0 ? 16 : 2;
    const double part = 1.0 / n;
    double sum = 0.0;
    for (int py = 0; py < n; ++py) {
        for (int px = 0; px < n; ++px) {
            const double x0 = dx - 0.5 + px * part;
            const double y0 = dy - 0.5 + py * part;
            for (std::size_t a = 0; a < NODES.size(); ++a) {
                for (std::size_t b = 0; b < NODES.size(); ++b) {
                    const double x = x0 + 0.5 * part * (1.0 + NODES[a]);
                    const double y = y0 + 0.5 * part * (1.0 + NODES[b]);
                    sum += 0.25 * part * part * WEIGHTS[a] * WEIGHTS[b] * profile(std::hypot(x, y));
                }
            }
        }
    }
    return sum;
}

/// The camera, with the measured PSF or none, scattered light, and focused at 10 m.
CameraModelHandle<RGB> make_camera(Scene<RGB>& scene, bool with_psf)
{
    auto camera_model = scene.new_camera_model();
    camera_model.set_pixel_convention(PixelConvention::colmap());
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, units::Meter(PITCH));
    camera_model.set_focal_length(units::Meter(FOCAL));
    camera_model.set_fstop(static_cast<float>(FSTOP));
    camera_model.set_focus_distance(units::Meter(FOCUS));
    if (with_psf) {
        camera_model.set_measured_psf(
            measurement(), static_cast<float>(PER_PIXEL), PSFSampling::PointSampled, RADIUS);
    } else {
        camera_model.delete_psf();
    }
    camera_model.set_harvey_shack_scatter(
        static_cast<float>(SCATTERED), static_cast<float>(SLOPE), static_cast<float>(SHOULDER));
    camera_model.set_sensor_read_noise(10.f);
    return camera_model;
}

/// The camera with the measured PSF alone, for its part of the light, as psf_image() gives it.
CameraModelHandle<RGB> core_camera(Scene<RGB>& scene, bool depth_of_field)
{
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, units::Meter(PITCH));
    camera_model.set_focal_length(units::Meter(FOCAL));
    camera_model.set_fstop(static_cast<float>(FSTOP));
    camera_model.set_focus_distance(units::Meter(FOCUS));
    camera_model.enable_depth_of_field(depth_of_field);
    camera_model.set_measured_psf(
        measurement(), static_cast<float>(PER_PIXEL), PSFSampling::PointSampled, RADIUS);
    return camera_model;
}

/// Tables of the scattered light alone, blurred by a disc, for its part of the light: the
/// aperture's tables with all but a part in 10^9 of the light scattered.
PsfTables<RGB> scatter_tables(double blur)
{
    return PsfTables<RGB>::airy(FSTOP,
                                PITCH,
                                PITCH,
                                4096.0,
                                PsfTables<RGB>::Scatter{1.0 - 1e-9, SLOPE, SHOULDER, std::nullopt},
                                blur);
}

/// Renders one bright still unresolved source at a range at (x, y) on the sensor.
Image<RGB>
render_source(bool with_psf, double range, double x, double y, bool depth_of_field = true)
{
    Scene<RGB> scene;
    auto camera_model = make_camera(scene, with_psf);
    camera_model.enable_depth_of_field(depth_of_field);
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1e12)));
    const double focal_pixels = FOCAL / PITCH;
    const double axis = SIZE / 2.0;
    source.set_position(units::Meter((x - axis) / focal_pixels * range),
                        units::Meter((y - axis) / focal_pixels * range),
                        units::Meter(range));
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(1.0)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::TRUE_STATE);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

/// The most a pixel's green channel differs between two images, each scaled to its sum, over
/// the brightest pixel of the second, and the same over the pixels at least far from (x, y), over
/// the brightest of those.
struct Difference {
    double worst = 0.0;
    double worst_far = 0.0;
};

Difference
difference(const Image<RGB>& image, const Image<RGB>& expected, double x, double y, double far)
{
    double image_sum = 0.0;
    double expected_sum = 0.0;
    double peak = 0.0;
    double far_peak = 0.0;
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            const double value = static_cast<double>(expected(i, j)[1]);
            image_sum += static_cast<double>(image(i, j)[1]);
            expected_sum += value;
            peak = std::max(peak, value);
            if (std::hypot(i + 0.5 - x, j + 0.5 - y) >= far) {
                far_peak = std::max(far_peak, value);
            }
        }
    }
    Difference result;
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            const double error = std::abs(static_cast<double>(image(i, j)[1]) / image_sum -
                                          static_cast<double>(expected(i, j)[1]) / expected_sum);
            result.worst = std::max(result.worst, error / (peak / expected_sum));
            if (std::hypot(i + 0.5 - x, j + 0.5 - y) >= far) {
                result.worst_far = std::max(result.worst_far, error / (far_peak / expected_sum));
            }
        }
    }
    return result;
}

} // namespace

TEST_CASE("A still source with a set PSF has its scattered light across the frame",
          "[render][unresolved][psf][measured][scatter]")
{
    // In focus (an object at the focus), the PSF takes nine tenths of the light and the scattered
    // light the rest, out to the frame's edges. Before, the scattered light was cut at the
    // convolution radius, which defaults to the PSF's, 6 px.
    const double x = 30.27;
    const double y = 33.61;
    const Image<RGB> image = render_source(true, FOCUS, x, y);
    Image<RGB> expected(SIZE, SIZE, RGB{0.f});
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            const double dx = i + 0.5 - x;
            const double dy = j + 0.5 - y;
            const double value = (1.0 - SCATTERED) * gaussian_between(dx - 0.5, dx + 0.5) *
                                     gaussian_between(dy - 0.5, dy + 0.5) +
                                 SCATTERED * scattered_pixel(dx, dy);
            expected(i, j) = RGB{static_cast<float>(value)};
        }
    }
    const Difference result = difference(image, expected, x, y, 10.0);
    CHECK(result.worst < 2e-4);
    CHECK(result.worst_far < 2e-3);
}

TEST_CASE("A set PSF's scattered light is blurred by the defocus at the source's depth",
          "[render][unresolved][psf][measured][scatter][defocus]")
{
    // At 20 m, blurred 1.5625 px: both parts, against the PSF blurred as psf_image() gives it
    // without scattered light, and the scattered light's tables blurred the same. Before, the
    // scattered light was not blurred, and cut at 6 px.
    const double x = 30.27;
    const double y = 33.61;
    const double range = 20.0;
    const Image<RGB> image = render_source(true, range, x, y);

    Scene<RGB> scene;
    auto core_only = core_camera(scene, true);
    const PsfTables<RGB> scatter = scatter_tables(blur_at(range));

    const int bx = static_cast<int>(std::floor(x));
    const int by = static_cast<int>(std::floor(y));
    const Image<RGB> core = core_only.psf_image(SIZE,
                                                static_cast<float>(x - bx - 0.5),
                                                static_cast<float>(y - by - 0.5),
                                                units::Meter(range));
    Image<RGB> expected(SIZE, SIZE, RGB{0.f});
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            const double value =
                (1.0 - SCATTERED) * static_cast<double>(core(i - bx + SIZE, j - by + SIZE)[1]) +
                SCATTERED * static_cast<double>(scatter.pixel(i + 0.5 - x, j + 0.5 - y)[1]);
            expected(i, j) = RGB{static_cast<float>(value)};
        }
    }
    const Difference result = difference(image, expected, x, y, 10.0);
    CHECK(result.worst < 2e-4);
    CHECK(result.worst_far < 2e-3);
}

TEST_CASE("Without a PSF, scattered light needs no convolution radius",
          "[render][unresolved][psf][scatter]")
{
    // The source's pixel takes nine tenths of its light, and the scattered light the rest, in
    // focus. This used to throw unless set_psf_convolution_radius() was called.
    const double x = 30.27;
    const double y = 33.61;
    const Image<RGB> image = render_source(false, FOCUS, x, y, false);
    Image<RGB> expected(SIZE, SIZE, RGB{0.f});
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            double value = SCATTERED * scattered_pixel(i + 0.5 - x, j + 0.5 - y);
            if (i == static_cast<int>(std::floor(x)) && j == static_cast<int>(std::floor(y))) {
                value += 1.0 - SCATTERED;
            }
            expected(i, j) = RGB{static_cast<float>(value)};
        }
    }
    const Difference result = difference(image, expected, x, y, 10.0);
    CHECK(result.worst < 2e-4);
    CHECK(result.worst_far < 2e-3);
}

TEST_CASE("Resolved bodies are convolved with the whole of a set PSF and its scattered light",
          "[render][psf][measured][scatter][bodies]")
{
    // A sunlit ball 10 px in radius, in focus, each of its pixels spread as a source at a pixel's
    // center: the measured PSF, as psf_image() gives it without scattered light, and the
    // scattered light from its tables, out to the frame's edges. Before, bodies were convolved
    // with a kernel cut at 6 px, the PSF's radius.
    auto render = [](bool convolve) {
        Scene<RGB> scene;
        auto camera_model = make_camera(scene, true);
        camera_model.enable_depth_of_field(false);
        camera_model.enable_psf_convolution(convolve);
        auto camera = scene.root.new_instance(camera_model);
        auto geometry = scene.add_ellipsoid(2_cm, 2_cm, 2_cm);
        auto ball = scene.root.new_instance(scene.add_primitive(geometry));
        ball.set_position(0_m, 0_m, 10_m);
        auto sun = scene.root.new_instance(scene.new_sun_light());
        sun.set_position(0_m, 0_m, -1_au);
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        renderer.set_samples_per_pixel(2);
        Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);
        return Image<RGB>(frame_buffer.received_power());
    };
    const Image<RGB> raw = render(false);
    const Image<RGB> convolved = render(true);

    // The light a pixel's center puts at every offset across the frame:
    Scene<RGB> scene;
    const Image<RGB> core = core_camera(scene, false).psf_image(SIZE, 0.f, 0.f);
    const Image<RGB> scattered = scatter_tables(0.0).image(SIZE, 0.0, 0.0);
    Image<RGB> spread(core.width(), core.height(), RGB{0.f});
    for (std::size_t i = 0; i < spread.size(); ++i) {
        spread[i] = core[i] * static_cast<float>(1.0 - SCATTERED) +
                    scattered[i] * static_cast<float>(SCATTERED);
    }
    double worst = 0.0;
    double peak = 0.0;
    double edge_expected = 0.0;
    double edge_got = 0.0;
    for (int ty = 0; ty < SIZE; ++ty) {
        for (int tx = 0; tx < SIZE; ++tx) {
            double expected = 0.0;
            for (int y = 0; y < SIZE; ++y) {
                for (int x = 0; x < SIZE; ++x) {
                    const double value = static_cast<double>(raw(x, y)[1]);
                    if (value != 0.0) {
                        expected +=
                            value * static_cast<double>(spread(tx - x + SIZE, ty - y + SIZE)[1]);
                    }
                }
            }
            peak = std::max(peak, expected);
            worst = std::max(worst, std::abs(static_cast<double>(convolved(tx, ty)[1]) - expected));
            if (tx == 0 && ty == 0) {
                edge_expected = expected;
                edge_got = static_cast<double>(convolved(tx, ty)[1]);
            }
        }
    }
    REQUIRE(peak > 0.0);
    CHECK(worst < 1e-5 * peak);
    // The frame's corner, 30 px from the ball's edge, gets scattered light:
    CHECK(edge_expected > 1e-6 * peak);
    CHECK(std::abs(edge_got - edge_expected) < 1e-3 * edge_expected);
}
