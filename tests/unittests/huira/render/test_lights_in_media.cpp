// Where a path scatters in a medium, it samples the lights, and samples the phase function for
// the direction it goes on in, which can find a light too. Each light's samples are weighted
// against the phase function's, and a light the path then finds is weighted against them, from
// that point, so that between them the light is counted once.

#include <cstddef>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::WithinRel;

namespace {

using Radiance = units::SpectralWattsPerMeterSquaredSteradian<RGB>;

enum class Emitter { Light, Emissive };

/// A sphere that radiates 1 W/(m^2 sr): a sphere light, or a black sphere that emits it. Nothing
/// samples the emissive sphere: a path counts its light in full when it finds it, which makes it
/// a reference that light sampling plays no part in.
void add_emitter(Scene<RGB>& scene, Emitter emitter, double radius, double y)
{
    const units::Meter r(radius);
    if (emitter == Emitter::Light) {
        scene.root.new_instance(scene.new_sphere_light(r, Radiance(RGB{1.f})))
            .set_position(0_m, units::Meter(y), 0_m);
        return;
    }
    auto black = scene.new_material(scene.new_bsdf_lambertian());
    black.set_albedo(RGB{0.f});
    black.set_emissive(RGB{1.f});
    scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(r, r, r), black))
        .set_position(0_m, units::Meter(y), 0_m);
}

/// A ball of fog at the origin, which scatters `sigma` per meter and absorbs nothing, inside a
/// surface that lets all light through: a null BSDF with a transmission of 1, or a Lambertian
/// one with an alpha of 0.
void add_fog(Scene<RGB>& scene, double radius, float sigma, bool null_surface)
{
    auto fog = scene.new_medium(scene.new_constant_density_field(RGB{0.f}, RGB{sigma}),
                                scene.new_isotropic_phase_function());
    auto surface =
        scene.new_material(null_surface ? scene.new_bsdf_null() : scene.new_bsdf_lambertian());
    if (null_surface) {
        surface.set_transmission(RGB{1.f});
    } else {
        surface.set_alpha(0.f);
    }
    const units::Meter r(radius);
    scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(r, r, r), surface, fog));
}

/// The power received from a camera at (x, 0, -10 m), looking along +z through a 10 degree field
/// of view.
double received_power(Scene<RGB>& scene, double x)
{
    auto model = scene.new_camera_model();
    model.configure_sensor_from_size({32, 32}, 36_mm);
    model.set_focal_length(200_mm);
    model.enable_diffraction(false);
    model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(model);
    camera.set_position(units::Meter(x), 0_m, -10_m);

    auto frame_buffer = model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(256);
    renderer.set_max_bounces(32);
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

/// Fog 3 m in radius with a sphere 0.5 m in radius at its center, which the camera, 2 m off to
/// the side, does not see.
double emitter_in_fog(Emitter emitter)
{
    Scene<RGB> scene;
    add_fog(scene, 3.0, 0.3f, true);
    add_emitter(scene, emitter, 0.5, 0.0);
    return received_power(scene, 2.0);
}

/// Fog 1 m in radius, inside a surface with an alpha of 0, lit by a sphere 2.5 m in radius, 4 m
/// above its center and out of the camera's view.
double fog_under_emitter(Emitter emitter)
{
    Scene<RGB> scene;
    add_fog(scene, 1.0, 0.5f, false);
    add_emitter(scene, emitter, 2.5, 4.0);
    return received_power(scene, 0.0);
}

} // namespace

TEST_CASE("A light in a medium is counted once", "[render]")
{
    // Before, a light's samples where the path scattered were counted in full, and the light that
    // a ray sampled there found was weighted against them from where the path last met a surface,
    // which often could not see the light that way: it was counted twice, 1.94 times as much.
    const double reference = emitter_in_fog(Emitter::Emissive);
    REQUIRE(reference > 0.0);
    CHECK_THAT(emitter_in_fog(Emitter::Light), WithinRel(reference, 0.05));
}

TEST_CASE("A light outside a medium, seen through a clear surface, is counted once", "[render]")
{
    // Before, 2.0 times as much.
    const double reference = fog_under_emitter(Emitter::Emissive);
    REQUIRE(reference > 0.0);
    CHECK_THAT(fog_under_emitter(Emitter::Light), WithinRel(reference, 0.05));
}
