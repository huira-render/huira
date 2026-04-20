#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

static std::pair<fs::path, fs::path> parse_input_paths(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: earth <earth.glb_path> <kernel_path>" << std::endl;
        std::exit(1);
    }
    fs::path earth_path = argv[1];
    fs::path kernel_path = argv[2];
    return { earth_path, kernel_path };
}


int main(int argc, char** argv) {
    // Parsing input paths
    auto [earth_path, kernel_path] = parse_input_paths(argc, argv);

    // Load the require SPICE kernels
    huira::spice::furnsh(kernel_path / "spk/de440s.bsp");
    huira::spice::furnsh(kernel_path / "pck/earth_latest_high_prec.bpc");
    huira::spice::furnsh(kernel_path / "pck/earth_fixed.tf");

    // Create the scene
    huira::Scene<TSpectral> scene;
    scene.set_background_radiance(TSpectral{ 1.f, 1.f, 1.f });

    // Set the observation time
    huira::Time time("2019-02-06T10:27:00");
    huira::Interval exposure_interval{ time, time + 0.00025_s };

    // Configure a camera model
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(25_mm);
    camera_model.configure_sensor_from_size({ 1080, 1080 }, 6_mm);

    // Set camera exposure settings
    camera_model.set_fstop(12);
    camera_model.set_sensor_gain(1.f);
    camera_model.set_sensor_bit_depth(12);
    camera_model.set_sensor_quantum_efficiency(0.8);
    camera_model.set_sensor_full_well_capacity(20000);

    // Huira uses the OpenCV convention by default, which is
    // +z forward, +y down.  Blender uses -z forward, +y up.
    // This flag allows you to match Blender's for easier
    // comparison with blender generated images.
    camera_model.use_blender_convention();
    
    // Create instance of the camera:
    auto eci = scene.root.new_spice_subframe("EARTH", "J2000");
    auto navcam = eci.new_instance(camera_model);
    navcam.set_position(100000_Km, 0_Km, 0_m);
    navcam.set_euler_angles(90_deg, 0_deg, 90_deg);

    // Create the Earth frame:
    auto ecef = scene.root.new_spice_subframe("EARTH", "ITRF93");

    // Load the Earth model
    auto earth_model = scene.load_model(earth_path);
    auto earth = ecef.new_instance(earth_model);
    earth.set_scale(6371*1000);

    // Create an atmosphere:
    //auto earth_atmosphere = scene.new_earth_atmosphere();
    //ecef.new_instance(earth_atmosphere);

    // Create the sun
    auto sun_light = scene.new_sun_light();
    auto sun = scene.root.new_instance(sun_light);
    sun.set_spice_origin("SUN");

    // Configure the render buffers
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.enable_camera_normals();

    // Create the renderer
    huira::Renderer<TSpectral> renderer;
    renderer.set_max_bounces(3);
    renderer.set_samples_per_pixel(100);

    // Create a scene view over the exposure interval
    std::size_t num_blur_samples = 1;
    auto scene_view = huira::SceneView<TSpectral>(scene, exposure_interval, navcam, huira::ObservationMode::ABERRATED_STATE, num_blur_samples);

    // Render the current scene view
    renderer.render(scene_view, frame_buffer);

    // Save the results
    huira::write_image_png("output/earth.png", frame_buffer.sensor_response());
    huira::write_image_png("output/earth_normals.png", huira::normal_map(frame_buffer.camera_normals()));
}
