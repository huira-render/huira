#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/psfs/grid_psf.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A 64 x 64 sensor of 10 um pixels behind a 50 mm lens at f/4, whose pixel edges are at
/// integers, so that sensor coordinates and pixel coordinates agree. Focused at 10 m, a point at
/// 20 m is blurred by a disc of 1.5625 px, and a star by one of 3.125 px.
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

/// The measured PSF: an elliptical Gaussian, sigma 0.7 px across and 0.5 px down, so that a
/// streak's direction matters, point-sampled four times a pixel out to RADIUS pixels.
constexpr double SIGMA_X = 0.7;
constexpr double SIGMA_Y = 0.5;
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
            data(i, j) = RGB{static_cast<float>(
                std::exp(-0.5 * (x * x / (SIGMA_X * SIGMA_X) + y * y / (SIGMA_Y * SIGMA_Y))))};
        }
    }
    return data;
}

/// The Gaussian's light between a and b along an axis, for a source at c.
double gaussian_between(double c, double a, double b, double sigma)
{
    const double scale = 1.0 / (std::sqrt(2.0) * sigma);
    return 0.5 * (std::erf((b - c) * scale) - std::erf((a - c) * scale));
}

/// The light in pixel (i, j) from a source at (sx, sy): the Gaussian's, or without a PSF, the
/// disc's of radius blur.
double gaussian_pixel(int i, int j, double sx, double sy)
{
    return gaussian_between(sx, i, i + 1, SIGMA_X) * gaussian_between(sy, j, j + 1, SIGMA_Y);
}

/// The area of [x0, x1] x [y0, y1] within the circle of radius r about the origin: the width
/// within the circle integrated over the heights by 16-point Gauss-Legendre quadrature on each
/// piece between the heights where an edge meets the circle, in the angle whose sine is the
/// height over r, which takes away the chord's square-root ends.
double disc_area(double x0, double x1, double y0, double y1, double r)
{
    constexpr std::array<double, 8> NODES{0.0950125098376374,
                                          0.2816035507792589,
                                          0.4580167776572274,
                                          0.6178762444026438,
                                          0.7554044083550030,
                                          0.8656312023878318,
                                          0.9445750230732326,
                                          0.9894009349916499};
    constexpr std::array<double, 8> WEIGHTS{0.1894506104550685,
                                            0.1826034150449236,
                                            0.1691565193950025,
                                            0.1495959888165767,
                                            0.1246289712555339,
                                            0.0951585116824928,
                                            0.0622535239386479,
                                            0.0271524594117541};
    std::vector<double> cuts{std::max(y0, -r), std::min(y1, r)};
    for (const double x : {x0, x1}) {
        if (std::abs(x) < r) {
            cuts.push_back(std::sqrt(r * r - x * x));
            cuts.push_back(-std::sqrt(r * r - x * x));
        }
    }
    std::sort(cuts.begin(), cuts.end());
    double area = 0.0;
    for (std::size_t k = 0; k + 1 < cuts.size(); ++k) {
        const double a = std::max(cuts[k], std::max(y0, -r));
        const double b = std::min(cuts[k + 1], std::min(y1, r));
        if (!(b > a)) {
            continue;
        }
        const double t0 = std::asin(a / r);
        const double t1 = std::asin(b / r);
        for (std::size_t n = 0; n < NODES.size(); ++n) {
            for (const double sign : {-1.0, 1.0}) {
                const double t = 0.5 * (t0 + t1) + sign * 0.5 * (t1 - t0) * NODES[n];
                const double half = r * std::cos(t);
                const double width = std::max(0.0, std::min(x1, half) - std::max(x0, -half));
                area += 0.5 * (t1 - t0) * WEIGHTS[n] * width * r * std::cos(t);
            }
        }
    }
    return area;
}

double disc_pixel(int i, int j, double sx, double sy, double blur)
{
    return disc_area(i - sx, i + 1 - sx, j - sy, j + 1 - sy, blur) / (PI<double>() * blur * blur);
}

/// A source moving evenly from (x, y) by (dx, dy) over the exposure, on the frame: each pixel's
/// light from a still source, averaged along the path by 8-point Gauss-Legendre quadrature on 64
/// panels, for the pixels within reach of it.
Image<RGB> expected_on_frame(double x,
                             double y,
                             double dx,
                             double dy,
                             double reach,
                             const std::function<double(int, int, double, double)>& pixel)
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
    const int panels = dx == 0.0 && dy == 0.0 ? 1 : 64;
    Image<RGB> image(SIZE, SIZE, RGB{0.f});
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            const double cx = i + 0.5 - x;
            const double cy = j + 0.5 - y;
            const double length2 = dx * dx + dy * dy;
            const double t =
                length2 > 0.0 ? std::clamp((cx * dx + cy * dy) / length2, 0.0, 1.0) : 0.0;
            if (std::hypot(cx - t * dx, cy - t * dy) > reach) {
                continue;
            }
            double value = 0.0;
            for (int k = 0; k < panels; ++k) {
                for (std::size_t n = 0; n < NODES.size(); ++n) {
                    const double s = (k + 0.5 + 0.5 * NODES[n]) / panels;
                    value += 0.5 * WEIGHTS[n] / panels * pixel(i, j, x + s * dx, y + s * dy);
                }
            }
            image(i, j) = RGB{static_cast<float>(value)};
        }
    }
    return image;
}

/// Renders unresolved sources at a range, each moving from its place on the sensor by (dx, dy)
/// over the one-second exposure, with the measured PSF or none.
Image<RGB> render_sources(bool with_psf,
                          bool depth_of_field,
                          double range,
                          const std::vector<std::array<double, 2>>& places,
                          double dx,
                          double dy,
                          double watts = 1e12)
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.set_pixel_convention(PixelConvention::colmap());
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, units::Meter(PITCH));
    camera_model.set_focal_length(units::Meter(FOCAL));
    camera_model.set_fstop(static_cast<float>(FSTOP));
    camera_model.set_focus_distance(units::Meter(FOCUS));
    camera_model.enable_depth_of_field(depth_of_field);
    if (with_psf) {
        camera_model.set_measured_psf(
            measurement(), static_cast<float>(PER_PIXEL), PSFSampling::PointSampled);
    } else {
        camera_model.enable_diffraction(false);
    }
    camera_model.set_sensor_read_noise(10.f);
    auto camera = scene.root.new_instance(camera_model);
    const double focal_pixels = FOCAL / PITCH;
    const double axis = SIZE / 2.0;
    for (const auto& [x, y] : places) {
        auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(watts)));
        source.set_position(units::Meter((x - axis) / focal_pixels * range),
                            units::Meter((y - axis) / focal_pixels * range),
                            units::Meter(range));
        source.set_velocity(units::MetersPerSecond(dx / focal_pixels * range),
                            units::MetersPerSecond(dy / focal_pixels * range),
                            units::MetersPerSecond(0.0));
    }
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(1.0)};
    // Where the source is, not where its light left it.
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::TRUE_STATE, 3);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

Image<RGB> render_source(bool with_psf,
                         bool depth_of_field,
                         double range,
                         double x,
                         double y,
                         double dx,
                         double dy,
                         double watts = 1e12)
{
    return render_sources(with_psf, depth_of_field, range, {{x, y}}, dx, dy, watts);
}

double total(const Image<RGB>& image)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        sum += static_cast<double>(image[i][1]);
    }
    return sum;
}

/// The most a pixel's green channel differs between two images, each scaled to its sum, over
/// the brightest pixel of the second.
double worst_difference(const Image<RGB>& image, const Image<RGB>& expected)
{
    const double image_sum = total(image);
    const double expected_sum = total(expected);
    double peak = 0.0;
    double worst = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        const double a = static_cast<double>(image[i][1]) / image_sum;
        const double b = static_cast<double>(expected[i][1]) / expected_sum;
        peak = std::max(peak, b);
        worst = std::max(worst, std::abs(a - b));
    }
    return worst / peak;
}

} // namespace

TEST_CASE("A moving source with a measured PSF is its path's line integral, in any direction",
          "[render][unresolved][psf][measured][streak]")
{
    // In focus. Before, it was stamped every 0.75 px or less along its path, from 16 subpixel
    // positions per axis.
    const auto gaussian = [](int i, int j, double sx, double sy) {
        return gaussian_pixel(i, j, sx, sy);
    };
    struct Path {
        double length;
        double angle;
    };
    for (const Path& path : {Path{9.7, 0.3}, Path{4.3, 2.1}, Path{0.4, -1.2}}) {
        INFO(path.length << " px at " << path.angle << " rad");
        const double x = 27.31;
        const double y = 30.77;
        const double dx = path.length * std::cos(path.angle);
        const double dy = path.length * std::sin(path.angle);
        const Image<RGB> image = render_source(true, false, FOCUS, x, y, dx, dy);
        CHECK(worst_difference(image, expected_on_frame(x, y, dx, dy, 8.0, gaussian)) < 3e-4);
    }
}

TEST_CASE("Without a PSF, an object is blurred by the defocus at its own depth, still or moving",
          "[render][unresolved][defocus][streak]")
{
    // At 20 m, a disc of 1.5625 px, where a star's is 3.125 px. Before, every source got the
    // star's, stamped from 16 subpixel positions per axis.
    const double blur = blur_at(20.0);
    const auto disc = [blur](int i, int j, double sx, double sy) {
        return disc_pixel(i, j, sx, sy, blur);
    };
    const double x = 27.31;
    const double y = 30.77;
    for (const double length : {0.0, 7.3}) {
        INFO((length > 0.0 ? "moving" : "still"));
        const double dx = length * std::cos(0.8);
        const double dy = length * std::sin(0.8);
        const Image<RGB> image = render_source(false, true, 20.0, x, y, dx, dy);
        CHECK(worst_difference(image, expected_on_frame(x, y, dx, dy, blur + 2.0, disc)) < 5e-4);
    }
}

TEST_CASE("A moving source with a set PSF keeps all of its light, however faint",
          "[render][unresolved][psf][measured][streak]")
{
    // A faint source is drawn only as far as its light shows, and the rest is spread over the
    // frame; a bright one, over the whole frame. Both put the same share of their light on it.
    // (A guard: stamps, cropped for faint sources, kept their light too.)
    const double x = 27.31;
    const double y = 30.77;
    const double faint = total(render_source(true, false, FOCUS, x, y, 6.1, 2.3, 1e-6)) / 1e-6;
    const double bright = total(render_source(true, false, FOCUS, x, y, 6.1, 2.3, 1e6)) / 1e6;
    CHECK(std::abs(faint / bright - 1.0) < 1e-5);
}

TEST_CASE("Many sources moving the same way with a measured PSF are drawn as exactly",
          "[render][unresolved][psf][measured][streak]")
{
    // Enough share the direction to draw them from tables made for it, where one alone is drawn
    // without (see the first test case); both are the line integral. Before, each was stamped
    // every 0.75 px or less along its path.
    const auto gaussian = [](int i, int j, double sx, double sy) {
        return gaussian_pixel(i, j, sx, sy);
    };
    std::vector<std::array<double, 2>> places;
    for (int k = 0; k < 96; ++k) {
        places.push_back({6.13 + 4.37 * (k % 12), 7.71 + 6.29 * (k / 12)});
    }
    const double dx = 4.3 * std::cos(0.3);
    const double dy = 4.3 * std::sin(0.3);
    const Image<RGB> image = render_sources(true, false, FOCUS, places, dx, dy);
    Image<RGB> expected(SIZE, SIZE, RGB{0.f});
    for (const auto& [x, y] : places) {
        const Image<RGB> one = expected_on_frame(x, y, dx, dy, 8.0, gaussian);
        for (std::size_t i = 0; i < expected.size(); ++i) {
            expected[i] += one[i];
        }
    }
    CHECK(worst_difference(image, expected) < 3e-4);
}

TEST_CASE("A faint line's wings are interpolated only as far as the tolerance allows",
          "[render][unresolved][psf][measured][streak]")
{
    // A line from a measured PSF with wings falling as r^-3 out to 16 px, drawn with and without
    // leave to interpolate its faint wings, from its tables and directly: with leave, it is off
    // by no more than the tolerance anywhere, and is interpolated somewhere. Before, nothing was
    // interpolated.
    constexpr int WINGS = 16;
    constexpr int PER = 2;
    const int side = 2 * PER * WINGS + 1;
    Image<RGB> data(side, side);
    for (int j = 0; j < side; ++j) {
        for (int i = 0; i < side; ++i) {
            const double x = static_cast<double>(i - PER * WINGS) / PER;
            const double y = static_cast<double>(j - PER * WINGS) / PER;
            data(i, j) = RGB{static_cast<float>(std::pow(1.0 + (x * x + y * y) / 0.64, -1.5))};
        }
    }
    MeasuredPSF<RGB> measured(data, static_cast<float>(PER), PSFSampling::PointSampled);
    const GridPsf<RGB> grid(measured, 1.0);
    constexpr int SIDE = 72;
    const std::array<double, 2> start{24.31, 30.77};
    const std::array<double, 2> end{start[0] + 17.3 * std::cos(0.37),
                                    start[1] + 17.3 * std::sin(0.37)};
    const auto everywhere = [](double) { return 1.0; };
    // A source of 300 electrons in each channel, for a tolerance of 5e-3 electrons:
    constexpr double ELECTRONS = 300.0;
    GridPsf<RGB>::LineTolerance tolerance;
    tolerance.electrons.fill(ELECTRONS);
    tolerance.electrons_off = 5e-3;
    tolerance.per_channel = true;
    for (const bool tables : {true, false}) {
        INFO((tables ? "from tables" : "directly"));
        Image<RGB> exact(SIDE, SIDE, RGB{0.f});
        Image<RGB> interpolated(SIDE, SIDE, RGB{0.f});
        if (tables) {
            static_cast<void>(
                grid.draw_line(exact, 0, SIDE, 0, SIDE, start, end, RGB{1.f}, 1e9, everywhere));
            static_cast<void>(grid.draw_line(
                interpolated, 0, SIDE, 0, SIDE, start, end, RGB{1.f}, 1e9, everywhere, tolerance));
        } else {
            static_cast<void>(grid.draw_line_direct(
                exact, 0, SIDE, 0, SIDE, start, end, RGB{1.f}, 1e9, everywhere));
            static_cast<void>(grid.draw_line_direct(
                interpolated, 0, SIDE, 0, SIDE, start, end, RGB{1.f}, 1e9, everywhere, tolerance));
        }
        double worst = 0.0;
        std::size_t different = 0;
        for (std::size_t i = 0; i < exact.size(); ++i) {
            for (std::size_t c = 0; c < 3; ++c) {
                const double off = ELECTRONS * std::abs(static_cast<double>(interpolated[i][c]) -
                                                        static_cast<double>(exact[i][c]));
                worst = std::max(worst, off);
                different += off > 1e-6 * tolerance.electrons_off ? 1 : 0;
            }
        }
        INFO("worst " << worst << " electrons, " << different << " values differ");
        CHECK(worst <= tolerance.electrons_off);
        CHECK(different > 300);
    }
}
