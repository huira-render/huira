#include <cmath>
#include <cstddef>
#include <limits>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::WithinRel;

namespace {
constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
constexpr double INF = std::numeric_limits<double>::infinity();
constexpr float NaN_F = std::numeric_limits<float>::quiet_NaN();
constexpr double AU_M = 149597870700.0;

/// The scene, seen over an exposure from 0 to 1 s by a camera (with its true state: nothing is
/// displaced by light time or aberration), resolving its unresolved objects.
SceneView<RGB> view_of(const Scene<RGB>& scene, const InstanceHandle<RGB>& camera)
{
    return SceneView<RGB>(scene,
                          Interval{Time::from_et(0.0), Time::from_et(1.0)},
                          camera,
                          ObservationMode::TRUE_STATE);
}

/**
 * An asteroid's apparent magnitude in the IAU's H-G system (Bowell et al. 1989), at distances
 * from its Sun and the observer in AU, and a phase angle:
 *
 *     V = H + 5 log10(r delta) - 2.5 log10((1 - G) phi1(alpha) + G phi2(alpha)),
 *     phi_i(alpha) = exp(-A_i tan(alpha / 2)^B_i),  A = (3.33, 1.87),  B = (0.63, 1.22)
 */
double hg_magnitude(double H, double G, double r_au, double delta_au, double alpha)
{
    const double t = std::tan(alpha / 2.0);
    const double phi1 = std::exp(-3.33 * std::pow(t, 0.63));
    const double phi2 = std::exp(-1.87 * std::pow(t, 1.22));
    return H + 5.0 * std::log10(r_au * delta_au) - 2.5 * std::log10((1.0 - G) * phi1 + G * phi2);
}

/// Put an instance at a position, in meters.
InstanceHandle<RGB> place(InstanceHandle<RGB> instance, const Vec3<double>& position)
{
    instance.set_position(
        units::Meter(position.x), units::Meter(position.y), units::Meter(position.z));
    return instance;
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

TEST_CASE("An asteroid is as bright as the H-G magnitude system says", "[assets][unresolved]")
{
    // Before, its distances were multiplied by the meters in an AU, not divided by them, which
    // made every asteroid 112 magnitudes too faint (its irradiance, in a float, 0); and phi1's
    // exponent was 0.64, not 0.63.
    struct Geometry {
        double r_au;     // from its Sun
        double delta_au; // from the camera
        double degrees;  // the phase angle
    };
    const double H = 12.0;
    const double G = 0.15;
    const RGB albedo{0.3f, 0.2f, 0.1f};
    for (const Geometry& g : {Geometry{1.0, 1.0, 90.0},
                              Geometry{2.5, 1.5, 20.0},
                              Geometry{1.2, 0.3, 60.0},
                              Geometry{3.0, 2.0, 5.0}}) {
        INFO("r = " << g.r_au << " AU, delta = " << g.delta_au << " AU, alpha = " << g.degrees);
        const double alpha = g.degrees * PI<double>() / 180.0;

        // The camera at the origin, the asteroid along z, and its Sun at the phase angle from
        // the camera, in the plane of x and z:
        Scene<RGB> scene;
        auto camera = scene.root.new_instance(scene.new_camera_model());
        const Vec3<double> position{0.0, 0.0, g.delta_au * AU_M};
        auto sun = scene.root.new_instance(scene.new_sun_light());
        place(sun, position + g.r_au * AU_M * Vec3<double>{std::sin(alpha), 0.0, -std::cos(alpha)});
        auto asteroid = place(
            scene.root.new_instance(scene.new_unresolved_asteroid(H, G, sun, albedo)), position);

        const SceneView<RGB> view = view_of(scene, camera);
        const RGB expected = scene
                                 .new_unresolved_object_from_magnitude(
                                     hg_magnitude(H, G, g.r_au, g.delta_au, alpha), albedo)
                                 .get_irradiance();
        const RGB irradiance = view.unresolved_irradiance(asteroid, Time::from_et(0.5));
        for (std::size_t i = 0; i < RGB::size(); ++i) {
            INFO("bin " << i);
            REQUIRE(expected[i] > 0.f);
            CHECK_THAT(irradiance[i], WithinRel(expected[i], 1e-4f));
        }
    }
}
