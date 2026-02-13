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
    // Parsing input paths
    auto [star_catalog_path, kernel_path] = parse_input_paths(argc, argv);

    // Load the require SPICE kernels
    huira::spice::furnsh(kernel_path / "fk/orx_v14.tf");
    huira::spice::furnsh(kernel_path / "sclk/orx_sclkscet_00093.tsc");
    huira::spice::furnsh(kernel_path / "ck/orx_struct_mapcam_v01.bc");
    huira::spice::furnsh(kernel_path / "ck/orx_sc_rel_160919_160925_v01.bc");
    huira::spice::furnsh(kernel_path / "spk/orx_struct_v04.bsp");
    huira::spice::furnsh(kernel_path / "spk/orx_160909_171201_170830_od023_v1.bsp");
    huira::spice::furnsh(kernel_path / "spk/de424.bsp");

    // Create the scene
    huira::Scene<TSpectral> scene;

    // Configure a camera model
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(.125f);
    camera_model.set_fstop(3.30f);
    camera_model.set_sensor_rotation(90_deg);
    camera_model.set_sensor_pixel_pitch(8.5e-6f, 8.5e-6f);
    camera_model.set_sensor_resolution(1024, 1024);
    camera_model.use_aperture_psf(32, 16);
    camera_model.set_sensor_bit_depth(14);

    // Set the observation time
    huira::Time time("2016-09-19T16:22:05.728");
    float exposure_time = 9.984285275f;

    // Load stars
    scene.load_stars(star_catalog_path, time);

    // Create an instance of the camera using SPICE configuration
    auto mapcam = scene.root.new_instance(camera_model);
    mapcam.set_spice("ORX_OCAMS_MAPCAM", "ORX_OCAMS_MAPCAM");
    
    // Configure the render buffers
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    frame_buffer.enable_sensor_response();

    // Create the renderer
    huira::RasterRenderer<TSpectral> renderer;

    // Create a scene view at the observation time
    auto scene_view = huira::SceneView<TSpectral>(scene, time, mapcam, huira::ObservationMode::ABERRATED_STATE);

    // Render the current scene view
    renderer.render(scene_view, frame_buffer, exposure_time);

    // Save the results
    huira::write_image_png("output/starfield.png", frame_buffer.sensor_response(), 8);
}
