#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::Visible8;

static fs::path parse_input_paths(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: moon <moon.glb_path>" << std::endl;
        std::exit(1);
    }
    fs::path moon_path = argv[1];
    return moon_path;
}


void write_image_csv(const std::string& filename, const huira::Image<float>& image) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return;
    }

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            file << image(x, y);
            if (x < image.width() - 1) {
                file << ",";
            }
        }
        file << "\n";
    }
    file.close();
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
    camera_model.set_focal_length(25_mm);
    camera_model.configure_sensor_from_size({ 1080, 1080 }, 6_mm);

    // Set camera exposure settings
    camera_model.set_fstop(18);
    camera_model.set_sensor_gain(0.8f);
    camera_model.set_sensor_bit_depth(12);
    camera_model.set_sensor_quantum_efficiency(0.6);
    camera_model.set_sensor_full_well_capacity(20000);

    // Huira uses the OpenCV convention by default, which is
    // +z forward, +y down.  Blender uses -z forward, +y up.
    // This flag allows you to match Blender's for easier
    // comparison with blender generated images.
    camera_model.use_blender_convention();

    // Disable noise
    camera_model.set_sensor_simulate_noise(false);
    camera_model.use_aperture_psf(false);

    // Create an instance of the camera and model
    auto navcam = scene.root.new_instance(camera_model);
    navcam.set_position(0_m, 10_m, 0_m);
    navcam.set_euler_angles(90_deg, 0_deg, 180_deg);


    // Load the moon model
    auto moon_model = scene.load_model(moon_path);
    moon_model.set_all_bsdfs(huira::McEwenBSDF<TSpectral>());
    //moon_model.set_all_bsdfs(huira::LambertianBSDF<TSpectral>());

    // Add moon model to the scene:
    auto moon = scene.root.new_instance(moon_model);

    // Create a local light source
    auto sun_light = scene.new_sun_light();
    auto sun = scene.root.new_instance(sun_light);
    sun.set_position(1_au, 0_m, 0_m);

    
    // Print the scene contents
    scene.print_contents();
    

    // Configure the render buffers
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.enable_albedo(); // This produces a flat, unshaded image

    // Create the renderer
    huira::Renderer<TSpectral> renderer;
    renderer.set_max_bounces(1);
    renderer.set_samples_per_pixel(100);

    // Create a scene view over the exposure interval
    auto scene_view = huira::SceneView<TSpectral>(scene, exposure_interval, navcam, huira::ObservationMode::GEOMETRIC_STATE, 1);

    // Render the current scene view
    renderer.render(scene_view, frame_buffer);

    // Save the results to PNGs
    huira::write_image_png("output/moon_render.png", frame_buffer.sensor_response(), 8);
    huira::write_image_png("output/moon_albedo.png", frame_buffer.albedo().get_channel(0), 8);

    // Save the results to CSVs:  THIS ONLY WORKS FOR MONO IMAGES
    write_image_csv("output/moon_render.csv", frame_buffer.sensor_response().get_channel(0));
    write_image_csv("output/moon_albedo.csv", frame_buffer.albedo().get_channel(0));
}
