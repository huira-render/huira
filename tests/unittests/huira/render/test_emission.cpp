#include <cmath>
#include <cstddef>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A camera 10 m from the origin, looking at it, with a field of view about 3 m across there.
struct TestCamera {
    CameraModelHandle<RGB> model;
    InstanceHandle<RGB> instance;

    // A constructor, rather than an aggregate: the handles have no default constructor, so an
    // aggregate's would be deleted, which MSVC warns about (C4623).
    TestCamera(CameraModelHandle<RGB> m, InstanceHandle<RGB> i)
        : model(std::move(m)), instance(std::move(i))
    {
    }
};

TestCamera add_camera(Scene<RGB>& scene, units::Meter x)
{
    auto model = scene.new_camera_model();
    model.configure_sensor_from_pitch({32, 32}, 100_um);
    model.set_focal_length(10_mm);
    model.enable_diffraction(false);
    model.enable_depth_of_field(false);
    auto instance = scene.root.new_instance(model);
    instance.set_position(x, 0_m, -10_m);
    return {model, instance};
}

/// Render what the camera sees, with the direct and indirect power, and the depth.
FrameBuffer<RGB> render(const Scene<RGB>& scene, const TestCamera& camera)
{
    auto frame_buffer = camera.model.make_frame_buffer();
    frame_buffer.enable_received_direct_power();
    frame_buffer.enable_received_indirect_power();
    frame_buffer.enable_depth();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(16);
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera.instance, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer;
}

bool close(float a, float b)
{
    return std::abs(a - b) <= 1e-5f * std::abs(b);
}

} // namespace

TEST_CASE("An emissive surface is seen at the radiance it emits", "[render][emission]")
{
    const RGB emission{0.5f, 1.f, 2.f};

    // A glowing sphere in the dark: no lights, and a black background.
    Scene<RGB> scene;
    auto glowing = scene.new_material(scene.new_bsdf_lambertian());
    glowing.set_emissive(emission);
    scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m), glowing));
    const TestCamera camera = add_camera(scene, 0_m);
    auto frame_buffer = render(scene, camera);

    // What the same pixels receive from a background of that radiance, with nothing in front.
    Scene<RGB> sky;
    sky.set_background_radiance(emission);
    const TestCamera sky_camera = add_camera(sky, 0_m);
    auto reference = render(sky, sky_camera);

    // Pixels entirely within the sphere receive its emission, seen directly; the corner sees
    // nothing.
    for (int x : {12, 15, 16, 19}) {
        for (int y : {12, 15, 16, 19}) {
            REQUIRE(std::isfinite(frame_buffer.depth()(x, y)));
            const RGB power = frame_buffer.received_power()(x, y);
            const RGB expected = reference.received_power()(x, y);
            for (std::size_t c = 0; c < 3; ++c) {
                CHECK(expected[c] > 0.f);
                CHECK(close(power[c], expected[c]));
                CHECK(close(frame_buffer.received_direct_power()(x, y)[c], expected[c]));
                CHECK(frame_buffer.received_indirect_power()(x, y)[c] == 0.f);
            }
        }
    }
    CHECK_FALSE(std::isfinite(frame_buffer.depth()(0, 0)));
    CHECK(frame_buffer.received_power()(0, 0).max() == 0.f);
}

TEST_CASE("An emissive surface lights the surfaces around it", "[render][emission]")
{
    // A plain sphere in the dark, in view, and a glowing one beside it and out of view.
    Scene<RGB> scene;
    auto plain = scene.new_material(scene.new_bsdf_lambertian());
    scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m), plain));

    auto glowing = scene.new_material(scene.new_bsdf_lambertian());
    glowing.set_emissive(RGB{1.f});
    auto lamp = scene.root.new_instance(
        scene.add_primitive(scene.add_ellipsoid(0.5_m, 0.5_m, 0.5_m), glowing));
    lamp.set_position(2_m, 0_m, -2_m);

    const TestCamera camera = add_camera(scene, 0_m);
    auto frame_buffer = render(scene, camera);

    // The plain sphere is seen only by the lamp's light, which has bounced off it once.
    double direct = 0.0;
    double indirect = 0.0;
    for (int y = 0; y < frame_buffer.height(); ++y) {
        for (int x = 0; x < frame_buffer.width(); ++x) {
            direct += frame_buffer.received_direct_power()(x, y).total();
            indirect += frame_buffer.received_indirect_power()(x, y).total();
        }
    }
    CHECK(direct == 0.0);
    CHECK(indirect > 0.0);
}
