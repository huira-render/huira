#include <filesystem>
#include <fstream>
#include <memory>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

// Nodes made through the API used to be impossible to delete: deleting one tried to remove it
// from the scene's registry of named nodes, which only holds nodes loaded from model files, and
// threw "does not exist in the scene" without deleting anything. Deleting an asset that had an
// instance in the scene graph failed the same way.

TEST_CASE("An instance made through the API can be deleted", "[scene][deletion]")
{
    Scene<RGB> scene;
    auto light = scene.new_sun_light();
    auto instance = scene.root.new_instance(light);
    REQUIRE(instance.valid());

    CHECK_NOTHROW(scene.root.delete_instance(instance));
    CHECK_FALSE(instance.valid());
    CHECK(light.valid()); // the asset stays
}

TEST_CASE("A subframe made through the API can be deleted, with everything in it",
          "[scene][deletion]")
{
    Scene<RGB> scene;
    auto frame = scene.root.new_subframe();
    auto inner = frame.new_subframe();
    auto instance = inner.new_instance(scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m)));
    REQUIRE(instance.valid());

    CHECK_NOTHROW(scene.root.delete_subframe(frame));
    CHECK_FALSE(frame.valid());
    CHECK_FALSE(inner.valid());
    CHECK_FALSE(instance.valid());

    // Deleting it again is an error, but not the old one:
    CHECK_THROWS(scene.root.delete_subframe(frame));
}

TEST_CASE("An asset with instances in the scene graph can be deleted", "[scene][deletion]")
{
    Scene<RGB> scene;
    auto light = scene.new_sun_light();
    auto frame = scene.root.new_subframe();
    auto instance = frame.new_instance(light);

    CHECK_NOTHROW(scene.delete_light(light));
    CHECK_FALSE(light.valid());
    CHECK_FALSE(instance.valid());
    CHECK(frame.valid());

    auto primitive = scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m));
    auto primitive_instance = scene.root.new_instance(primitive);
    CHECK_NOTHROW(scene.delete_primitive(primitive));
    CHECK_FALSE(primitive_instance.valid());
}

namespace {

/// A model file with a single triangle mesh, named "Tri".
std::filesystem::path triangle_model()
{
    const auto path = std::filesystem::temp_directory_path() / "huira_test_triangle.obj";
    std::ofstream(path) << "o Tri\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    return path;
}

/// A small camera, with nothing but its geometry.
struct Camera {
    CameraModelHandle<RGB> model;
    InstanceHandle<RGB> instance;
};

Camera small_camera(Scene<RGB>& scene)
{
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({16, 16}, 0.5_mm, 0.5_mm);
    camera_model.delete_psf();
    camera_model.enable_depth_of_field(false);
    return {camera_model, scene.root.new_instance(camera_model)};
}

/// Render a small frame through the camera.
void render_through(Scene<RGB>& scene, const Camera& camera)
{
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera.instance, ObservationMode::TRUE_STATE);
    auto frame_buffer = camera.model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(1);
    renderer.render(scene_view, frame_buffer);
}

} // namespace

TEST_CASE("A node looking at a deleted node reports it", "[scene][deletion]")
{
    // look_at() kept a raw pointer to its target, so deleting the target left the camera
    // reading freed memory on the next render.
    Scene<RGB> scene;
    auto camera = small_camera(scene);
    auto frame = scene.root.new_subframe();
    auto target = frame.new_instance(scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m)));
    target.set_position(0_m, 0_m, 10_m);
    camera.instance.look_at(target);
    CHECK_NOTHROW(render_through(scene, camera));

    scene.root.delete_subframe(frame);
    CHECK_THROWS(render_through(scene, camera));

    // Looking somewhere else works again:
    camera.instance.look_at(Vec3<double>{0.0, 0.0, 10.0});
    CHECK_NOTHROW(render_through(scene, camera));
}

TEST_CASE("Deleting a loaded model's primitive removes it from the model", "[scene][deletion]")
{
    // Only the scene's own graph was pruned, so the model's graph kept an instance of the
    // deleted primitive, and rendering the model used freed memory.
    Scene<RGB> scene;
    auto camera = small_camera(scene);
    auto model = scene.load_model(triangle_model());
    auto placed = scene.root.new_instance(model);
    placed.set_position(0_m, 0_m, 10_m);
    CHECK_NOTHROW(render_through(scene, camera));

    auto primitive = scene.get_primitive("Tri");
    REQUIRE(primitive.valid());
    scene.delete_primitive(primitive);
    CHECK(placed.valid()); // the model and its instance stay
    CHECK_NOTHROW(render_through(scene, camera));
}

TEST_CASE("A loaded model can be deleted and loaded again", "[scene][deletion]")
{
    Scene<RGB> scene;
    auto camera = small_camera(scene);
    auto model = scene.load_model(triangle_model(), "triangle");
    auto placed = scene.root.new_instance(model);

    std::weak_ptr<Model<RGB>> weak_model = model.get();
    scene.delete_model(model);
    CHECK_FALSE(model.valid());
    CHECK_FALSE(placed.valid());
    CHECK(weak_model.expired());
    CHECK_NOTHROW(render_through(scene, camera));

    auto again = scene.load_model(triangle_model(), "triangle");
    CHECK(again.valid());
    scene.root.new_instance(again).set_position(0_m, 0_m, 10_m);
    CHECK_NOTHROW(render_through(scene, camera));
}
