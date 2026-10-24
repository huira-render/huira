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

TEST_CASE("A hit where the vertex normals give no direction is shaded flat",
          "[geometry][mesh][nan]")
{
    // Vertices given no normal (Vertex's default is 0), or normals that cancel where they are
    // interpolated, were normalized into NaN, which every pixel the mesh covered became. The
    // triangle's own normal is used there instead: here, along z.
    const auto check_flat = [](const Mesh<RGB>& mesh, float u, float v) {
        HitRecord hit;
        hit.prim_id = 0;
        hit.u = u;
        hit.v = v;
        Interaction<RGB> isect;
        mesh.compute_surface_interaction(hit, isect);
        const Vec3<float> n = glm::normalize(isect.normal_s);
        REQUIRE(finite(n));
        CHECK(std::abs(std::abs(n.z) - 1.f) < 1e-6f);
        CHECK(finite(isect.tangent));
        CHECK(finite(isect.bitangent));
    };

    VertexBuffer<RGB> unset(3);
    unset[1].position = Vec3<float>{1.f, 0.f, 0.f};
    unset[2].position = Vec3<float>{0.f, 1.f, 0.f};
    check_flat(Mesh<RGB>(IndexBuffer{0, 1, 2}, unset), 0.25f, 0.25f);

    const Mesh<RGB> opposed(IndexBuffer{0, 1, 2},
                            VertexBuffer<RGB>{
                                vertex({0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}),
                                vertex({1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}),
                                vertex({0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}),
                            });
    check_flat(opposed, 0.5f, 0.f); // halfway between the first two
}

TEST_CASE("Interpolated tangents that cancel or lie along the normal get a frame anyway",
          "[geometry][mesh]")
{
    // A flat triangle facing +z. Its vertex tangents are a valid frame at each corner, but
    // two of them point the other way, as at a UV seam, so the interpolated tangent passes
    // through zero inside the triangle: at the point where the weights are 1/2, 1/4, 1/4 it is
    // (1/2) t + (1/4)(-t) + (1/4)(-t) = 0.
    const Vec3<float> n{0.f, 0.f, 1.f};
    const Vec3<float> t{1.f, 0.f, 0.f};
    const Vec3<float> b = glm::cross(n, t);
    const Mesh<RGB> mesh(IndexBuffer{0, 1, 2},
                         VertexBuffer<RGB>{
                             vertex({0.f, 0.f, 0.f}, n),
                             vertex({1.f, 0.f, 0.f}, n),
                             vertex({0.f, 1.f, 0.f}, n),
                         },
                         TangentBuffer{{t, b}, {-t, -b}, {-t, -b}});
    REQUIRE(mesh.has_tangents());

    auto frame_at = [&](float u, float v) {
        HitRecord hit;
        hit.prim_id = 0;
        hit.u = u;
        hit.v = v;
        Interaction<RGB> isect;
        mesh.compute_surface_interaction(hit, isect);
        return isect;
    };

    auto check_frame = [&](const Interaction<RGB>& isect) {
        REQUIRE(finite(isect.tangent));
        REQUIRE(finite(isect.bitangent));
        CHECK(std::abs(glm::length(isect.tangent) - 1.f) < 1e-5f);
        CHECK(std::abs(glm::length(isect.bitangent) - 1.f) < 1e-5f);
        CHECK(std::abs(glm::dot(isect.tangent, n)) < 1e-5f);
        CHECK(std::abs(glm::dot(isect.bitangent, n)) < 1e-5f);
        CHECK(std::abs(glm::dot(isect.tangent, isect.bitangent)) < 1e-5f);
    };

    // At the cancellation, and all around it:
    check_frame(frame_at(0.25f, 0.25f));
    for (float u = 0.f; u <= 1.f; u += 0.125f) {
        for (float v = 0.f; u + v <= 1.f; v += 0.125f) {
            check_frame(frame_at(u, v));
        }
    }

    // Away from it, the frame is the buffer's, normalized, with its handedness:
    const Interaction<RGB> corner = frame_at(0.f, 0.f);
    CHECK(glm::dot(corner.tangent, t) > 0.999f);
    CHECK(glm::dot(corner.bitangent, b) > 0.999f);
    const Interaction<RGB> flipped = frame_at(1.f, 0.f);
    CHECK(glm::dot(flipped.tangent, -t) > 0.999f);
    CHECK(glm::dot(flipped.bitangent, -b) > 0.999f);

    // A tangent with a component along the normal is projected into the surface; one lying
    // entirely along the normal is no tangent, and gets a frame built instead:
    const Mesh<RGB> tilted(IndexBuffer{0, 1, 2},
                           VertexBuffer<RGB>{
                               vertex({0.f, 0.f, 0.f}, n),
                               vertex({1.f, 0.f, 0.f}, n),
                               vertex({0.f, 1.f, 0.f}, n),
                           },
                           TangentBuffer{{n, b}, {n, b}, {t + n, b}});
    HitRecord hit;
    hit.prim_id = 0;
    hit.u = 0.f;
    hit.v = 0.f;
    Interaction<RGB> along_normal;
    tilted.compute_surface_interaction(hit, along_normal);
    check_frame(along_normal);
    hit.v = 1.f;
    Interaction<RGB> projected;
    tilted.compute_surface_interaction(hit, projected);
    check_frame(projected);
    CHECK(glm::dot(projected.tangent, t) > 0.999f);
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
