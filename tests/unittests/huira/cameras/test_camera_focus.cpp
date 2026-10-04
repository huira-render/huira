#include <array>
#include <cmath>
#include <limits>
#include <memory>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/sampling/sampler.hpp"
#include "huira/units/units.hpp"

using namespace huira;

namespace {

/// A wide-ish camera (about 21 degrees to the corner) with a large aperture, so that both the
/// depth-of-field geometry and any distortion are far above float noise.
std::unique_ptr<CameraModel<RGB>> make_camera(bool blender, bool distorted)
{
    auto camera = std::make_unique<CameraModel<RGB>>();
    if (blender) {
        camera->use_blender_convention();
    }
    // Sensor first: each setter recomputes per-pixel camera data at the current resolution,
    // and the default sensor is 1024x1024, which is slow in unoptimized builds.
    camera->configure_sensor_from_pitch(Resolution{64, 48}, units::Micrometer(200.0));
    camera->set_focal_length(units::Millimeter(20.0));
    camera->set_fstop(2.0f);
    if (distorted) {
        camera->set_brown_conrady_distortion(BrownCoefficients(-0.2, 0.05, 0.0, 0.0, 0.0));
    }
    camera->enable_depth_of_field();
    return camera;
}

/// Unit vector along the optical axis, in the direction the camera looks.
Vec3<double> forward_axis(bool blender)
{
    return blender ? Vec3<double>{0.0, 0.0, -1.0} : Vec3<double>{0.0, 0.0, 1.0};
}

/// Point where the (infinite) line carrying the ray crosses the plane perpendicular to the
/// optical axis at signed distance `distance` in front of the camera. A negative distance is
/// behind the camera, so for a past-infinity focus the crossing is behind the ray's origin.
Vec3<double> cross_focal_plane(const Ray<RGB>& ray, double distance, bool blender)
{
    const Vec3<double> axis = forward_axis(blender);
    const Vec3<double> origin{ray.origin()};
    const Vec3<double> direction{ray.direction()};
    const double t = (distance - glm::dot(origin, axis)) / glm::dot(direction, axis);
    return origin + t * direction;
}

/// Point of exact focus for a pixel: where its pinhole ray meets the focal plane.
Vec3<double> pinhole_focal_point(const CameraModel<RGB>& camera,
                                 const Pixel& pixel,
                                 double distance,
                                 bool blender)
{
    const Vec3<double> direction{camera.cast_ray(pixel).direction()};
    const double axial = glm::dot(direction, forward_axis(blender));
    return direction * (distance / axial);
}

// Aperture samples away from the centre, which maps to the pinhole ray itself.
const std::array<Vec2<float>, 4> APERTURE_SAMPLES{
    Vec2<float>{0.1f, 0.2f},
    Vec2<float>{0.9f, 0.5f},
    Vec2<float>{0.5f, 0.95f},
    Vec2<float>{0.3f, 0.7f},
};

const std::array<Pixel, 3> PIXELS{
    Pixel{32.f, 24.f},  // on axis
    Pixel{3.25f, 2.5f}, // near a corner, where distortion and obliquity are largest
    Pixel{60.7f, 45.1f},
};

/// Every depth-of-field ray must travel forward, and the line carrying it must pass through
/// the pixel's point of exact focus on the plane at `distance` (behind the camera when
/// negative, where the rays diverge from a virtual point).
void check_rays_meet_focal_point(const CameraModel<RGB>& camera, double distance, bool blender)
{
    const double tolerance = 1e-4 * std::abs(distance) + 1e-6;

    for (const Pixel& pixel : PIXELS) {
        const Vec3<double> expected = pinhole_focal_point(camera, pixel, distance, blender);

        for (const Vec2<float>& u : APERTURE_SAMPLES) {
            DeterministicSampler<float> sampler(0.5f, u);
            const Ray<RGB> ray = camera.cast_ray(pixel, sampler);

            INFO("pixel (" << pixel[0] << ", " << pixel[1] << "), aperture sample (" << u.x << ", "
                           << u.y << "), focus distance " << distance);

            REQUIRE(glm::dot(Vec3<double>{ray.direction()}, forward_axis(blender)) > 0.0);

            const Vec3<double> hit = cross_focal_plane(ray, distance, blender);
            REQUIRE(glm::length(hit - expected) < tolerance);
        }
    }
}

constexpr double INF = std::numeric_limits<double>::infinity();

/// Relative comparison; an infinite expectation must be matched exactly.
bool close(double actual, double expected, double relative = 1e-6)
{
    if (std::isinf(expected)) {
        return actual == expected;
    }
    return std::abs(actual - expected) <= relative * std::abs(expected) + 1e-15;
}

/// Thin lens: offset of the sensor from the infinity-focus plane that focuses at `distance`.
double thin_lens_offset(double focal_length, double distance)
{
    return std::isinf(distance) ? 0.0 : focal_length * focal_length / (distance - focal_length);
}

/// A camera for tests of the focus state itself, rather than of the rays. Stopped down with
/// large pixels, so that even the closest focus tested blurs by under 100 pixels: the defocus
/// kernel cache grows with the square of the blur.
std::unique_ptr<CameraModel<RGB>> make_focus_camera()
{
    auto camera = std::make_unique<CameraModel<RGB>>();
    camera->configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(50.0));
    camera->set_focal_length(units::Millimeter(50.0));
    camera->set_fstop(16.0f);
    return camera;
}

/// All three focus getters must describe a focus at `distance` (+inf for infinity).
void check_focus(const CameraModel<RGB>& camera, double distance)
{
    const double f = camera.focal_length().to_si();
    INFO("expected focus distance " << distance << " m, focal length " << f << " m");
    CHECK(close(camera.focus_distance().to_si(), distance));
    CHECK(close(camera.focus_diopters().to_si(), std::isinf(distance) ? 0.0 : 1.0 / distance));
    CHECK(close(camera.focus_sensor_offset().to_si(), thin_lens_offset(f, distance)));
}

/// Expected defocus blur radius in pixels of a point at infinity: the camera projects with a
/// fixed pinhole model, so the blur is |diopters| * f * aperture radius, over the pixel pitch.
double expected_blur_radius(const CameraModel<RGB>& camera, double pitch)
{
    const double f = camera.focal_length().to_si();
    const double aperture_radius = f / (2.0 * static_cast<double>(camera.fstop()));
    const double radius = std::abs(camera.focus_diopters().to_si()) * f * aperture_radius / pitch;
    return radius < 0.5 ? 0.0 : radius; // under half a pixel counts as in focus
}

} // namespace

TEST_CASE("Depth of field rays meet at the point of focus", "[cameras][focus]")
{
    for (bool blender : {false, true}) {
        for (bool distorted : {false, true}) {
            DYNAMIC_SECTION("blender " << blender << ", distorted " << distorted)
            {
                auto camera = make_camera(blender, distorted);

                // In front of the camera, then past infinity:
                for (double distance : {0.5, 10.0, -0.5, -10.0}) {
                    camera->set_focus_distance(units::Meter(distance));
                    check_rays_meet_focal_point(*camera, distance, blender);
                }

                // Past infinity, set through diopters:
                camera->set_focus_diopters(units::Diopter(-0.1));
                REQUIRE(std::abs(camera->focus_distance().to_si() + 10.0) < 1e-4);
                check_rays_meet_focal_point(*camera, -10.0, blender);
            }
        }
    }
}

TEST_CASE("Depth of field rays are parallel when focused at infinity", "[cameras][focus]")
{
    const float inf = std::numeric_limits<float>::infinity();

    for (bool blender : {false, true}) {
        DYNAMIC_SECTION("blender " << blender)
        {
            auto camera = make_camera(blender, true);

            for (float distance : {inf, -inf}) {
                INFO("focus distance " << distance);
                camera->set_focus_distance(units::Meter(distance));
                REQUIRE(camera->focus_diopters().to_si() == 0.0);

                for (const Pixel& pixel : PIXELS) {
                    const Vec3<float> pinhole = camera->cast_ray(pixel).direction();
                    for (const Vec2<float>& u : APERTURE_SAMPLES) {
                        DeterministicSampler<float> sampler(0.5f, u);
                        const Ray<RGB> ray = camera->cast_ray(pixel, sampler);

                        REQUIRE(glm::dot(ray.direction(), pinhole) > 1.f - 1e-6f);
                        REQUIRE(ray.origin().z == 0.f);
                        REQUIRE(glm::length(ray.origin()) > 0.f);
                    }
                }
            }
        }
    }
}

TEST_CASE("The focus setters and getters describe one focus state", "[cameras][focus]")
{
    auto camera = make_focus_camera();
    const double f = camera->focal_length().to_si();

    // In front of the camera (including closer than one focal length, which a physical lens
    // cannot focus on), at infinity, and past infinity:
    for (double distance : {0.02, 0.3, 10.0, 1e4, INF, -1e4, -10.0, -0.3}) {
        camera->set_focus_distance(units::Meter(distance));
        check_focus(*camera, distance);

        camera->set_focus_diopters(units::Diopter(std::isinf(distance) ? 0.0 : 1.0 / distance));
        check_focus(*camera, distance);

        camera->set_focus_sensor_offset(units::Meter(thin_lens_offset(f, distance)));
        check_focus(*camera, distance);
    }

    // Infinity of either sign is the same focus:
    camera->set_focus_distance(units::Meter(-INF));
    check_focus(*camera, INF);
}

TEST_CASE("Each focus getter returns its own setting exactly", "[cameras][focus]")
{
    auto camera = make_focus_camera();

    // Values chosen because their reciprocals do not round-trip in double precision:
    // 1 / (1 / 49.0) == 49.00000000000001.
    camera->set_focus_distance(units::Meter(49.0));
    CHECK(camera->focus_distance().to_si() == 49.0);
    camera->set_focus_distance(units::Meter(-0.9));
    CHECK(camera->focus_distance().to_si() == -0.9);

    camera->set_focus_diopters(units::Diopter(1.0 / 49.0));
    CHECK(camera->focus_diopters().to_si() == 1.0 / 49.0);

    camera->set_focus_sensor_offset(units::Micrometer(37.0));
    CHECK(camera->focus_sensor_offset().to_si() == units::Micrometer(37.0).to_si());

    camera->set_focus_distance(units::Meter(INF));
    CHECK(camera->focus_diopters().to_si() == 0.0);
    CHECK(camera->focus_sensor_offset().to_si() == 0.0);
}

TEST_CASE("Invalid focus settings are rejected and leave the focus unchanged", "[cameras][focus]")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    auto camera = make_focus_camera();
    camera->set_focus_distance(units::Meter(10.0));

    CHECK_THROWS(camera->set_focus_distance(units::Meter(nan)));
    CHECK_THROWS(camera->set_focus_distance(units::Meter(0.0)));
    CHECK_THROWS(camera->set_focus_distance(units::Meter(-1e-13)));
    CHECK_THROWS(camera->set_focus_diopters(units::Diopter(nan)));
    CHECK_THROWS(camera->set_focus_diopters(units::Diopter(INF)));
    CHECK_THROWS(camera->set_focus_diopters(units::Diopter(-2e12)));
    CHECK_THROWS(camera->set_focus_sensor_offset(units::Meter(nan)));
    CHECK_THROWS(camera->set_focus_sensor_offset(units::Meter(INF)));

    // Moving the sensor towards the lens by exactly the focal length (as stored, in single
    // precision) puts it at the lens:
    CHECK_THROWS(camera->set_focus_sensor_offset(units::Meter(-static_cast<double>(0.05f))));

    check_focus(*camera, 10.0);
}

TEST_CASE("A focus setting is kept on the side of the lens it was given on", "[cameras][focus]")
{
    auto camera = make_focus_camera();

    // Distance and diopters are object-side: the focus distance survives a focal length change.
    camera->set_focus_distance(units::Meter(10.0));
    camera->set_focal_length(units::Millimeter(100.0));
    check_focus(*camera, 10.0);

    camera->set_focus_diopters(units::Diopter(0.5));
    camera->set_focal_length(units::Millimeter(35.0));
    check_focus(*camera, 2.0);

    // A sensor offset is image-side: the offset survives instead, and the distance moves.
    const double offset = 250e-6;
    camera->set_focus_sensor_offset(units::Meter(offset));
    camera->set_focal_length(units::Millimeter(100.0));
    const double f = camera->focal_length().to_si();
    CHECK(close(camera->focus_sensor_offset().to_si(), offset));
    check_focus(*camera, f * (f + offset) / offset);
}

TEST_CASE("The defocus blur follows the current optics, whatever order they are set in",
          "[cameras][focus]")
{
    const double pitch = 5e-6;

    // Optics first, then focus:
    auto before = std::make_unique<CameraModel<RGB>>();
    before->configure_sensor_from_pitch(Resolution{32, 32}, units::Meter(pitch));
    before->set_focal_length(units::Millimeter(100.0));
    before->set_fstop(4.0f);
    before->set_focus_distance(units::Meter(10.0));

    // Focus first, then optics:
    auto after = std::make_unique<CameraModel<RGB>>();
    after->set_focus_distance(units::Meter(10.0));
    after->configure_sensor_from_pitch(Resolution{32, 32}, units::Meter(pitch));
    after->set_focal_length(units::Millimeter(100.0));
    after->set_fstop(4.0f);

    // 0.1 dpt * 100 mm * 12.5 mm / 5 um:
    CHECK(close(before->defocus_blur_radius(), 25.0, 1e-4));
    CHECK(close(after->defocus_blur_radius(), 25.0, 1e-4));

    // A sensor offset is re-resolved when the focal length changes:
    after->set_focus_sensor_offset(units::Micrometer(250.0));
    after->set_focal_length(units::Millimeter(50.0));
    CHECK(close(after->defocus_blur_radius(), expected_blur_radius(*after, pitch), 1e-4));

    // A replacement aperture gets a defocus kernel too:
    after->set_aperture<CircularAperture<RGB>>(units::Millimeter(10.0));
    CHECK(close(after->defocus_blur_radius(), expected_blur_radius(*after, pitch), 1e-4));
    CHECK(after->defocus_blur_radius() > 1.f);

    // And focusing at infinity removes the blur:
    after->set_focus_distance(units::Meter(INF));
    CHECK(after->defocus_blur_radius() == 0.f);
}

TEST_CASE("Depth of field rays follow a sensor offset when the focal length changes",
          "[cameras][focus]")
{
    auto camera = make_camera(false, true);
    camera->set_focus_sensor_offset(units::Micrometer(40.0));
    camera->set_focal_length(units::Millimeter(25.0));

    // 25 mm * 25.04 mm / 40 um, not the 10.02 m that the same offset gave at 20 mm:
    const double distance = camera->focus_distance().to_si();
    CHECK(close(distance, 0.025 * 0.02504 / 40e-6, 1e-5));
    check_rays_meet_focal_point(*camera, distance, false);
}
