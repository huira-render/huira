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
                camera->set_diopters(units::Diopter(-0.1));
                REQUIRE(std::abs(camera->get_focus_distance().to_si() + 10.0) < 1e-4);
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
                REQUIRE(camera->get_diopters().to_si() == 0.0);

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
