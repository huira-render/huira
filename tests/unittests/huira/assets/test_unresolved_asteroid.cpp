#include <limits>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {
constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
constexpr double INF = std::numeric_limits<double>::infinity();
constexpr float NaN_F = std::numeric_limits<float>::quiet_NaN();

/// The scene, seen over an exposure from 0 to 1 s by a camera (with its true state: nothing is
/// displaced by light time or aberration), resolving its unresolved objects.
SceneView<RGB> view_of(const Scene<RGB>& scene, const InstanceHandle<RGB>& camera)
{
    return SceneView<RGB>(scene,
                          Interval{Time::from_et(0.0), Time::from_et(1.0)},
                          camera,
                          ObservationMode::TRUE_STATE);
}
} // namespace

TEST_CASE("An asteroid needs a light, a finite H and G, and an albedo that is a ratio",
          "[assets][unresolved]")
{
    Scene<RGB> scene;
    auto sun = scene.root.new_instance(scene.new_sun_light());

    CHECK_THROWS(scene.new_unresolved_asteroid(NaN, 0.15, sun));
    CHECK_THROWS(scene.new_unresolved_asteroid(INF, 0.15, sun));
    CHECK_THROWS(scene.new_unresolved_asteroid(15.0, NaN, sun));
    CHECK_THROWS(scene.new_unresolved_asteroid(15.0, -INF, sun));
    CHECK_THROWS(scene.new_unresolved_asteroid(15.0, 0.15, sun, 1.5f));
    CHECK_THROWS(scene.new_unresolved_asteroid(15.0, 0.15, sun, -0.1f));
    CHECK_THROWS(scene.new_unresolved_asteroid(15.0, 0.15, sun, RGB{0.5f, NaN_F, 0.5f}));

    // Its light is the instance of a light, and nothing else:
    auto not_a_light = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
    CHECK_THROWS(scene.new_unresolved_asteroid(15.0, 0.15, not_a_light));
    auto ball = scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m)));
    CHECK_THROWS(scene.new_unresolved_asteroid(15.0, 0.15, ball));

    auto asteroid = scene.new_unresolved_asteroid(15.0, 0.15, sun, RGB{0.2f, 0.3f, 0.4f});
    CHECK(asteroid.type() == "UnresolvedAsteroid");
    CHECK_NOTHROW(scene.new_unresolved_asteroid(15.0, 0.0, sun, 0.25f));
    CHECK_NOTHROW(scene.new_unresolved_asteroid(-1.0, 1.0, sun)); // bright, and steep
}

TEST_CASE("An asteroid whose light is not in the view cannot be resolved", "[assets][unresolved]")
{
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, 0_m, -1_au);
    scene.root.new_instance(scene.new_unresolved_asteroid(15.0, 0.15, sun))
        .set_position(0_m, 0_m, 1000_Km);
    CHECK_NOTHROW(view_of(scene, camera));

    // Its light taken out of the scene graph (the light itself stays in the scene):
    scene.root.delete_instance(sun);
    CHECK_THROWS(view_of(scene, camera));
}

TEST_CASE("An asteroid at its light, or at the camera, cannot be resolved", "[assets][unresolved]")
{
    // The H-G system's magnitude is undefined at no distance from either.
    const auto view_with_asteroid_at = [](units::Meter z) {
        Scene<RGB> scene;
        auto camera = scene.root.new_instance(scene.new_camera_model());
        auto sun = scene.root.new_instance(scene.new_sun_light());
        sun.set_position(0_m, 0_m, 1_au);
        scene.root.new_instance(scene.new_unresolved_asteroid(15.0, 0.15, sun))
            .set_position(0_m, 0_m, z);
        const SceneView<RGB> view = view_of(scene, camera);
    };
    CHECK_NOTHROW(view_with_asteroid_at(0.5_au));
    CHECK_THROWS(view_with_asteroid_at(1_au));
    CHECK_THROWS(view_with_asteroid_at(0_m));
}

TEST_CASE("An asteroid too bright for its irradiance to be held is refused", "[assets][unresolved]")
{
    // An absolute magnitude of -400 is 10^160 times brighter than one of 0: no float holds that.
    // (At full phase: the sun is behind the camera.)
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, 0_m, -1_au);
    scene.root.new_instance(scene.new_unresolved_asteroid(-400.0, 0.15, sun))
        .set_position(0_m, 0_m, 1_au);
    CHECK_THROWS(view_of(scene, camera));
}
