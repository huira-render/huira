#include "huira/huira.hpp"

#include <filesystem>
#include <iostream>
#include <utility>

using namespace std;
using namespace std::chrono;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

namespace fs = std::filesystem;

static std::pair<fs::path, fs::path> parse_input_paths(int argc, char** argv) {
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

    // Load the require SPICE kernels:
    huira::spice::furnsh(kernel_path / "spk/de440s.bsp");
    huira::spice::furnsh(kernel_path / "spk/jup365.bsp");

    // Create the scene:
    huira::Scene<TSpectral> scene;

    // Configure a camera model:
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(.025f);
    camera_model.set_fstop(3.30f);
    camera_model.set_sensor_pixel_pitch(8.5e-6f, 8.5e-6f);
    camera_model.set_sensor_resolution(1920, 1080);
    camera_model.use_aperture_psf();
    camera_model.set_sensor_bit_depth(14);
    camera_model.set_psf_accuracy(64, 16);
    
    huira::Time time("2016-09-19T16:22:05.728");
    float exposure_time = 1.f;

    // Load stars:
    scene.load_stars(star_catalog_path, time);

    // Create unresolved objects for Jupiter and its moons:
    auto jupiter_model = scene.new_unresolved_object(TSpectral{ 100 });
    auto io_model = scene.new_unresolved_object(TSpectral{ 10 });
    auto europa_model = scene.new_unresolved_object(TSpectral{ 10 });
    auto ganymede_model = scene.new_unresolved_object(TSpectral{ 10 });
    auto callisto_model = scene.new_unresolved_object(TSpectral{ 10 });

    // Create new instances of the unresolved objects:
    auto jupiter = scene.root.new_instance(jupiter_model);
    jupiter.set_spice_origin("JUPITER");
    auto io = scene.root.new_instance(io_model);
    io.set_spice_origin("IO");
    auto europa = scene.root.new_instance(europa_model);
    europa.set_spice_origin("EUROPA");
    auto ganymede = scene.root.new_instance(ganymede_model);
    ganymede.set_spice_origin("GANYMEDE");
    auto callisto = scene.root.new_instance(callisto_model);
    callisto.set_spice_origin("CALLISTO");

    // Create an instance of the camera:
    auto navcam = scene.root.new_instance(camera_model);
    camera_model.use_blender_convention();
    
    // Configure the render buffers:
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    frame_buffer.enable_sensor_response();

    // Create the renderer:
    huira::RasterRenderer<TSpectral> renderer;

    // Create a scene view at the observation time:
    for (int i = 0; i < 36; ++i) {
        navcam.set_euler_angles(90_deg, 0_deg, i * 10_deg);

        auto scene_view = huira::SceneView<TSpectral>(scene, time, navcam, huira::ObservationMode::ABERRATED_STATE);

        // Render the current scene view:
        renderer.render(scene_view, frame_buffer, exposure_time);

        // Save the results:
        char filename[64];
        snprintf(filename, sizeof(filename), "jupiter/frame_%03d.png", i);
        huira::write_image_png(filename, frame_buffer.sensor_response(), 8);
        //huira::write_image_png("output/jupiter_long_range.png", frame_buffer.sensor_response(), 8);
    }
    
}
