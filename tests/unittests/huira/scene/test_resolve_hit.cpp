#include <cmath>
#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

TEST_CASE("A hit on an unevenly scaled instance has the scaled surface's normals",
          "[scene][resolve_hit]")
{
    // A sphere 1 m in radius, in an instance 100 m in front of the camera scaled 3 times along
    // its x axis and halved along its z axis, so that it is drawn as an ellipsoid; turned 30
    // degrees about the line of sight, or turning 1 rad during the exposure. Turned by the
    // instance's transform as a direction is, a normal leaned up to 71 degrees away from the
    // ellipsoid's here: it must be turned by the inverse transpose, and stay at right angles to
    // the tangents.
    for (const bool turning : {false, true}) {
        INFO((turning ? "turning" : "still"));
        Scene<RGB> scene;
        auto material = scene.new_material(scene.new_bsdf_lambertian());
        auto body = scene.root.new_instance(
            scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m), material));
        body.set_position(0_m, 0_m, 100_m);
        body.set_scale(3.0, 1.0, 0.5);
        if (turning) {
            body.set_angular_velocity(units::RadiansPerSecond(0.0),
                                      units::RadiansPerSecond(0.0),
                                      units::RadiansPerSecond(1.0));
        } else {
            body.set_rotation_local_to_parent(Vec3<double>{0.0, 0.0, 1.0}, units::Degree(30.0));
        }
        auto camera = scene.root.new_instance(scene.new_camera_model());
        SceneView<RGB> scene_view(scene,
                                  Interval{Time::from_et(0.0), Time::from_et(1.0)},
                                  camera,
                                  ObservationMode::TRUE_STATE,
                                  turning ? 3 : 1);

        const Vec3<float> scale{3.f, 1.f, 0.5f};
        for (float time : {0.f, 0.3f, 0.7f, 1.f}) {
            // How far it is turned about the line of sight at that time:
            const float angle = turning ? time : PI<float>() / 6.f;
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            std::size_t hits = 0;
            for (float x = -2.f; x <= 2.f; x += 1.f) {
                for (float y = -0.6f; y <= 0.6f; y += 0.3f) {
                    INFO("time " << time << ", aimed at " << x << ", " << y);
                    const Ray<RGB> ray(Vec3<float>{0.f}, glm::normalize(Vec3<float>{x, y, 100.f}));
                    const HitRecord hit = scene_view.intersect(ray, time);
                    if (!hit.hit()) {
                        continue;
                    }
                    const Interaction<RGB> isect = scene_view.resolve_hit(ray, hit);
                    ++hits;

                    // The hit on the unit sphere, in the instance's frame, turned back and
                    // unscaled:
                    const Vec3<float> d = isect.position - Vec3<float>{0.f, 0.f, 100.f};
                    const Vec3<float> on_sphere =
                        Vec3<float>{c * d.x + s * d.y, -s * d.x + c * d.y, d.z} / scale;
                    REQUIRE(std::abs(glm::length(on_sphere) - 1.f) < 1e-4f);

                    // The ellipsoid's normal there, turned with it, facing the ray:
                    const Vec3<float> local = on_sphere / scale;
                    Vec3<float> normal = glm::normalize(
                        Vec3<float>{c * local.x - s * local.y, s * local.x + c * local.y, local.z});
                    if (glm::dot(normal, ray.direction()) > 0.f) {
                        normal = -normal;
                    }
                    CHECK(glm::dot(isect.normal_g, normal) > 0.99999f);
                    CHECK(glm::dot(isect.normal_s, normal) > 0.99999f);
                    CHECK(std::abs(glm::dot(isect.tangent, isect.normal_s)) < 1e-5f);
                }
            }
            CHECK(hits >= 10);
        }
    }
}
