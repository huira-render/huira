#include "huira/huira.hpp"

#include <filesystem>
#include <iostream>
#include <utility>

using namespace std;
using namespace std::chrono;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

namespace fs = std::filesystem;

static fs::path parse_input_paths(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: lunar_gateway <gateway.glb_path>" << std::endl;
        std::exit(1);
    }
    fs::path gateway_path = argv[1];
    return gateway_path;
}

int main(int argc, char** argv) {
    // Parsing input paths:
    fs::path gateway_path = parse_input_paths(argc, argv);

    // Create the scene:
    huira::Scene<TSpectral> scene;

    // Specify the time:
    huira::Time time("2019-02-06T10:27:00");
    float exposure_time = 1.f;

    // Configure a camera model:
    auto camera_model = scene.new_camera_model();
    camera_model.use_blender_convention();
    camera_model.set_focal_length(50_mm);
    camera_model.set_sensor_resolution(1920, 1080);
    camera_model.set_sensor_size(36_mm);
    camera_model.use_aperture_psf();

    // Create an instance of the camera and model:
    auto navcam = scene.root.new_instance(camera_model);
    navcam.set_position(40_m, -40_m, 0_m);
    navcam.set_euler_angles(90_deg, 0_deg, 30_deg);

    // Load stars:
    auto gateway_model = scene.load_model(gateway_path);
    auto gateway = scene.root.new_instance(gateway_model);

    // Create some local light source:
    auto point_light = scene.new_point_light(5000000_W);
    auto light = scene.root.new_instance(point_light);
    light.set_position(0_m, -200_m, 50_m);

    // Print the scene contents:
    scene.print_contents();
    

    // Configure the render buffers:
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_depth();
    frame_buffer.enable_albedo();
    frame_buffer.enable_camera_normals();
    frame_buffer.enable_received_power();
    frame_buffer.enable_sensor_response();

    // Create the renderer:
    huira::RasterRenderer<TSpectral> renderer;

    auto scene_view = huira::SceneView<TSpectral>(scene, time, navcam, huira::ObservationMode::ABERRATED_STATE);

    // Render the current scene view:
    renderer.render(scene_view, frame_buffer, exposure_time);

    // Save the results:
    huira::write_image_png("output/gateway_render.png", frame_buffer.sensor_response(), 8);
    huira::write_image_png("output/gateway_albedo.png", frame_buffer.albedo(), 8);

    // Convert normals to RGB for visualization:
    auto normals_vec3 = frame_buffer.camera_normals();
    huira::Image<huira::RGB> normals(normals_vec3.resolution());
    for (std::size_t i = 0; i < normals.size(); ++i) {
        const auto& n = normals_vec3[i];
        normals[i] = huira::RGB{
            static_cast<float>(n.x * 0.5f + 0.5f),
            static_cast<float>(n.y * 0.5f + 0.5f),
            static_cast<float>(n.z * 0.5f + 0.5f)
        };
    }

    huira::write_image_png("output/gateway_normals.png", normals, 8);
}
