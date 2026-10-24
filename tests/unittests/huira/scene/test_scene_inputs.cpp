#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
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

    TestNode(const TestNode&) = delete;
    TestNode& operator=(const TestNode&) = delete;
    TestNode(TestNode&&) = delete;
    TestNode& operator=(TestNode&&) = delete;
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
                                 398600.4418_Km3ps2);
    const PositionState state =
        orbit.evaluate(PoseContext{epoch, epoch, ObservationMode::TRUE_STATE}, RotationState{});
    CHECK(std::abs(glm::length(state.position) / (a * (1.0 - e)) - 1.0) < 1e-9);

    // And the elements must describe an orbit:
    TestNode node;
    const auto set = [&](double semi_major_axis, double eccentricity, double mu_Km3ps2) {
        node.set_keplerian_orbit(units::Meter(semi_major_axis),
                                 eccentricity,
                                 0_rad,
                                 0_rad,
                                 0_rad,
                                 0_rad,
                                 epoch,
                                 units::CubicKilometersPerSecondSquared(mu_Km3ps2));
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
    CHECK_THROWS(renderer.set_noise_threshold(NaN_F));
    CHECK_THROWS(renderer.set_noise_threshold(-1.f));
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
    // An albedo is a reflectance, at most 1, as the material's own is (Hapke took the square root
    // of 1 minus it, NaN):
    vertices[1].albedo = RGB{1.5f};
    CHECK_THROWS(scene.add_mesh(IndexBuffer{0, 1, 2}, vertices));
    vertices[1].albedo = RGB{1.f};
    vertices[1].normal = Vec3<float>{NaN_F, 0.f, 0.f};
    CHECK_THROWS(scene.add_mesh(IndexBuffer{0, 1, 2}, vertices));

    // Lights:
    CHECK_THROWS(scene.new_sphere_light(-1_m, units::Watt(1.0)));
    CHECK_THROWS(scene.new_sphere_light(
        1_m, units::SpectralWattsPerMeterSquaredSteradian<RGB>(RGB{1.f, NaN_F, 1.f})));
    CHECK_THROWS(scene.new_sphere_light(1_m, units::SpectralWatts<RGB>(RGB{-1.f})));
    CHECK_THROWS(black_body<RGB>(-10.0, 100));
    CHECK_THROWS(black_body<RGB>(5000.0, 1));
    CHECK_THROWS(black_body<RGB>(1e40, 100)); // more than a float holds
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
    CHECK_THROWS(material.set_roughness(1.5f));
    CHECK_THROWS(material.set_alpha_factor(1.5f));
    CHECK_THROWS(material.set_alpha(-0.1f));
    CHECK_THROWS(material.set_metallic_factor(-0.1f));
    CHECK_THROWS(material.set_metallic(NaN_F));
    CHECK_THROWS(material.set_albedo_factor(RGB{NaN_F}));
    CHECK_THROWS(material.set_albedo_factor(RGB{1.5f})); // a reflectance, so at most 1
    CHECK_THROWS(material.set_albedo(RGB{-0.1f}));
    CHECK_THROWS(material.set_transmission_factor(RGB{2.f}));
    CHECK_THROWS(material.set_transmission(RGB{INF_F}));
    // ...and so are their images' pixels, which a texture allows above 1, as an emissive one is:
    auto bright = scene.add_texture(Image<RGB>(2, 2, RGB{1.5f}));
    CHECK_THROWS(material.set_albedo_image(bright));
    CHECK_THROWS(material.set_transmission_image(bright));
    CHECK_NOTHROW(material.set_emissive_image(bright));
    auto gray = scene.add_texture(Image<RGB>(2, 2, RGB{0.5f}));
    CHECK_NOTHROW(material.set_albedo_image(gray));
    CHECK_NOTHROW(material.set_transmission_image(gray));
    CHECK_THROWS(material.set_emissive_factor(RGB{-1.f}));
    CHECK_THROWS(material.set_emissive(RGB{NaN_F}));
    CHECK_NOTHROW(material.set_emissive(RGB{1000.f})); // a radiance, with no upper bound
    CHECK_THROWS(material.set_normal_factor(NaN_F));
    CHECK_THROWS(material.set_bsdf(nullptr));
    CHECK_THROWS(scene.new_constant_density_field(RGB{NaN_F}, RGB{0.f}));
    CHECK_THROWS(scene.new_constant_density_field(RGB{0.f}, RGB{INF_F}));

    // Unresolved objects and stars:
    CHECK_THROWS(scene.new_unresolved_object_from_magnitude(NaN, RGB{0.3f}));
    CHECK_THROWS(visual_magnitude_to_irradiance<RGB>(-200.0));  // more than a float holds
    CHECK(visual_magnitude_to_irradiance<RGB>(-26.74).valid()); // the Sun (a guard)
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

TEST_CASE("A hot black body's radiance is Rayleigh-Jeans', not infinite", "[scene][physics]")
{
    // Planck's exp(x) - 1, for x = hc / (lambda k T) under about 1e-16, was 0, and the radiance
    // infinite. At high temperatures the spectral radiance is Rayleigh-Jeans', 2 c k T / lambda^4,
    // whose integral over a bin is 2 c k T / 3 (1 / min^3 - 1 / max^3):
    const double temperature = 1e30;
    const RGB radiance = black_body<RGB>(temperature, 1000);
    const auto bins = RGB::get_all_bins();
    for (std::size_t i = 0; i < RGB::size(); ++i) {
        INFO("bin " << i);
        const double min = bins[i].min_wavelength;
        const double max = bins[i].max_wavelength;
        const double rayleigh_jeans = 2.0 * SPEED_OF_LIGHT<double>() * K_BOLTZ<double>() *
                                      temperature / 3.0 *
                                      (1.0 / (min * min * min) - 1.0 / (max * max * max));
        CHECK(std::abs(static_cast<double>(radiance[i]) / rayleigh_jeans - 1.0) < 1e-5);
    }
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

TEST_CASE("Instances that look at each other are reported, not recursed into",
          "[scene][validation]")
{
    // Each one's orientation needed the other's, which needed its own: the stack overflowed when
    // a pose was evaluated. The cycle can only be seen then, when the poses are.
    Scene<RGB> scene;
    auto a = scene.root.new_instance(scene.new_camera_model());
    auto b = scene.root.new_instance(scene.new_camera_model());
    auto c = scene.root.new_instance(scene.new_camera_model());
    a.set_position(1_m, 0_m, 0_m);
    b.set_position(0_m, 1_m, 0_m);
    c.set_position(0_m, 0_m, 1_m);
    const Time time = Time::from_et(0.0);
    const auto pose = [&](const auto& instance) {
        return instance.get()->get_apparent_transform(
            ObservationMode::TRUE_STATE, time, time, Transform<double>{});
    };

    a.look_at(b);
    b.look_at(a);
    CHECK_THROWS(pose(a));
    CHECK_THROWS(pose(b));

    // Through a third:
    b.look_at(c);
    c.look_at(a);
    CHECK_THROWS(pose(a));

    // A chain is fine, and is evaluated again after a cycle was found:
    c.look_at(Vec3<double>{0.0, 0.0, 0.0});
    CHECK(std::isfinite(pose(a).rotation.z_axis().x));
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

TEST_CASE("A scene view is neither copied nor moved", "[scene]")
{
    // It owns its Embree scene, which it releases when it is destroyed: a copy would release it
    // a second time.
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<SceneView<RGB>>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<SceneView<RGB>>);
    STATIC_REQUIRE_FALSE(std::is_move_constructible_v<SceneView<RGB>>);
    STATIC_REQUIRE_FALSE(std::is_move_assignable_v<SceneView<RGB>>);
}

TEST_CASE("An asteroid's slope parameter may be 1, and it may be seen at any phase angle",
          "[scene][validation]")
{
    // Its H-G phase function, (1 - G) phi1 + G phi2, is positive at every phase angle short of
    // 180 degrees for G from 0 to 1, but phi2 underflows to 0 near 180 degrees, so G = 1 was
    // refused as if it were not. At 180 degrees itself (the asteroid in front of its Sun) the
    // function is 0 for any G, and the render threw for the infinite magnitude it gave.
    Scene<RGB> refusing;
    auto refusing_sun = refusing.root.new_instance(refusing.new_sun_light());
    CHECK_THROWS(refusing.new_unresolved_asteroid(15.0, 1.05, refusing_sun)); // negative at 155 deg

    // The camera looks along z at the Sun, with the asteroid in front of it; the frame is the
    // Sun's alone, the asteroid's lit side facing away.
    const auto render = [](std::optional<double> G) {
        Scene<RGB> scene;
        auto sun = scene.root.new_instance(scene.new_sun_light());
        sun.set_position(0_m, 0_m, units::Meter(2e11));
        if (G) {
            auto asteroid = scene.root.new_instance(scene.new_unresolved_asteroid(15.0, *G, sun));
            asteroid.set_position(0_m, 0_m, units::Meter(1e9));
        }
        auto camera_model = scene.new_camera_model();
        camera_model.configure_sensor_from_pitch({16, 16}, units::Micrometer(8.5));
        auto camera = scene.root.new_instance(camera_model);
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        SceneView<RGB> view(scene,
                            Interval{Time::from_et(0.0), Time::from_et(1.0)},
                            camera,
                            ObservationMode::TRUE_STATE);
        renderer.render(view, frame_buffer);
        return frame_buffer.received_power();
    };
    const Image<RGB> sun_alone = render(std::nullopt);
    for (const double G : {0.15, 1.0}) {
        INFO("G = " << G);
        Image<RGB> image;
        REQUIRE_NOTHROW(image = render(G));
        double most = 0.0;
        for (std::size_t i = 0; i < image.size(); ++i) {
            most = std::max(most, std::abs(static_cast<double>(image[i][1] - sun_alone[i][1])));
        }
        CHECK(most == 0.0);
    }
}
