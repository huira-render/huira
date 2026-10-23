#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::WithinRel;

// An unresolved Lambertian sphere of radius R and albedo a, lit by an irradiance E from a
// direction at a phase angle alpha from the observer's, at a distance d, gives the observer
//
//     F = (2/3) a (R / d)^2 E Phi(alpha),  Phi(alpha) = (sin alpha + (pi - alpha) cos alpha) / pi
//
// (2/3 a is a Lambertian sphere's geometric albedo, and Phi its phase function). These tests
// check its estimate of F against that, and its options against each other.

namespace {
constexpr float NaN_F = std::numeric_limits<float>::quiet_NaN();
constexpr float INF_F = std::numeric_limits<float>::infinity();
constexpr double AU_M = 149597870700.0;
constexpr double SUN_RADIUS_M = 6.957e8;

using Radiance = units::SpectralWattsPerMeterSquaredSteradian<RGB>;

double lambert_phase(double alpha)
{
    return (std::sin(alpha) + (PI<double>() - alpha) * std::cos(alpha)) / PI<double>();
}

/// The sun's irradiance at 1 AU, as Scene::new_sun_light() makes the sun.
RGB sun_irradiance_at_1_au()
{
    const double sin2 = (SUN_RADIUS_M / AU_M) * (SUN_RADIUS_M / AU_M);
    return black_body<RGB>(5800.0, 1000) * static_cast<float>(PI<double>() * sin2);
}

/// The F above, for each bin.
RGB lambert_sphere(double R, double a, double d, const RGB& E, double alpha)
{
    return E * static_cast<float>((2.0 / 3.0) * a * (R / d) * (R / d) * lambert_phase(alpha));
}

/**
 * The irradiance, per unit of the sunlight's, that a Lambertian ball of radius Rp and albedo ap,
 * lit at full phase (the sun behind the observer), gives at a distance D from its center. Far
 * away it is (2/3) ap (Rp / D)^2, but nearer it is more, by about 0.75 Rp / D, the near side being
 * nearer: this integrates it over the directions the ball fills, at angles psi from its center.
 */
double full_phase_irradiance(double Rp, double ap, double D)
{
    const double psi_max = std::asin(Rp / D);
    constexpr int STEPS = 10000;
    double sum = 0.0;
    for (int k = 0; k < STEPS; ++k) {
        const double psi = (k + 0.5) * psi_max / STEPS;
        // How far a ray at psi goes to the ball, and the cosine of the sunlight's incidence there:
        const double t =
            D * std::cos(psi) - std::sqrt(Rp * Rp - D * D * std::sin(psi) * std::sin(psi));
        const double cos_i = (D - t * std::cos(psi)) / Rp;
        sum += (ap / PI<double>()) * cos_i * std::cos(psi) * 2.0 * PI<double>() * std::sin(psi);
    }
    return sum * psi_max / STEPS;
}

/// Each bin of a matches b's, to a relative tolerance.
void check_bins(const RGB& a, const RGB& b, float tolerance)
{
    for (std::size_t i = 0; i < RGB::size(); ++i) {
        INFO("bin " << i);
        CHECK_THAT(a[i], WithinRel(b[i], tolerance));
    }
}

/// Put an instance at a position, in meters.
InstanceHandle<RGB> place(InstanceHandle<RGB> instance, const Vec3<double>& position)
{
    instance.set_position(
        units::Meter(position.x), units::Meter(position.y), units::Meter(position.z));
    return instance;
}

/// A scene with a camera at its origin.
struct CameraScene {
    Scene<RGB> scene;
    InstanceHandle<RGB> camera;

    CameraScene() : camera(scene.root.new_instance(scene.new_camera_model())) {}

    CameraScene(const CameraScene&) = delete;
    CameraScene& operator=(const CameraScene&) = delete;
    CameraScene(CameraScene&&) = delete;
    CameraScene& operator=(CameraScene&&) = delete;

    /// The sun, 1 AU from a point, in the direction at a phase angle from the camera's: in the
    /// plane of x and z, the camera being along -z from the point.
    InstanceHandle<RGB> sun_at_phase(const Vec3<double>& point, double alpha)
    {
        auto sun = scene.root.new_instance(scene.new_sun_light());
        const Vec3<double> toward{std::sin(alpha), 0.0, -std::cos(alpha)};
        place(sun, point + AU_M * toward);
        return sun;
    }

    /// Resolve the scene's unresolved objects, as seen over an exposure from 0 to 1 s, with this
    /// many temporal samples (as the motion needs, by default).
    SceneView<RGB> view(std::optional<std::size_t> temporal_samples = std::nullopt) const
    {
        return SceneView<RGB>(scene,
                              Interval{Time::from_et(0.0), Time::from_et(1.0)},
                              camera,
                              ObservationMode::TRUE_STATE,
                              temporal_samples);
    }
};

const Time MID = Time::from_et(0.5);
} // namespace

TEST_CASE("An unresolved sphere's radius must be positive and finite, and its albedo a ratio",
          "[assets][unresolved]")
{
    Scene<RGB> scene;
    auto sun = scene.root.new_instance(scene.new_sun_light());
    for (const units::Meter& radius : {0_m, -1_m, units::Meter(INF_F)}) {
        INFO("radius " << radius.to_si());
        CHECK_THROWS(scene.new_unresolved_sphere(radius, 0.5f));
        CHECK_THROWS(scene.new_unresolved_sphere(radius, sun));
    }
    CHECK_THROWS(scene.new_unresolved_sphere(units::Meter(NaN_F), 0.5f));
    CHECK_THROWS(scene.new_unresolved_sphere(1_m, 1.5f));
    CHECK_THROWS(scene.new_unresolved_sphere(1_m, -0.5f));
    CHECK_THROWS(scene.new_unresolved_sphere(1_m, RGB{0.5f, NaN_F, 0.5f}));
    CHECK_THROWS(scene.new_unresolved_sphere(1_m, sun, 1.5f));
    CHECK_THROWS(scene.new_unresolved_sphere(1_m, sun, RGB{0.5f, 2.f, 0.5f}));
    CHECK_NOTHROW(scene.new_unresolved_sphere(1_m, RGB{0.f, 0.5f, 1.f}));

    // Only a light can light it alone:
    auto not_a_light = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
    CHECK_THROWS(scene.new_unresolved_sphere(1_m, not_a_light));
    CHECK_THROWS(scene.new_unresolved_sphere(1_m, not_a_light, 0.5f));

    // And it takes at least one sample of each light and indirect source:
    UnresolvedLambertianSphere<RGB> sphere(1_m, 0.5f);
    CHECK_THROWS(sphere.set_light_samples(0));
    CHECK_THROWS(sphere.set_indirect_source_samples(0));
    CHECK_NOTHROW(sphere.set_light_samples(1));
    CHECK_NOTHROW(sphere.set_indirect_source_samples(1));
    CHECK(sphere.type() == "UnresolvedLambertianSphere");
}

TEST_CASE("A sunlit unresolved sphere is as bright as Lambert's law says, at each phase angle",
          "[assets][unresolved]")
{
    const double R = 1000.0;
    const double a = 0.4;
    const double d = 1e6;
    const Vec3<double> position{0.0, 0.0, d};

    for (const double degrees : {0.0, 30.0, 60.0, 90.0, 120.0, 150.0}) {
        INFO("phase angle " << degrees << " degrees");
        const double alpha = degrees * PI<double>() / 180.0;

        CameraScene s;
        s.sun_at_phase(position, alpha);
        auto sphere = std::make_shared<UnresolvedLambertianSphere<RGB>>(units::Meter(R),
                                                                        static_cast<float>(a));
        sphere->set_light_samples(1024);
        auto instance =
            place(s.scene.root.new_instance(s.scene.add_unresolved_object(sphere)), position);

        const SceneView<RGB> view = s.view();
        check_bins(view.unresolved_irradiance(instance, MID),
                   lambert_sphere(R, a, d, sun_irradiance_at_1_au(), alpha),
                   2e-3f);
    }
}

TEST_CASE("A sphere seen from behind its lit side is dark", "[assets][unresolved]")
{
    // At a phase angle of 180 degrees, the sphere is between the camera and the sun:
    CameraScene s;
    const Vec3<double> position{0.0, 0.0, 1e6};
    s.sun_at_phase(position, PI<double>());
    auto behind =
        place(s.scene.root.new_instance(s.scene.new_unresolved_sphere(1000_m, 1.f)), position);
    // And at full phase, for comparison:
    CameraScene full;
    full.sun_at_phase(position, 0.0);
    auto lit = place(full.scene.root.new_instance(full.scene.new_unresolved_sphere(1000_m, 1.f)),
                     position);

    const SceneView<RGB> view = s.view();
    const SceneView<RGB> full_view = full.view();
    const RGB lit_irradiance = full_view.unresolved_irradiance(lit, MID);
    REQUIRE(lit_irradiance[1] > 0.f);
    CHECK(view.unresolved_irradiance(behind, MID)[1] < 1e-6f * lit_irradiance[1]);
}

TEST_CASE("An unresolved sphere's albedo scales its brightness, bin by bin", "[assets][unresolved]")
{
    CameraScene s;
    const Vec3<double> position{0.0, 0.0, 1e6};
    s.sun_at_phase(position, PI<double>() / 3.0);
    auto white =
        place(s.scene.root.new_instance(s.scene.new_unresolved_sphere(1000_m, 1.f)), position);
    auto colored = place(
        s.scene.root.new_instance(s.scene.new_unresolved_sphere(1000_m, RGB{0.25f, 0.5f, 1.f})),
        position);
    auto gray =
        place(s.scene.root.new_instance(s.scene.new_unresolved_sphere(1000_m, 0.5f)), position);

    const SceneView<RGB> view = s.view();
    const RGB w = view.unresolved_irradiance(white, MID);
    check_bins(view.unresolved_irradiance(colored, MID), w * RGB{0.25f, 0.5f, 1.f}, 2e-3f);
    check_bins(view.unresolved_irradiance(gray, MID), w * 0.5f, 2e-3f);
}

TEST_CASE("An unresolved sphere is lit by every light, unless it is linked to one",
          "[assets][unresolved]")
{
    CameraScene s;
    const Vec3<double> position{0.0, 0.0, 1e6};
    auto first = s.sun_at_phase(position, PI<double>() / 4.0);
    auto second = s.sun_at_phase(position, PI<double>() / 2.0);

    auto both = std::make_shared<UnresolvedLambertianSphere<RGB>>(1000_m, 0.5f);
    auto first_only = std::make_shared<UnresolvedLambertianSphere<RGB>>(1000_m, first, 0.5f);
    auto second_only = std::make_shared<UnresolvedLambertianSphere<RGB>>(1000_m, second, RGB{0.5f});
    std::vector<InstanceHandle<RGB>> instances;
    for (const auto& sphere : {both, first_only, second_only}) {
        sphere->set_light_samples(1024);
        instances.push_back(
            place(s.scene.root.new_instance(s.scene.add_unresolved_object(sphere)), position));
    }

    const SceneView<RGB> view = s.view();
    const RGB E = sun_irradiance_at_1_au();
    const RGB both_irradiance = view.unresolved_irradiance(instances[0], MID);
    const RGB first_irradiance = view.unresolved_irradiance(instances[1], MID);
    const RGB second_irradiance = view.unresolved_irradiance(instances[2], MID);
    check_bins(first_irradiance, lambert_sphere(1000.0, 0.5, 1e6, E, PI<double>() / 4.0), 2e-3f);
    check_bins(second_irradiance, lambert_sphere(1000.0, 0.5, 1e6, E, PI<double>() / 2.0), 2e-3f);
    check_bins(both_irradiance, first_irradiance + second_irradiance, 2e-3f);
}

TEST_CASE("An unresolved sphere linked to a light that is not in the view cannot be resolved",
          "[assets][unresolved]")
{
    CameraScene s;
    auto sun = s.scene.root.new_instance(s.scene.new_sun_light());
    sun.set_position(0_m, 0_m, -1_au);
    auto sphere = s.scene.new_unresolved_sphere(1000_m, sun, 0.5f);
    s.scene.root.new_instance(sphere).set_position(0_m, 0_m, 1000_Km);
    CHECK_NOTHROW(s.view());

    // Its light taken out of the scene graph (the light itself stays in the scene):
    s.scene.root.delete_instance(sun);
    CHECK_THROWS(s.view());
}

TEST_CASE("An unresolved sphere where the camera is cannot be resolved", "[assets][unresolved]")
{
    // At no distance, it would give an infinite irradiance.
    CameraScene s;
    s.sun_at_phase(Vec3<double>{0.0, 0.0, 0.0}, 0.0);
    s.scene.root.new_instance(s.scene.new_unresolved_sphere(1000_m, 0.5f));
    CHECK_THROWS(s.view());
}

TEST_CASE("An unresolved sphere in a shadow is dark", "[assets][unresolved]")
{
    CameraScene s;
    const Vec3<double> position{0.0, 0.0, 1e6};
    const double alpha = PI<double>() / 3.0;
    s.sun_at_phase(position, alpha);
    auto shadowed =
        place(s.scene.root.new_instance(s.scene.new_unresolved_sphere(1000_m, 1.f)), position);

    // A ball 10 km across, 100 km from the sphere toward the sun, hides all of it:
    auto ball = s.scene.add_primitive(s.scene.add_ellipsoid(10_Km, 10_Km, 10_Km),
                                      s.scene.new_material(s.scene.new_bsdf_lambertian()));
    const Vec3<double> toward_sun{std::sin(alpha), 0.0, -std::cos(alpha)};
    place(s.scene.root.new_instance(ball), position + 1e5 * toward_sun);

    const SceneView<RGB> view = s.view();
    CHECK(view.unresolved_irradiance(shadowed, MID) == RGB{0.f});
}

TEST_CASE("An unresolved sphere is lit by the sunlight an indirect source reflects onto it",
          "[assets][unresolved]")
{
    // A Lambertian planet of radius Rp and albedo ap, lit at full phase as the sphere sees it,
    // from a distance D, gives the sphere an irradiance Ep (by the same law, far away: see
    // full_phase_irradiance()), which the sphere reflects as it reflects sunlight.
    //
    // The sphere is linked to a light that gives nothing, so that the planet's light is all it
    // has: none at all unless the planet is an indirect source.
    const double R = 1000.0;
    const double a = 0.5;
    const double d = 1e7;
    const double Rp = 1e6;
    const double ap = 0.5;
    const double D = 1e8;

    const auto resolve = [&](bool indirect, bool sun_behind_sphere = true) {
        CameraScene s;
        auto dark = s.scene.root.new_instance(s.scene.new_sphere_light(1_m, Radiance(RGB{0.f})));
        dark.set_position(0_m, 0_m, -1_au);

        // The planet is beside the sphere (+y), the sun behind the sphere from the planet:
        const Vec3<double> position{0.0, 0.0, d};
        const Vec3<double> planet_position = position + Vec3<double>{0.0, D, 0.0};
        auto material = s.scene.new_material(s.scene.new_bsdf_lambertian());
        material.set_albedo(RGB{static_cast<float>(ap)});
        auto planet = s.scene.root.new_instance(s.scene.add_primitive(
            s.scene.add_ellipsoid(units::Meter(Rp), units::Meter(Rp), units::Meter(Rp)), material));
        place(planet, planet_position);
        planet.set_indirect_source(indirect);
        const double toward_sun = sun_behind_sphere ? -1.0 : 1.0;
        place(s.scene.root.new_instance(s.scene.new_sun_light()),
              planet_position + Vec3<double>{0.0, toward_sun * AU_M, 0.0});

        auto sphere = std::make_shared<UnresolvedLambertianSphere<RGB>>(
            units::Meter(R), dark, static_cast<float>(a));
        sphere->set_indirect_source_samples(1 << 18);
        auto instance =
            place(s.scene.root.new_instance(s.scene.add_unresolved_object(sphere)), position);

        const SceneView<RGB> view = s.view();
        return view.unresolved_irradiance(instance, MID);
    };

    CHECK(resolve(false) == RGB{0.f});
    // Nor any from the planet's night side, with the sun beyond it:
    CHECK(resolve(true, false) == RGB{0.f});

    // Seen from the sphere, the planet is at a phase angle of 90 degrees from the camera. (The
    // estimate is noisy: a part in 250, from rays that miss the planet, and a planet's shading.)
    const RGB Ep = sun_irradiance_at_1_au() * static_cast<float>(full_phase_irradiance(Rp, ap, D));
    check_bins(resolve(true), lambert_sphere(R, a, d, Ep, PI<double>() / 2.0), 0.02f);
}

TEST_CASE("A moving unresolved sphere's brightness follows its distance through the exposure",
          "[assets][unresolved]")
{
    // From 1000 km to 2000 km from the camera over the exposure, with the sun behind the camera
    // (far enough that the sphere's light from it doesn't change):
    CameraScene s;
    s.sun_at_phase(Vec3<double>{0.0, 0.0, 1e6}, 0.0);
    auto sphere = std::make_shared<UnresolvedLambertianSphere<RGB>>(1000_m, 0.5f);
    sphere->set_light_samples(1024);
    auto instance = s.scene.root.new_instance(s.scene.add_unresolved_object(sphere));
    instance.set_position(0_m, 0_m, 1000_Km);
    instance.set_velocity(0_mps, 0_mps, 1000_Kmps);

    const SceneView<RGB> view = s.view(3);
    const RGB E = sun_irradiance_at_1_au();
    check_bins(view.unresolved_irradiance(instance, Time::from_et(0.0)),
               lambert_sphere(1000.0, 0.5, 1e6, E, 0.0),
               2e-3f);
    check_bins(view.unresolved_irradiance(instance, Time::from_et(0.5)),
               lambert_sphere(1000.0, 0.5, 1.5e6, E, 0.0),
               2e-3f);
    check_bins(view.unresolved_irradiance(instance, Time::from_et(1.0)),
               lambert_sphere(1000.0, 0.5, 2e6, E, 0.0),
               2e-3f);
}
