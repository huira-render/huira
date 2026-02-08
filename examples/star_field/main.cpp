#include "huira/huira.hpp"

#include <filesystem>
#include <iostream>
#include <utility>

using namespace std;
using namespace std::chrono;

using namespace huira::units::literals;

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
    // Image details (read from: https://sbnarchive.psi.edu/pds4/orex/orex.ocams/data_calibrated/cruise_1/20160919T162205S722_map_iofL2pan.xml)
    huira::Time time("2016-09-19T16:22:05.728");
    float exposure_time = 9.984285275f;
    huira::Vec2<float> pixel_pitch{ 8.5f * 1e-6f, 8.5f * 1e-6f };
    huira::Resolution resolution{ 1024, 1024 };
    float focal_length = 125.00f * 1e-3f;
    float f_number = 3.30f;

    // Parsing input paths:
    auto [star_catalog_path, kernel_path] = parse_input_paths(argc, argv);

    // Create the scene:
    huira::Scene<huira::RGB> scene;

    // Configure a camera model:
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(focal_length);
    camera_model.set_fstop(f_number);
    camera_model.set_sensor_rotation(-90_deg);
    camera_model.set_sensor_pixel_pitch(pixel_pitch);
    camera_model.set_sensor_resolution(resolution);
    camera_model.use_aperture_psf();
    camera_model.set_psf_accuracy(10, 16);

    // Load stars:
    scene.load_stars(star_catalog_path, time); // Load stars up to magnitude 6.0

    // Load the require SPICE kernels:
    huira::spice::furnsh(kernel_path / "fk/orx_v14.tf");
    huira::spice::furnsh(kernel_path / "sclk/orx_sclkscet_00093.tsc");
    huira::spice::furnsh(kernel_path / "ck/orx_struct_mapcam_v01.bc");
    huira::spice::furnsh(kernel_path / "ck/orx_sc_rel_160919_160925_v01.bc");
    huira::spice::furnsh(kernel_path / "spk/orx_struct_v04.bsp");
    huira::spice::furnsh(kernel_path / "spk/orx_160909_171201_170830_od023_v1.bsp");
    huira::spice::furnsh(kernel_path / "spk/de424.bsp");

    // Create an instance of the camera in the ECI frame:
    auto mapcam = scene.root.new_instance(camera_model);
    mapcam.set_spice("ORX_OCAMS_MAPCAM", "ORX_OCAMS_MAPCAM");
    

    // Configure the render buffers:
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    frame_buffer.enable_sensor_response();

    // Create the renderer:
    huira::RasterRenderer<huira::RGB> renderer;

    // Create the renderer:
    for (int i = 0; i < 100; ++i) {
        // Start timing the render
        auto frame_start = steady_clock::now();

        time = time + 10_s; // Advance time by 10 seconds

        // Create a scene view at the observation time:
        auto scene_view = huira::SceneView<huira::RGB>(scene, time, mapcam, huira::ObservationMode::ABERRATED_STATE);

        // Render the current scene view:
        renderer.render(scene_view, frame_buffer, exposure_time);

        // Save the results:
        char filename[64];
        snprintf(filename, sizeof(filename), "output/frame_%03d.png", i);
        huira::write_image_png(filename, frame_buffer.sensor_response(), 8);

        huira::FitsMetadata metadata{};
        char filename2[64];
        snprintf(filename2, sizeof(filename2), "output_fits/frame_%03d.FITS", i);
        huira::write_image_fits(filename2, frame_buffer.sensor_response(), 16, metadata);

        // Stop timing and print duration
        auto frame_end = steady_clock::now();
        auto frame_duration = duration_cast<milliseconds>(frame_end - frame_start);
        std::cout << "Frame " << i << " rendered in " << frame_duration.count() << " ms" << std::endl;
    }
}
