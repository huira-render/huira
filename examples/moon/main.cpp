#include "huira/huira.hpp"

#include <filesystem>
#include <iostream>
#include <utility>

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

static fs::path parse_input_paths(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: moon <moon.glb_path>" << std::endl;
        std::exit(1);
    }
    fs::path moon_path = argv[1];
    return moon_path;
}

int main(int argc, char** argv) {
    // Parsing input paths
    fs::path moon_path = parse_input_paths(argc, argv);

    // Create the scene
    huira::Scene<TSpectral> scene;

    // Set the observation time
    huira::Time time("2019-02-06T10:27:00");
    huira::Interval exposure_interval{ time, time + 0.00025_s };

    // Configure a camera model
    auto camera_model = scene.new_camera_model();
    camera_model.use_blender_convention();
    camera_model.set_focal_length(25_mm);
    camera_model.set_sensor_resolution(1920, 1080);
    camera_model.set_sensor_size(16_mm);
    camera_model.use_aperture_psf();
    camera_model.set_sensor_bias_level(10.f);
    camera_model.set_fstop(16);
    //camera_model.enable_depth_of_field();
    //camera_model.set_focus_distance(1_m);

    // Create an instance of the camera and model
    auto navcam = scene.root.new_instance(camera_model);
    navcam.set_position(0_m, 8000_Km, 0_m);
    navcam.set_euler_angles(90_deg, 0_deg, 180_deg);
    //navcam.set_body_angular_velocity(0_deg / 1_s, 0_deg / 1_s, 10_deg / 1_s);

    // Load the moon model
    auto moon_model = scene.load_model(moon_path);
    auto moon = scene.root.new_instance(moon_model);
    moon.set_scale(1000); // model is in units of km, not meters

    // Create a local light source
    auto sun_light = scene.new_sun_light();
    auto sun = scene.root.new_instance(sun_light);
    sun.set_position(1_au, 0_m, 0_m);

    scene.set_background_radiance(1e-7f);

    // Print the scene contents
    scene.print_contents();
    

    // Configure the render buffers
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();

    // Create the renderer
    huira::Renderer<TSpectral> renderer;
    renderer.set_max_bounces(1);
    renderer.set_samples_per_pixel(10);
    renderer.set_indirect_clamp(10.0f);

    // Create a scene view over the exposure interval
    auto scene_view = huira::SceneView<TSpectral>(scene, exposure_interval, navcam, huira::ObservationMode::GEOMETRIC_STATE, 3);

    // Render the current scene view
    renderer.render(scene_view, frame_buffer);

    // Save the results
    huira::write_image_png("output/moon_render.png", frame_buffer.sensor_response(), 8);
}
