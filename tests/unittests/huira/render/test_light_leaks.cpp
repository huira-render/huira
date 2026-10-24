// An opaque surface lets no light through, whatever its shading normals: a path that meets it
// ends on its near side, though the BSDF, sampled around a shading normal tilted from the
// geometric one, can face directions beyond it (see Renderer, after bsdf_sample()).

#include <cmath>
#include <cstddef>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

struct Received {
    double direct = 0.0;
    double indirect = 0.0;
};

/// A 40 m square at z = 0, its front toward a camera at z = -1 m that sees nothing else, its
/// vertex normals tilted by `tilt` rad from its geometric normal; and a sphere 15 m in radius
/// wholly behind its plane (z from 1 to 31 m), off to its +x side, where light leaking through a
/// square tilted toward +x would go. The Sun, from -x and +z, lights the square's back and the
/// side of the sphere that faces it, and the square's front only if `sun_in_front`.
Received render(float tilt, bool sphere_is_indirect_source, bool sun_in_front)
{
    Scene<RGB> scene;
    VertexBuffer<RGB> vertices;
    for (auto [x, y] : {std::pair{-20.f, -20.f}, {20.f, -20.f}, {20.f, 20.f}, {-20.f, 20.f}}) {
        Vertex<RGB> vertex;
        vertex.position = {x, y, 0.f};
        vertex.normal = {std::sin(tilt), 0.f, -std::cos(tilt)};
        vertices.push_back(vertex);
    }
    auto square = scene.new_material(scene.new_bsdf_lambertian());
    square.set_albedo(RGB{0.5f});
    scene.root.new_instance(
        scene.add_primitive(scene.add_mesh(IndexBuffer{0, 1, 2, 0, 2, 3}, vertices), square));

    auto sphere = scene.new_material(scene.new_bsdf_lambertian());
    sphere.set_albedo(RGB{1.f});
    auto ball =
        scene.root.new_instance(scene.add_primitive(scene.add_ellipsoid(15_m, 15_m, 15_m), sphere));
    ball.set_position(35_m, 0_m, 16_m);
    if (sphere_is_indirect_source) {
        ball.set_indirect_source(true);
    }
    scene.root.new_instance(scene.new_sun_light())
        .set_position(units::AstronomicalUnit(-0.7),
                      units::AstronomicalUnit(0.0),
                      units::AstronomicalUnit(sun_in_front ? -0.7 : 0.7));

    auto model = scene.new_camera_model();
    model.configure_sensor_from_pitch({32, 32}, 20_um);
    model.set_focal_length(50_mm);
    model.enable_diffraction(false);
    model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(model);
    camera.set_position(0_m, 0_m, -1_m);

    auto frame_buffer = model.make_frame_buffer();
    frame_buffer.enable_received_direct_power();
    frame_buffer.enable_received_indirect_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(16);
    SceneView<RGB> scene_view(scene,
                              Interval{Time::from_et(0.0), Time::from_et(1e-3)},
                              camera,
                              ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);

    Received received;
    const auto& direct = frame_buffer.received_direct_power();
    const auto& indirect = frame_buffer.received_indirect_power();
    for (std::size_t k = 0; k < direct.size(); ++k) {
        received.direct += direct[k][1];
        received.indirect += indirect[k][1];
    }
    return received;
}

} // namespace

TEST_CASE("No light reaches an opaque surface through it, whatever its shading normals", "[render]")
{
    for (bool indirect_source : {false, true}) {
        // With the sphere an indirect source, the square samples it directly as well, by a probe
        // ray that must not start beyond the square either.
        INFO((indirect_source ? "sphere sampled as an indirect source" : "sphere a plain surface"));

        // The camera sees the square's front, which the Sun lights when it is in front:
        CHECK(render(0.7f, indirect_source, true).direct > 0.0);

        // Lit only from behind, the front receives nothing. Before, light came through the
        // square tilted 0.4 and 0.7 rad, to 13% of what the Sun gives its front at 0.7 rad.
        for (float tilt : {0.f, 0.4f, 0.7f}) {
            INFO("vertex normals tilted " << tilt << " rad");
            const Received received = render(tilt, indirect_source, false);
            CHECK(received.direct == 0.0);
            CHECK(received.indirect == 0.0);
        }
    }
}
