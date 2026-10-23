// A shadow ray that meets a surface opaque everywhere, which transmits nothing, stops there
// without resolving the hit or evaluating the material (see SceneView::evaluate_transmittance()).
// That must change nothing: not the light, nor the random numbers the rest of the render draws.

#include <cmath>
#include <cstddef>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A sphere 10 m in front of the camera, and a wall 20 m in front of it (the near side of a
/// sphere 50 m across), lit by the Sun from behind the camera and 19 degrees to the side, so that
/// the sphere's shadow falls on the wall beside it, in view. With no sphere material, the wall
/// alone.
FrameBuffer<RGB>
render(Scene<RGB>& scene, const MaterialHandle<RGB>* sphere, const MaterialHandle<RGB>& wall)
{
    if (sphere != nullptr) {
        scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m), *sphere));
    }
    scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(50_m, 50_m, 50_m), wall))
        .set_position(0_m, 0_m, 60_m);
    scene.root.new_instance(scene.new_sun_light())
        .set_position(units::AstronomicalUnit(0.35),
                      units::AstronomicalUnit(0.0),
                      units::AstronomicalUnit(-1.0));

    auto model = scene.new_camera_model();
    model.configure_sensor_from_pitch({64, 64}, 100_um);
    model.set_focal_length(10_mm);
    model.enable_diffraction(false);
    model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(model);
    camera.set_position(0_m, 0_m, -10_m);

    auto frame_buffer = model.make_frame_buffer();
    frame_buffer.enable_received_direct_power();
    frame_buffer.enable_received_indirect_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(8);
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer;
}

} // namespace

TEST_CASE("A shadow ray stops at an opaque surface with the light it would have found",
          "[render][shadows]")
{
    // The same surfaces twice: once with the default material, which is opaque and transmits
    // nothing, so its shadow rays stop at it; and once with a transmission image of zeros,
    // which transmits nothing either, but is an image, so the material is evaluated as before.
    Scene<RGB> fast_scene;
    auto fast = fast_scene.new_material(fast_scene.new_bsdf_lambertian());
    fast.set_albedo(RGB{0.6f, 0.5f, 0.4f});
    REQUIRE(!fast.get()->has_alpha());
    REQUIRE(!fast.get()->has_transmission());
    FrameBuffer<RGB> stopped = render(fast_scene, &fast, fast);

    Scene<RGB> slow_scene;
    auto slow = slow_scene.new_material(slow_scene.new_bsdf_lambertian());
    slow.set_albedo(RGB{0.6f, 0.5f, 0.4f});
    slow.set_transmission_image(slow_scene.add_texture(Image<RGB>(2, 2, RGB{0.f})));
    REQUIRE(slow.get()->has_transmission());
    FrameBuffer<RGB> evaluated = render(slow_scene, &slow, slow);

    // The same light in every pixel, to the bit, so the same random numbers were drawn:
    const auto& a = stopped.received_direct_power();
    const auto& b = evaluated.received_direct_power();
    const auto& c = stopped.received_indirect_power();
    const auto& d = evaluated.received_indirect_power();
    int shadowed = 0;
    int lit = 0;
    for (std::size_t k = 0; k < a.size(); ++k) {
        INFO("pixel " << k);
        CHECK(a[k] == b[k]);
        CHECK(c[k] == d[k]);
        shadowed += a[k].max() == 0.f;
        lit += a[k].max() > 0.f;
    }
    // and the sphere's shadow is in view, so shadow rays did meet it:
    INFO(shadowed << " pixels in shadow, " << lit << " lit");
    CHECK(shadowed > 40);
    CHECK(lit > 40);

    // A half-transparent sphere still lets light through to its shadow, by the full path: each
    // of its two surfaces passes half the light, so its shadow gets a quarter of the light the
    // wall gets without the sphere.
    Scene<RGB> glass_scene;
    auto wall = glass_scene.new_material(glass_scene.new_bsdf_lambertian());
    wall.set_albedo(RGB{0.6f, 0.5f, 0.4f});
    auto glass = glass_scene.new_material(glass_scene.new_bsdf_lambertian());
    glass.set_alpha(0.5f);
    REQUIRE(glass.get()->has_alpha());
    FrameBuffer<RGB> through = render(glass_scene, &glass, wall);

    Scene<RGB> bare_scene;
    auto bare = bare_scene.new_material(bare_scene.new_bsdf_lambertian());
    bare.set_albedo(RGB{0.6f, 0.5f, 0.4f});
    FrameBuffer<RGB> unshadowed = render(bare_scene, nullptr, bare);

    const auto& e = through.received_direct_power();
    const auto& f = unshadowed.received_direct_power();
    constexpr int width = 64;
    const auto dark = [&](int x, int y) {
        return a[static_cast<std::size_t>(y * width + x)].max() == 0.f;
    };
    double received = 0.0;
    double unblocked = 0.0;
    int interior = 0;
    for (int y = 1; y + 1 < width; ++y) {
        for (int x = 1; x + 1 < width; ++x) {
            // a pixel the opaque sphere shadows, as it does the pixels around it:
            if (dark(x, y) && dark(x - 1, y) && dark(x + 1, y) && dark(x, y - 1) &&
                dark(x, y + 1)) {
                received += e[static_cast<std::size_t>(y * width + x)][1];
                unblocked += f[static_cast<std::size_t>(y * width + x)][1];
                ++interior;
            }
        }
    }
    INFO(interior << " pixels deep in the shadow get " << received / unblocked
                  << " of the light the wall gets without the sphere");
    REQUIRE(interior > 10);
    CHECK(unblocked > 0.0);
    CHECK(std::abs(received / unblocked - 0.25) < 0.05);
}
