#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"
#include "huira/scene/state_callbacks/keplerian_orbit.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {
constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
constexpr double INF = std::numeric_limits<double>::infinity();
constexpr float NaN_F = std::numeric_limits<float>::quiet_NaN();
constexpr float INF_F = std::numeric_limits<float>::infinity();

/// A node outside any scene, as test_node_rates.cpp makes one.
class TestNode : public Node<RGB> {
  public:
    TestNode() : Node<RGB>(nullptr) {}
};
} // namespace

TEST_CASE("A node's position, motion and scale are checked when set", "[scene][validation]")
{
    // Before, each was taken, and the node rendered at NaN, or flattened (a scale of 0, whose
    // inverse the transform takes).
    TestNode node;
    CHECK_THROWS(node.set_position(Vec3<double>{0.0, NaN, 0.0}));
    CHECK_THROWS(node.set_position(units::Meter(INF), 0_m, 0_m));
    CHECK_THROWS(node.set_velocity(Vec3<double>{INF, 0.0, 0.0}));
    CHECK_THROWS(node.set_angular_velocity(Vec3<double>{0.0, 0.0, NaN}));
    CHECK_THROWS(node.set_body_angular_velocity(
        units::RadiansPerSecond(INF), units::RadiansPerSecond(0.0), units::RadiansPerSecond(0.0)));
    CHECK_THROWS(node.set_scale(1.0, 0.0, 1.0));
    CHECK_THROWS(node.set_scale(Vec3<double>{1.0, NaN, 1.0}));
    CHECK_THROWS(node.set_scale(1e-320, 1.0, 1.0)); // its inverse is infinite
    Transform<double> transform;
    transform.velocity = Vec3<double>{0.0, NaN, 0.0};
    CHECK_THROWS(node.set_manual_transform(transform));
    // A mirror is a scale too (a guard):
    CHECK_NOTHROW(node.set_scale(-1.0, 1.0, 1.0));
    CHECK_NOTHROW(node.set_position(Vec3<double>{1.0, 2.0, 3.0}));
}

TEST_CASE("A Keplerian orbit starts at its periapsis, a (1 - e) away", "[scene][orbit]")
{
    // conics_c() takes the periapsis distance first. It was given the semi-major axis, so an
    // eccentric orbit came out a / (1 - e) times too large.
    const double a = 10000e3;
    const double e = 0.5;
    const Time epoch = Time::from_et(0.0);
    KeplerianOrbitCallback orbit(units::Meter(a),
                                 e,
                                 units::Radian(0.3),
                                 units::Radian(0.2),
                                 units::Radian(0.1),
                                 units::Radian(0.0),
                                 epoch,
                                 398600.4418);
    orbit.evaluate(epoch, Rotation<double>{}, Vec3<double>{});
    CHECK(std::abs(glm::length(orbit.position) / (a * (1.0 - e)) - 1.0) < 1e-9);

    // And the elements must describe an orbit:
    TestNode node;
    const auto set = [&](double semi_major_axis, double eccentricity, double mu) {
        node.set_keplerian_orbit(
            units::Meter(semi_major_axis), eccentricity, 0_rad, 0_rad, 0_rad, 0_rad, epoch, mu);
    };
    CHECK_THROWS(set(-a, 0.5, 398600.0)); // an ellipse with a < 0
    CHECK_THROWS(set(a, 1.5, 398600.0));  // a hyperbola with a > 0
    CHECK_THROWS(set(a, 1.0, 398600.0));  // a parabola
    CHECK_THROWS(set(a, -0.1, 398600.0));
    CHECK_THROWS(set(a, NaN, 398600.0));
    CHECK_THROWS(set(a, 0.5, 0.0));
    CHECK_NOTHROW(set(a, 0.5, 398600.0));
    CHECK_NOTHROW(set(-a, 1.5, 398600.0));
}

TEST_CASE("The renderer's settings are checked when set", "[render][validation]")
{
    // Before, no sample per pixel made every pixel NaN, no bounce made every pixel black, and a
    // negative clamp made indirect light negative.
    Renderer<RGB> renderer;
    CHECK_THROWS(renderer.set_samples_per_pixel(0));
    CHECK_THROWS(renderer.set_max_bounces(0));
    CHECK_THROWS(renderer.set_min_samples(1));
    CHECK_THROWS(renderer.set_variance_threshold(NaN_F));
    CHECK_THROWS(renderer.set_variance_threshold(-1.f));
    CHECK_THROWS(renderer.set_indirect_clamp(-1.f));
    CHECK_THROWS(renderer.set_indirect_clamp(NaN_F));
    CHECK_THROWS(renderer.set_region_cull_margin_scale(INF_F));
    // No clamp, the default, is infinite (a guard):
    CHECK_NOTHROW(renderer.set_indirect_clamp(INF_F));
    CHECK_NOTHROW(renderer.set_samples_per_pixel(1));
}

TEST_CASE("Scene assets are checked when made", "[scene][validation]")
{
    // Before, each was taken, and rendered as NaN or nothing, or crashed when sampled.
    Scene<RGB> scene;
    CHECK_THROWS(scene.add_ellipsoid(1_m, 0_m, 1_m));
    CHECK_THROWS(scene.add_ellipsoid(1_m, units::Meter(INF), 1_m));
    CHECK_THROWS(scene.add_ellipsoid(1_m, -1_m, 1_m));

    // Meshes: indices in threes, and finite vertices:
    VertexBuffer<RGB> vertices(3);
    vertices[1].position = Vec3<float>{1.f, 0.f, 0.f};
    vertices[2].position = Vec3<float>{0.f, 1.f, 0.f};
    CHECK_NOTHROW(scene.add_mesh(IndexBuffer{0, 1, 2}, vertices));
    CHECK_THROWS(scene.add_mesh(IndexBuffer{0, 1, 2, 0}, vertices));
    vertices[1].normal = Vec3<float>{NaN_F, 0.f, 0.f};
    CHECK_THROWS(scene.add_mesh(IndexBuffer{0, 1, 2}, vertices));

    // Lights:
    CHECK_THROWS(scene.new_sphere_light(-1_m, units::Watt(1.0)));
    CHECK_THROWS(scene.new_sphere_light(
        1_m, units::SpectralWattsPerMeterSquaredSteradian<RGB>(RGB{1.f, NaN_F, 1.f})));
    CHECK_THROWS(scene.new_sphere_light(1_m, units::SpectralWatts<RGB>(RGB{-1.f})));
    CHECK_THROWS(black_body<RGB>(-10.0, 100));
    CHECK_THROWS(black_body<RGB>(5000.0, 1));
    CHECK_THROWS(scene.add_light(nullptr));

    // The sky, and textures:
    CHECK_THROWS(scene.set_background_radiance(NaN_F));
    CHECK_THROWS(scene.set_background_radiance(RGB{-1.f}));
    CHECK_THROWS(scene.set_background_radiance(Image<RGB>(0, 0)));
    CHECK_THROWS(scene.add_texture(Image<float>(0, 3)));
    CHECK_THROWS(scene.add_texture(Image<RGB>(2, 2, RGB{NaN_F})));
    CHECK_THROWS(scene.add_texture(Image<float>(2, 2, INF_F)));
    // A normal map pixel of (0.5, 0.5, 0.5) encodes no direction:
    CHECK_THROWS(scene.add_normal_texture(Image<Vec3<float>>(2, 2, Vec3<float>{0.5f})));
    CHECK_NOTHROW(scene.add_normal_texture(Image<Vec3<float>>(2, 2, Vec3<float>{0.5f, 0.5f, 1.f})));
    CHECK_THROWS(Image<RGB>(-1, 4));

    // Materials and media:
    CHECK_THROWS(scene.new_bsdf_hapke(0.06f, 1.f, 1.f, 0.5f)); // b = 1: an infinite lobe
    CHECK_THROWS(scene.new_bsdf_hapke(0.06f, 1.f, 0.3f, 2.f));
    CHECK_THROWS(scene.new_bsdf_hapke(NaN_F, 1.f, 0.3f, 0.5f));
    CHECK_NOTHROW(scene.new_bsdf_hapke(0.06f, 1.f, 0.3f, 0.5f));
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    CHECK_THROWS(material.set_roughness_factor(NaN_F));
    CHECK_THROWS(material.set_alpha_factor(1.5f));
    CHECK_THROWS(material.set_metallic_factor(-0.1f));
    CHECK_THROWS(material.set_albedo_factor(RGB{NaN_F}));
    CHECK_THROWS(material.set_transmission_factor(RGB{2.f}));
    CHECK_THROWS(material.set_emissive_factor(RGB{-1.f}));
    CHECK_THROWS(material.set_bsdf(nullptr));
    CHECK_THROWS(scene.new_constant_density_field(RGB{NaN_F}, RGB{0.f}));
    CHECK_THROWS(scene.new_constant_density_field(RGB{0.f}, RGB{INF_F}));

    // Unresolved objects and stars:
    CHECK_THROWS(scene.new_unresolved_object_from_magnitude(NaN, RGB{0.3f}));
    auto sun = scene.root.new_instance(scene.new_sun_light());
    CHECK_THROWS(scene.new_unresolved_asteroid(15.0, 3.0, sun)); // negative beyond 139 deg
    CHECK_NOTHROW(scene.new_unresolved_asteroid(15.0, 0.15, sun));
    CHECK_THROWS(Star<RGB>(Vec3<double>{0.0, 0.0, 0.0}, RGB{1.f}));
    CHECK_THROWS(Star<RGB>(Vec3<double>{NaN, 0.0, 1.0}, RGB{1.f}));
    CHECK_THROWS(Star<RGB>(Vec3<double>{0.0, 0.0, 1.0}, RGB{-1.f}));
    StarData data;
    data.RA = 0.1;
    data.DEC = 0.2;
    CHECK_FALSE(Star<RGB>::usable(data)); // no temperature or solid angle
    CHECK_THROWS(Star<RGB>(data, 0.0));
    data.temperature = 5800.f;
    data.solid_angle = 1e-16;
    CHECK(Star<RGB>::usable(data));
    // At a pole, right ascension's motion no longer divides by zero:
    data.DEC = 3.14159265358979323846 / 2.0;
    data.pmRA = 10.f;
    CHECK(std::isfinite(Star<RGB>(data, 10.0).get_direction().x));

    // Spectral data:
    CHECK_THROWS(RGB::integrate_over_data({400.0, NaN, 600.0}, {1.f, 1.f, 1.f}));
    CHECK_THROWS(RGB::integrate_over_data({-400.0, 500.0, 600.0}, {1.f, 1.f, 1.f}));
    CHECK_THROWS(RGB::integrate_over_data({400.0, 500.0, 600.0}, {1.f, NaN_F, 1.f}));
}

TEST_CASE("An instance cannot look at itself or at a point that is not finite",
          "[scene][validation]")
{
    // Before, its forward direction was NaN at the render.
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    CHECK_THROWS(camera.look_at(camera));
    CHECK_THROWS(camera.look_at(Vec3<double>{0.0, NaN, 1.0}));
    CHECK_THROWS(camera.look_at(Vec3<double>{0.0, 0.0, 1.0}, Vec3<double>{INF, 0.0, 0.0}));
    CHECK_NOTHROW(camera.look_at(Vec3<double>{0.0, 0.0, 1.0}));
}

TEST_CASE("A scene view needs a temporal sample and an exposure that runs forwards",
          "[scene][validation]")
{
    // Before, no temporal samples were taken as one, and an interval whose fields had been
    // changed to run backwards read out a negative exposure.
    Scene<RGB> scene;
    auto camera = scene.root.new_instance(scene.new_camera_model());
    Interval exposure{Time::from_et(0.0), Time::from_et(1.0)};
    CHECK_THROWS(SceneView<RGB>(scene, exposure, camera, ObservationMode::TRUE_STATE, 0));
    exposure.end = Time::from_et(-1.0);
    CHECK_THROWS(SceneView<RGB>(scene, exposure, camera, ObservationMode::TRUE_STATE, 1));
}
