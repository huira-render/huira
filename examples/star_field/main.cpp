#include "huira/huira.hpp"

#include <filesystem>
#include <iostream>
#include <utility>

using Deg = huira::units::Degree;
using Rad = huira::units::Radian;

namespace fs = std::filesystem;

std::pair<fs::path, fs::path> parse_input_paths(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: star_field <tycho2.hrsc_path> <kernel_path>" << std::endl;
        std::exit(1);
    }
    fs::path star_catalog_path = argv[1];
    fs::path kernel_path = argv[2];
    return {star_catalog_path, kernel_path};
}

int main(int argc, char** argv) {
    // Parsing input paths:
    auto [star_catalog_path, kernel_path] = parse_input_paths(argc, argv);

    // Create the scene:
    huira::Scene<huira::RGB> scene;

    // Set the observation time:
    huira::Time time("2019-02-06T10:27:00");

    // Load stars at a specific date:
    //scene.load_stars(star_catalog_path, time);

    // Configure a camera model:
    auto camera_model = scene.new_camera_model();

    // Load the require SPICE kernels:
    huira::spice::furnsh("kernels/naif0012.tls");

    // Create the ECI frame:
    auto eci = scene.root.new_spice_subframe("EARTH", "J2000");

    // Create an instance of the camera in the ECI frame:
    auto navcam = eci.new_instance(camera_model);
    navcam.set_position(0, 0, -100); // Set camera position (in ECI frame)

    // Configure the render buffers:
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    frame_buffer.enable_sensor_response();

    // Create the renderer:
    huira::RasterRenderer<huira::RGB> renderer;

    // Create a scene view at the observation time:
    auto scene_view = huira::SceneView<huira::RGB>(scene, time, navcam, huira::ObservationMode::ABERRATED_STATE);

    // Render the current scene view:
    renderer.render(scene_view, frame_buffer);

    // Save the results:
    huira::write_image_png("star_field.png", frame_buffer.sensor_response(), 8);
}
