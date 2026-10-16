#include <cmath>
#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A vertex with a position and a normal, which it normalizes.
Vertex<RGB> vertex(const Vec3<float>& position, const Vec3<float>& normal)
{
    Vertex<RGB> v;
    v.position = position;
    v.normal = glm::normalize(normal);
    return v;
}

bool finite(const Vec3<float>& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

TEST_CASE("A mesh without tangents gives every hit an orthonormal tangent frame",
          "[geometry][mesh]")
{
    // A smooth triangle whose vertex normals lie on both sides of the z = 0 plane, where the
    // default frame's tangent flips: per-vertex frames would interpolate to zero across it.
    // Without a tangent buffer, the mesh used to leave the tangent at zero, which resolving the
    // hit normalized into NaN.
    const Mesh<RGB> mesh(IndexBuffer{0, 1, 2},
                         VertexBuffer<RGB>{
                             vertex({0.f, -1.f, 0.f}, {1.f, -0.2f, 0.2f}),
                             vertex({0.f, 1.f, 0.f}, {1.f, 0.2f, -0.2f}),
                             vertex({0.f, 0.f, 1.f}, {0.8f, 0.f, 0.6f}),
                         });
    REQUIRE_FALSE(mesh.has_tangents());

    for (float u = 0.f; u <= 1.f; u += 0.125f) {
        for (float v = 0.f; u + v <= 1.f; v += 0.125f) {
            HitRecord hit;
            hit.prim_id = 0;
            hit.u = u;
            hit.v = v;
            Interaction<RGB> isect;
            mesh.compute_surface_interaction(hit, isect);

            const Vec3<float> n = glm::normalize(isect.normal_s);
            REQUIRE(finite(isect.tangent));
            REQUIRE(finite(isect.bitangent));
            CHECK(std::abs(glm::length(isect.tangent) - 1.f) < 1e-5f);
            CHECK(std::abs(glm::length(isect.bitangent) - 1.f) < 1e-5f);
            CHECK(std::abs(glm::dot(isect.tangent, n)) < 1e-5f);
            CHECK(std::abs(glm::dot(isect.bitangent, n)) < 1e-5f);
            CHECK(std::abs(glm::dot(isect.tangent, isect.bitangent)) < 1e-5f);
        }
    }
}

TEST_CASE("A mesh without tangents renders", "[geometry][mesh][nan]")
{
    // A square of two triangles, made with add_mesh() and no tangents, facing a camera with the
    // Sun behind it. It used to render NaN wherever it was seen.
    Scene<RGB> scene;
    const VertexBuffer<RGB> vertices{
        vertex({-1.f, -1.f, 0.f}, {0.f, 0.f, -1.f}),
        vertex({1.f, -1.f, 0.f}, {0.f, 0.f, -1.f}),
        vertex({1.f, 1.f, 0.f}, {0.f, 0.f, -1.f}),
        vertex({-1.f, 1.f, 0.f}, {0.f, 0.f, -1.f}),
    };
    auto square = scene.add_mesh(IndexBuffer{0, 2, 1, 0, 3, 2}, vertices);
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    scene.root.new_instance(scene.add_primitive(square, material));

    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, 0_m, -1_au);

    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({32, 32}, 10_um);
    camera_model.set_focal_length(50_mm);
    camera_model.enable_diffraction(false);
    camera_model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(camera_model);
    camera.set_position(0_m, 0_m, -5_m); // the square fills the frame

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(4);
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);

    const Image<RGB>& power = frame_buffer.received_power();
    std::size_t lit = 0;
    for (std::size_t i = 0; i < power.size(); ++i) {
        REQUIRE(std::isfinite(power[i][0]));
        REQUIRE(std::isfinite(power[i][1]));
        REQUIRE(std::isfinite(power[i][2]));
        if (power[i][1] > 0.f) {
            ++lit;
        }
    }
    CHECK(lit == power.size());
}
