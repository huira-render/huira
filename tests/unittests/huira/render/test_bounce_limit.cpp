// At the bounce limit, a path traces no ray sampled from the BSDF: the samples of lights and of
// indirect sources taken there are all there is of their light, and are counted in full rather
// than weighted against a ray that is never traced.

#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::WithinRel;

namespace {

using Radiance = units::SpectralWattsPerMeterSquaredSteradian<RGB>;

/// A white Lambertian sphere, 1 m in radius, at the origin.
void add_white_sphere(Scene<RGB>& scene)
{
    scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m),
                                                scene.new_material(scene.new_bsdf_lambertian())));
}

/// The power received from a camera 10 m along -z, looking along +z at the origin through a
/// 10 degree field of view, rendered with a bounce limit.
double received_power(Scene<RGB>& scene, int max_bounces)
{
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
    renderer.set_samples_per_pixel(256);
    renderer.set_max_bounces(max_bounces);
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

/// The white sphere under a sphere light 3 m in radius, 5 m above it. The sphere sees nothing
/// else, so all the light it reflects is direct.
double sphere_under_light(int max_bounces)
{
    Scene<RGB> scene;
    add_white_sphere(scene);
    scene.root.new_instance(scene.new_sphere_light(3_m, Radiance(RGB{1.f})))
        .set_position(0_m, 5_m, 0_m);
    return received_power(scene, max_bounces);
}

/// The white sphere lit only by a white indirect source 4 m in radius, 30 m along -z, which a
/// sphere light 8 m in radius lights from its side. A black wall hides the light from the white
/// sphere, but not the source.
double sphere_under_source(int max_bounces)
{
    Scene<RGB> scene;
    add_white_sphere(scene);
    auto source = scene.root.new_instance(scene.add_primitive(
        scene.add_ellipsoid(4_m, 4_m, 4_m), scene.new_material(scene.new_bsdf_lambertian())));
    source.set_position(0_m, 0_m, -30_m);
    source.set_indirect_source(true);
    scene.root.new_instance(scene.new_sphere_light(8_m, Radiance(RGB{1.f})))
        .set_position(14_m, 0_m, -24_m);

    auto black = scene.new_material(scene.new_bsdf_lambertian());
    black.set_albedo(RGB{0.f});
    scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(5.6_m, 10_m, 0.1_m), black))
        .set_position(8.4_m, 0_m, -15_m);
    return received_power(scene, max_bounces);
}

} // namespace

TEST_CASE("A light's samples are counted in full at the bounce limit", "[render]")
{
    // With one bounce, the camera ray's, the sphere's light comes from the light's samples alone.
    // Before, they were weighted against a BSDF ray that was never traced, and gave 9% less.
    const double reference = sphere_under_light(8);
    REQUIRE(reference > 0.0);
    CHECK_THAT(sphere_under_light(1), WithinRel(reference, 0.02));
    CHECK_THAT(sphere_under_light(2), WithinRel(reference, 0.02));
}

TEST_CASE("An indirect source's light is counted in full at the bounce limit", "[render]")
{
    // The source's radiance toward the white sphere is direct light, which a path finds in three
    // bounces: the camera ray's, at the white sphere, and at the source, whose BSDF ray can find
    // the light. With fewer, the light's samples at the source are all there is of its light,
    // whether the path reaches the source or samples it from the white sphere. Before, they were
    // weighted against that ray, and gave 3.7% less with one bounce and 3.4% less with two.
    const double reference = sphere_under_source(8);
    REQUIRE(reference > 0.0);
    CHECK_THAT(sphere_under_source(1), WithinRel(reference, 0.01));
    CHECK_THAT(sphere_under_source(2), WithinRel(reference, 0.01));
    CHECK_THAT(sphere_under_source(3), WithinRel(reference, 0.01));
}
