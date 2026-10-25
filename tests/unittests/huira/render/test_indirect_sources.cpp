// An indirect source lights what is behind a surface that lets light through, as a light does: a
// path that samples the source follows the ray toward it past such surfaces, and through the media
// between them, attenuated by what it crosses (huira-render/huira#119).

#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::WithinRel;

namespace {

/// What surrounds the receiver, a white sphere 1 m in radius at the origin: nothing, or a sphere
/// 2 m in radius that lets all light through (a Lambertian surface with an alpha of 0), or one
/// that lets half of it through (a black one with an alpha of 0.5).
enum class Shell { None, Clear, HalfOpaque };

/// A white sphere 1 m in radius at the origin, the receiver, which the camera sees from 10 m
/// along -z, filling its 10 degree field of view, and the Sun lights from far along +z, from
/// behind. Its side toward the camera is lit
/// only by a white sphere along -z, behind the camera, the reflector. `fog`, if not 0, fills the
/// shell with a medium that absorbs that much per meter.
struct Setup {
    Shell shell = Shell::None;
    double reflector_radius = 1.0e6;   // The Moon's, 1000 km ...
    double reflector_distance = 1.0e8; // ... at 100,000 km
    bool indirect_source = true;
    float fog = 0.f;
    int samples_per_pixel = 256;
};

double received_power(const Setup& setup)
{
    Scene<RGB> scene;
    auto lambertian = scene.new_bsdf_lambertian();
    scene.root.new_instance(scene.new_sun_light()).set_position(0_m, 0_m, 1_au);

    const units::Meter r(setup.reflector_radius);
    auto reflector = scene.root.new_instance(
        scene.add_primitive(scene.add_ellipsoid(r, r, r), scene.new_material(lambertian)));
    reflector.set_position(0_m, 0_m, units::Meter(-setup.reflector_distance));
    reflector.set_indirect_source(setup.indirect_source);

    scene.root.new_instance(
        scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m), scene.new_material(lambertian)));

    if (setup.shell != Shell::None) {
        auto surface = scene.new_material(lambertian);
        if (setup.shell == Shell::Clear) {
            surface.set_alpha(0.f);
        } else {
            surface.set_alpha(0.5f);
            surface.set_albedo(RGB{0.f});
        }
        auto shell = scene.add_ellipsoid(2_m, 2_m, 2_m);
        if (setup.fog > 0.f) {
            auto fog = scene.new_medium(scene.new_constant_density_field(RGB{setup.fog}, RGB{0.f}),
                                        scene.new_isotropic_phase_function());
            scene.root.new_instance(scene.add_primitive(shell, surface, fog));
        } else {
            scene.root.new_instance(scene.add_primitive(shell, surface));
        }
    }

    auto model = scene.new_camera_model();
    model.configure_sensor_from_size({32, 32}, 36_mm);
    model.set_focal_length(200_mm);
    model.enable_diffraction(false);
    model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(model);
    camera.set_position(0_m, 0_m, -10_m);

    auto frame_buffer = model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(setup.samples_per_pixel);
    SceneView<RGB> scene_view(scene,
                              Interval{Time::from_et(0.0), Time::from_et(1e-3)},
                              camera,
                              ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);

    const auto& received = frame_buffer.received_power();
    double power = 0.0;
    for (std::size_t k = 0; k < received.size(); ++k) {
        power += received[k][1];
    }
    return power;
}

} // namespace

TEST_CASE("An indirect source lights what is behind a clear surface", "[render]")
{
    // The scene of #119. Before, the ray toward the source stopped at the shell, and the source
    // gave the receiver only what a path found by sampling its BSDF: 6e-8 of its light.
    const double reference = received_power({});
    REQUIRE(reference > 0.0);
    CHECK_THAT(received_power({.shell = Shell::Clear}), WithinRel(reference, 0.03));
}

TEST_CASE("An indirect source's light is attenuated by a partly opaque surface", "[render]")
{
    // Half of the source's light reaches the receiver through the shell, and half of what the
    // receiver reflects reaches the camera: a quarter. Before, almost none.
    const double reference = received_power({});
    CHECK_THAT(received_power({.shell = Shell::HalfOpaque}), WithinRel(0.25 * reference, 0.05));
}

TEST_CASE("An indirect source's light is attenuated by the media it crosses", "[render]")
{
    // A reflector 50 m in radius, 80 m away, 39 degrees in radius from the receiver: large enough
    // that a path finds it by sampling the BSDF often, which with a plain reflector, not an
    // indirect source, is the only way, and gives the reference. The shell is clear, and filled
    // with fog that absorbs 0.3 per meter. Before, the ray toward the source stopped at the shell
    // too, and the source gave 12% less light.
    Setup setup{.shell = Shell::Clear,
                .reflector_radius = 50.0,
                .reflector_distance = 80.0,
                .indirect_source = false,
                .fog = 0.3f};
    const double reference = received_power(setup);
    REQUIRE(reference > 0.0);
    setup.indirect_source = true;
    CHECK_THAT(received_power(setup), WithinRel(reference, 0.04));
}
