#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

bool is_nan(const Pixel& p)
{
    return std::isnan(p.x) && std::isnan(p.y);
}

double distance(const Pixel& a, const Pixel& b)
{
    return std::hypot(static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y));
}

/// A wide field: 64 x 48 pixels of 100 um behind a 4 mm lens, so that the image's corners are at
/// a normalized radius of 1 (0.8, 0.6), where strong distortion has an effect.
void wide_field(CameraModel<RGB>& camera, const BrownCoefficients& coefficients)
{
    camera.configure_sensor_from_pitch(Resolution{64, 48}, units::Micrometer(100.0));
    camera.set_focal_length(units::Millimeter(4.0));
    camera.set_brown_conrady_distortion(coefficients);
    camera.delete_psf(); // the aperture's, by default: not needed here, and slow to build
}

void wide_field(CameraModel<RGB>& camera, double k1)
{
    wide_field(camera, BrownCoefficients(k1, 0.0, 0.0, 0.0, 0.0));
}

/// The worst distance, over a grid of the ideal rectangle [-hx, hx] x [-hy, hy], between a point
/// and the undistortion of its distortion; infinite if any has no inverse.
double worst_round_trip(const Distortion<RGB>& model, float hx, float hy)
{
    double worst = 0.0;
    constexpr int n = 8;
    for (int j = -n; j <= n; ++j) {
        for (int i = -n; i <= n; ++i) {
            const Pixel p{hx * static_cast<float>(i) / n, hy * static_cast<float>(j) / n};
            const Pixel back = model.undistort(model.distort(p));
            if (is_nan(back)) {
                return std::numeric_limits<double>::infinity();
            }
            worst = std::max(worst, distance(back, p));
        }
    }
    return worst;
}

double total(const Image<RGB>& image)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        sum += static_cast<double>(image[i][0]);
    }
    return sum;
}

/// A rational model of a wide-angle lens, 21% barrel at the corners of the wide field above
/// (where it undistorts to a normalized radius of 1.27), whose radial factor has a pole at
/// 1.4037. The camera's frustum, which bounds the image in tangent space, reaches 1.44 at its
/// corners, past it.
const OpenCVCoefficients WIDE_RATIONAL(-0.29, 0.1, -0.08, 0.27, -0.04, -0.18, 0.0, 0.0, 0.0, 0.0,
                                       0.0, 0.0);

/// Calls f and returns the message it throws, or "" if it does not.
template <typename F>
std::string thrown_message(F&& f)
{
    try {
        f();
    } catch (const std::exception& e) {
        return e.what();
    }
    return "";
}

} // namespace

TEST_CASE("Undistortion inverts strong distortion that has an inverse", "[cameras][distortion]")
{
    // Strong pincushion distortion has an inverse everywhere, but undistort() used to be a
    // fixed-point iteration that only converges while the distortion is a contraction: at a
    // normalized radius of 1 with k1 = 0.5 it stopped about 0.04 (15 px at a 400 px focal
    // length) short.
    const BrownDistortion<RGB> brown(BrownCoefficients(0.5, 0.0, 0.0, 0.01, -0.01));
    const OpenCVDistortion<RGB> opencv(
        OpenCVCoefficients(0.5, 0.0, 0.0, 0.05, 0.0, 0.0, 0.01, -0.01, 0.0, 0.0, 0.0, 0.0));
    const OwenDistortion<RGB> owen(OwenCoefficients(0.01, 0.5, 0.0, 0.0, 0.0, 0.0));
    const std::array<const Distortion<RGB>*, 3> models{&brown, &opencv, &owen};

    for (const Distortion<RGB>* model : models) {
        for (float y = -0.6f; y <= 0.61f; y += 0.2f) {
            for (float x = -0.8f; x <= 0.81f; x += 0.2f) {
                const Pixel p{x, y};
                const Pixel distorted = model->distort(p);
                const Pixel back = model->undistort(distorted);
                INFO(model->get_type_name() << " at (" << x << ", " << y << ")");
                CHECK(distance(back, p) < 1e-6);
            }
        }
    }
}

TEST_CASE("Undistortion inverts pincushion distortion that folds just beyond the image",
          "[cameras][distortion]")
{
    // The model r (1 + 0.35 r^2 + 0.2 r^4 - 0.5 r^6) folds over at r = 0.9665, beyond the image,
    // whose corner, distorted to (0.8, 0.6), is at r = 0.857. There the lens is well behaved (17%
    // pincushion, Jacobian determinant 1.08), but pincushion moves the corner out past the fold.
    // undistort() used to start from the distorted point, so it started folded over, found no
    // step that brought it closer, and returned NaN.
    const BrownDistortion<RGB> brown(BrownCoefficients(0.35, 0.2, -0.5, 0.0, 0.0));
    const Pixel corner = brown.undistort(Pixel{0.8f, 0.6f});
    REQUIRE_FALSE(is_nan(corner));
    CHECK(std::hypot(corner.x, corner.y) < 0.9665f);
    CHECK(distance(brown.distort(corner), Pixel{0.8f, 0.6f}) < 1e-6);
    CHECK(worst_round_trip(brown, corner.x, corner.y) < 1e-6);

    // The same happens with milder distortion: this rational model is within 10% of a pinhole
    // over the image, with a Jacobian determinant of at least 0.83, and its corners had no
    // inverse.
    const OpenCVDistortion<RGB> opencv(OpenCVCoefficients(
        -0.46, -3.1, -18.5, -0.97, -3.26, -11.8, -0.0013, -0.0009, 0.0, 0.0, 0.0, 0.0));
    CHECK(worst_round_trip(opencv, 0.4f, 0.3f) < 1e-6);
}

TEST_CASE("Undistortion finds the point in the image, not another that distorts to the same place",
          "[cameras][distortion]")
{
    // r (1 + 0.58 r^2 - 0.38 r^4 + 0.03 r^6) folds over at r = 1.23, beyond the image's corners at
    // 1, and beyond r = 1.77 its radial factor is negative, so it maps points through the
    // center: (1.558, 1.169), on the opposite side, distorts to the same place as the image's
    // corner (-0.8, -0.6). undistort() used to step across the fold to it, and returned it.
    const BrownDistortion<RGB> brown(BrownCoefficients(0.58, -0.38, 0.03, 0.0, 0.0));
    const Pixel corner{-0.8f, -0.6f};
    const Pixel back = brown.undistort(brown.distort(corner));
    INFO("(" << back.x << ", " << back.y << ")");
    CHECK(distance(back, corner) < 1e-6);
    CHECK(worst_round_trip(brown, 0.8f, 0.6f) < 1e-6);
}

TEST_CASE("Undistortion has no answer beyond the reach of a distortion", "[cameras][distortion]")
{
    // With k1 = -0.3 the distortion r (1 - 0.3 r^2) reaches no further than 0.703, at r = 1.054,
    // and folds back beyond.
    const BrownDistortion<RGB> barrel(BrownCoefficients(-0.3, 0.0, 0.0, 0.0, 0.0));

    CHECK(is_nan(barrel.undistort(Pixel{0.9f, 0.f})));
    CHECK(is_nan(barrel.undistort(Pixel{0.5f, -0.6f})));

    // Within reach every point has two preimages, and the one inside the fold is the image's:
    const Pixel inside = barrel.undistort(Pixel{0.6f, 0.f});
    REQUIRE_FALSE(is_nan(inside));
    CHECK(inside.x < 1.054f);
    CHECK(distance(barrel.distort(inside), Pixel{0.6f, 0.f}) < 1e-6);
}

TEST_CASE("The OpenCV model images nothing past the pole of its radial factor",
          "[cameras][distortion]")
{
    const OpenCVDistortion<RGB> opencv(WIDE_RATIONAL);

    // Within the pole, as far as the image's corners:
    const Pixel corner = opencv.undistort(Pixel{-0.8f, -0.6f});
    REQUIRE_FALSE(is_nan(corner));
    CHECK(std::hypot(corner.x, corner.y) < 1.4037f);
    CHECK(distance(opencv.distort(corner), Pixel{-0.8f, -0.6f}) < 1e-6);

    // Past it the radial factor is negative, and distort() used to map this point, outside the
    // image's corner, to (0.598, 0.453), inside the image on the opposite side:
    CHECK(is_nan(opencv.distort(Pixel{-1.148f, -0.870f})));
    CHECK(is_nan(opencv.distort(Pixel{3.f, 0.f})));

    // Nor does undistort() return a point past the pole. This model of a narrower field, 29%
    // pincushion at the corner (0.16, 0.12), has its pole at r = 0.2223, and past it maps
    // (0.2368, 0.1776) to the same place as the corner. undistort() converged on that.
    const OpenCVDistortion<RGB> narrow(
        OpenCVCoefficients(-3.86, -90.5, -3670, -4.95, -130, -3630, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    CHECK(worst_round_trip(narrow, 0.16f, 0.12f) < 1e-6);
}

TEST_CASE("A camera whose distortion has no inverse in the image reports it when used",
          "[cameras][distortion]")
{
    // The corners are at a normalized radius of 1, beyond the 0.703 that k1 = -0.3 reaches: no ray
    // lands there. Before, the camera's rays and pixel radiometry were silently NaN in the
    // corners.
    CameraModel<RGB> camera;
    REQUIRE_NOTHROW(wide_field(camera, -0.3)); // settings can come in any order

    const std::string precompute = thrown_message([&] { camera.precompute(); });
    INFO(precompute);
    CHECK(precompute.find("has no inverse") != std::string::npos);
    CHECK(precompute.find("(-0.5, -0.5)") != std::string::npos); // the first pixel's corner

    CHECK_THROWS(camera.cast_ray(Pixel{0.f, 0.f}));
    CHECK_THROWS(camera.pixel_radiance_to_power(0, 0));
    CHECK_NOTHROW(camera.cast_ray(Pixel{31.5f, 23.5f})); // the center is fine

    // A longer focal length brings the corners within reach (a normalized radius of 0.5), and
    // the camera is usable again:
    camera.set_focal_length(units::Millimeter(8.0));
    CHECK_NOTHROW(camera.precompute());
    const Ray<RGB> corner = camera.cast_ray(Pixel{0.f, 0.f});
    CHECK(std::isfinite(corner.direction().x));
    CHECK(std::isfinite(camera.pixel_radiance_to_power(0, 0)));
}

TEST_CASE("A camera whose pincushion distortion folds just beyond the image is usable",
          "[cameras][distortion]")
{
    // The corners, at a normalized radius of 1, undistort to 0.857, inside the fold at 0.9665
    // (see above). precompute() used to throw that the distortion had no inverse there.
    CameraModel<RGB> camera;
    wide_field(camera, BrownCoefficients(0.35, 0.2, -0.5, 0.0, 0.0));
    REQUIRE_NOTHROW(camera.precompute());

    for (const Pixel& p : {Pixel{-0.5f, -0.5f}, Pixel{63.5f, 47.5f}, Pixel{-0.5f, 47.5f}}) {
        const Vec3<float> point = camera.cast_ray(p).direction() * 10.f;
        const Pixel back = camera.project_point(point);
        INFO("pixel (" << p.x << ", " << p.y << ") projects back to (" << back.x << ", " << back.y
                       << ")");
        CHECK(distance(back, p) < 1e-3);
    }
    CHECK(std::isfinite(camera.pixel_radiance_to_power(0, 0)));
}

TEST_CASE("Rendering with a distortion that has no inverse in the image throws",
          "[cameras][distortion][render]")
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch(Resolution{64, 48}, units::Micrometer(100.0));
    camera_model.set_focal_length(units::Millimeter(4.0));
    camera_model.set_brown_conrady_distortion(BrownCoefficients(-0.3, 0.0, 0.0, 0.0, 0.0));
    auto camera = scene.root.new_instance(camera_model);

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);

    const std::string message = thrown_message([&] { renderer.render(scene_view, frame_buffer); });
    INFO(message);
    CHECK(message.find("has no inverse") != std::string::npos);
}

TEST_CASE("A source past an OpenCV model's pole does not appear in the image",
          "[cameras][distortion][render]")
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch(Resolution{64, 48}, units::Micrometer(100.0));
    camera_model.set_focal_length(units::Millimeter(4.0));
    camera_model.set_opencv_distortion(WIDE_RATIONAL);
    camera_model.delete_psf(); // each source's light in one pixel
    camera_model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));

    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    auto render_at = [&](double tan_x, double tan_y) {
        constexpr double RANGE = 1000.0;
        source.set_position(units::Meter(tan_x * RANGE),
                            units::Meter(tan_y * RANGE),
                            units::Meter(RANGE));
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);
        return total(frame_buffer.received_power());
    };

    // A source in the image lights it:
    CHECK(render_at(0.3, 0.2) > 0.0);

    // This one is outside the image, beyond its top-left corner, but inside the frustum, which
    // bounds the image in tangent space, and past the pole. It used to be projected to pixel
    // (55.4, 41.6), near the opposite corner, and drawn there:
    CHECK(render_at(-1.148, -0.870) == 0.0);
}

TEST_CASE("cast_ray() beyond the image throws where the distortion has no inverse",
          "[cameras][distortion]")
{
    // k1 = -0.1 reaches a normalized radius of 1.217, enough for the image's corners at 1:
    CameraModel<RGB> camera;
    wide_field(camera, -0.1);
    CHECK_NOTHROW(camera.precompute());

    // Just outside the image is within reach (a normalized radius of about 1.07)...
    const Ray<RGB> near_edge = camera.cast_ray(Pixel{-2.f, -2.f});
    CHECK(std::isfinite(near_edge.direction().x));

    // ...and far outside it is not (about 11.6). Before, this returned a NaN ray:
    CHECK_THROWS(camera.cast_ray(Pixel{-300.f, -300.f}));
}

TEST_CASE("Rays cast through a strongly distorted image project back where they started",
          "[cameras][distortion]")
{
    // Pincushion distortion this strong used to leave rays at the corners about 15 px off.
    CameraModel<RGB> camera;
    wide_field(camera, 0.5);
    CHECK_NOTHROW(camera.precompute());

    auto round_trip_error = [&](const Pixel& p) {
        const Vec3<float> point = camera.cast_ray(p).direction() * 10.f;
        const Pixel back = camera.project_point(point);
        INFO("pixel (" << p.x << ", " << p.y << ") projects back to (" << back.x << ", " << back.y
                       << ")");
        return distance(back, p);
    };

    // Inside the image, rays come from a table of directions at the pixel corners: exact at the
    // corners themselves, and interpolated in between, which with distortion this strong is good
    // to a few thousandths of a pixel. Beyond the image they are computed directly.
    for (const Pixel& p : {Pixel{-0.5f, -0.5f}, Pixel{-0.5f, 23.5f}, Pixel{31.5f, 47.5f}}) {
        CHECK(round_trip_error(p) < 1e-3);
    }
    for (const Pixel& p : {Pixel{0.f, 0.f}, Pixel{63.f, 47.f}, Pixel{10.25f, 3.75f}}) {
        CHECK(round_trip_error(p) < 1e-2);
    }
    CHECK(round_trip_error(Pixel{-3.f, -2.f}) < 1e-3);
}
