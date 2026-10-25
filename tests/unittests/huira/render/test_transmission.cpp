// A surface's transmission is the fraction of the light meeting it that passes straight through.
// A path passes the surface that way, or scatters there by its BSDF, at random (always the former
// with the null BSDF, which reflects nothing), as shadow rays pass it: so a light, or an emissive
// surface, is seen through it, and lights what is behind it, as much as its transmission lets.

#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::WithinRel;

namespace {

using Radiance = units::SpectralWattsPerMeterSquaredSteradian<RGB>;

enum class Emitter { Light, Emissive };

/// A shell around the origin: none, or a null BSDF with a transmission, or a black Lambertian
/// surface with one.
enum class Shell { None, Null, Black };

/// A sphere at (0, y, 0) that radiates 1 W/(m^2 sr): a sphere light, or a black sphere that emits
/// it, which nothing samples, so that a path counts its light in full when it finds it.
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

void add_shell(Scene<RGB>& scene, Shell shell, double radius, float transmission)
{
    if (shell == Shell::None) {
        return;
    }
    auto surface = scene.new_material(shell == Shell::Null ? scene.new_bsdf_null()
                                                           : scene.new_bsdf_lambertian());
    surface.set_transmission(RGB{transmission});
    surface.set_albedo(RGB{0.f});
    const units::Meter r(radius);
    scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(r, r, r), surface));
}

void add_white_sphere(Scene<RGB>& scene)
{
    scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m),
                                                scene.new_material(scene.new_bsdf_lambertian())));
}

struct Received {
    double direct = 0.0;
    double total = 0.0;
};

/// The power received from a camera at z (in meters) on the z axis, looking along +z through a
/// 10 degree field of view.
Received render(Scene<RGB>& scene, double z)
{
    auto model = scene.new_camera_model();
    model.configure_sensor_from_size({32, 32}, 36_mm);
    model.set_focal_length(200_mm);
    model.enable_diffraction(false);
    model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(model);
    camera.set_position(0_m, 0_m, units::Meter(z));

    auto frame_buffer = model.make_frame_buffer();
    frame_buffer.enable_received_direct_power();
    frame_buffer.enable_received_indirect_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(256);
    SceneView<RGB> scene_view(scene,
                              Interval{Time::from_et(0.0), Time::from_et(1e-3)},
                              camera,
                              ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);

    const auto& direct = frame_buffer.received_direct_power();
    const auto& indirect = frame_buffer.received_indirect_power();
    Received received;
    for (std::size_t k = 0; k < direct.size(); ++k) {
        received.direct += direct[k][1];
        received.total += direct[k][1] + indirect[k][1];
    }
    return received;
}

/// An emitter 0.5 m in radius at the origin, seen from 10 m through a shell 2 m in radius.
Received emitter_through_shell(Emitter emitter, Shell shell, float transmission = 1.f)
{
    Scene<RGB> scene;
    add_emitter(scene, emitter, 0.5, 0.0);
    add_shell(scene, shell, 2.0, transmission);
    return render(scene, -10.0);
}

/// The white sphere inside a shell 1.5 m in radius, lit by an emitter 3 m in radius, 5 m above it
/// and out of the view of the camera, 10 m away. With camera_inside, the shell is 12 m in radius,
/// around the camera too, and the emitter 17 m in radius, 30 m above.
Received sphere_in_shell(Emitter emitter, Shell shell, bool camera_inside)
{
    Scene<RGB> scene;
    add_white_sphere(scene);
    if (camera_inside) {
        add_shell(scene, shell, 12.0, 1.f);
        add_emitter(scene, emitter, 17.0, 30.0);
    } else {
        add_shell(scene, shell, 1.5, 1.f);
        add_emitter(scene, emitter, 3.0, 5.0);
    }
    return render(scene, -10.0);
}

} // namespace

TEST_CASE("A light is seen through a surface that transmits its light", "[render]")
{
    const double light = emitter_through_shell(Emitter::Light, Shell::None).total;
    REQUIRE(light > 0.0);
    CHECK_THAT(emitter_through_shell(Emitter::Emissive, Shell::None).total, WithinRel(light, 0.01));

    // Before, a path that crossed a null-BSDF surface weighted a light it then found at 0, as
    // though the surface's BSDF were a mirror that light sampling could not find: the light was
    // black. And the camera ray's crossing counted as a bounce, which put it in the indirect light.
    const Received null = emitter_through_shell(Emitter::Light, Shell::Null);
    CHECK_THAT(null.total, WithinRel(light, 0.01));
    CHECK_THAT(null.direct, WithinRel(light, 0.01));
    CHECK_THAT(emitter_through_shell(Emitter::Light, Shell::Null, 0.5f).total,
               WithinRel(0.5 * light, 0.01));

    // A surface that reflects (if only black) is passed by its transmission or met, at random.
    // Before, a path never passed it: the light was hidden.
    CHECK_THAT(emitter_through_shell(Emitter::Light, Shell::Black).total, WithinRel(light, 0.03));
    CHECK_THAT(emitter_through_shell(Emitter::Emissive, Shell::Black).total,
               WithinRel(light, 0.03));
}

TEST_CASE("A light lights what is behind a surface that transmits its light", "[render]")
{
    // The light's samples reach the white sphere through the shell, and so does a path that
    // samples the sphere's BSDF; between them they count the light once, as without the shell.
    const double reference = sphere_in_shell(Emitter::Light, Shell::None, false).total;
    REQUIRE(reference > 0.0);

    // Before, a path that crossed the null-BSDF shell weighted the light it then found at 0, and
    // the sphere received 9% less light. Its crossings counted as bounces too, which with the
    // default limit of three left the emissive sphere out of its reach.
    CHECK_THAT(sphere_in_shell(Emitter::Light, Shell::Null, false).total,
               WithinRel(reference, 0.02));
    CHECK_THAT(sphere_in_shell(Emitter::Emissive, Shell::Null, false).total,
               WithinRel(reference, 0.02));

    // A black shell that transmits all light, around the camera and the sphere: before, a path
    // never passed it, so the emissive sphere gave no light at all, and the light 5% less.
    const double inside = sphere_in_shell(Emitter::Light, Shell::None, true).total;
    REQUIRE(inside > 0.0);
    CHECK_THAT(sphere_in_shell(Emitter::Light, Shell::Black, true).total, WithinRel(inside, 0.02));
    CHECK_THAT(sphere_in_shell(Emitter::Emissive, Shell::Black, true).total,
               WithinRel(inside, 0.04));
}
