#include <array>
#include <cmath>
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
void wide_field(CameraModel<RGB>& camera, double k1)
{
    camera.configure_sensor_from_pitch(Resolution{64, 48}, units::Micrometer(100.0));
    camera.set_focal_length(units::Millimeter(4.0));
    camera.set_brown_conrady_distortion(BrownCoefficients(k1, 0.0, 0.0, 0.0, 0.0));
}

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
