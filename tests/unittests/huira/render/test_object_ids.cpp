#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <tuple>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A glTF model with one 1 m square mesh, facing -z, placed by two nodes at x = -1 m and x = 1 m:
/// one mesh, two parts.
std::filesystem::path two_panel_model()
{
    const auto path = std::filesystem::temp_directory_path() / "huira_test_two_panels.gltf";
    std::ofstream(path) << R"({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0, 1]}],
  "nodes": [
    {"name": "PanelLeft", "mesh": 0, "translation": [-1.0, 0.0, 0.0]},
    {"name": "PanelRight", "mesh": 0, "translation": [1.0, 0.0, 0.0]}
  ],
  "meshes": [{"name": "Panel", "primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
  "buffers": [{
    "byteLength": 60,
    "uri": "data:application/octet-stream;base64,AAAAvwAAAL8AAAAAAAAAPwAAAL8AAAAAAAAAPwAAAD8AAAAAAAAAvwAAAD8AAAAAAAABAAIAAAACAAMA"
  }],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 48},
    {"buffer": 0, "byteOffset": 48, "byteLength": 12}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
     "min": [-0.5, -0.5, 0.0], "max": [0.5, 0.5, 0.0]},
    {"bufferView": 1, "componentType": 5123, "count": 6, "type": "SCALAR"}
  ]
})";
    return path;
}

using IdTuple = std::tuple<std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t>;

} // namespace

TEST_CASE("The frame buffer records the geometry, model, instance and part each pixel sees",
          "[render][frame_buffer][ids]")
{
    // The two-panel model, placed twice (four panels), and a sphere placed directly.
    Scene<RGB> scene;
    auto model = scene.load_model(two_panel_model());
    auto upper = scene.root.new_instance(model);
    upper.set_position(0_m, 1.5_m, 0_m);
    auto lower = scene.root.new_instance(model);
    lower.set_position(0_m, -1.5_m, 0_m);

    auto ellipsoid = scene.add_ellipsoid(0.5_m, 0.5_m, 0.5_m);
    auto ball = scene.root.new_instance(
        scene.add_primitive(ellipsoid, scene.new_material(scene.new_bsdf_lambertian())));
    ball.set_position(3.5_m, 0_m, 0_m);

    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, 0_m, -1_au);

    // About 10 m across at the scene, with room around everything.
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({128, 128}, 80_um);
    camera_model.set_focal_length(10_mm);
    camera_model.enable_diffraction(false);
    camera_model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(camera_model);
    camera.set_position(0_m, 0_m, -10_m);

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_geometry_ids();
    frame_buffer.enable_model_ids();
    frame_buffer.enable_instance_ids();
    frame_buffer.enable_part_ids();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(4);
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);

    // Every distinct set of IDs seen, and how many pixels see it.
    std::map<IdTuple, std::size_t> seen;
    for (int y = 0; y < frame_buffer.height(); ++y) {
        for (int x = 0; x < frame_buffer.width(); ++x) {
            ++seen[{frame_buffer.geometry_ids()(x, y),
                    frame_buffer.model_ids()(x, y),
                    frame_buffer.instance_ids()(x, y),
                    frame_buffer.part_ids()(x, y)}];
        }
    }

    // The background, the sphere, and four panels.
    REQUIRE(seen.size() == 6);
    CHECK(seen.count({NO_OBJECT_ID, NO_OBJECT_ID, NO_OBJECT_ID, NO_OBJECT_ID}) == 1);
    CHECK(seen.count({ellipsoid.id(), NO_OBJECT_ID, ball.id(), ball.id()}) == 1);

    std::set<std::uint64_t> panel_geometries;
    std::set<std::uint64_t> panel_instances;
    std::set<std::uint64_t> panel_parts;
    std::set<std::pair<std::uint64_t, std::uint64_t>> placements;
    for (const auto& [ids, pixels] : seen) {
        const auto& [geometry, model_id, instance, part] = ids;
        if (geometry == NO_OBJECT_ID || geometry == ellipsoid.id()) {
            continue;
        }
        CHECK(pixels > 10);
        CHECK(model_id == model.id());
        panel_geometries.insert(geometry);
        panel_instances.insert(instance);
        panel_parts.insert(part);
        placements.insert({instance, part});
    }

    // One mesh, placed twice, each time as two parts: the parts tell the panels within a
    // placement apart, and the instances tell the placements apart.
    CHECK(panel_geometries.size() == 1);
    const std::set<std::uint64_t> placed{upper.id(), lower.id()};
    CHECK(panel_instances == placed);
    CHECK(panel_parts.size() == 2);
    CHECK(placements.size() == 4);
    CHECK(panel_parts.count(upper.id()) == 0);
    CHECK(panel_parts.count(lower.id()) == 0);

    // A scene view answers the same for a single ray: one from the camera to the sphere's centre,
    // in the camera's frame, which the scene view works in.
    const Ray<RGB> ray(Vec3<float>{0.f}, glm::normalize(Vec3<float>{3.5f, 0.f, 10.f}));
    const HitRecord hit = scene_view.intersect(ray);
    REQUIRE(hit.hit());
    const ObjectIds ball_ids{ellipsoid.id(), NO_OBJECT_ID, ball.id(), ball.id()};
    CHECK(scene_view.object_ids(hit) == ball_ids);
    CHECK(scene_view.object_ids(HitRecord{}) == ObjectIds{});

    // Clearing a frame buffer leaves nothing seen.
    frame_buffer.clear();
    CHECK(frame_buffer.geometry_ids()(0, 0) == NO_OBJECT_ID);
    CHECK(frame_buffer.model_ids()(0, 0) == NO_OBJECT_ID);
    CHECK(frame_buffer.instance_ids()(0, 0) == NO_OBJECT_ID);
    CHECK(frame_buffer.part_ids()(0, 0) == NO_OBJECT_ID);
}
