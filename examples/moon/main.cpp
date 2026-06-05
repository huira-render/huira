#include <filesystem>
#include <iostream>

#include "huira/huira.hpp"
#include "huira/assets/io/qld_loader.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::Visible8;

static fs::path parse_input_paths(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: moon <qpu_dir>" << std::endl;
        std::exit(1);
    }
    fs::path qpu_path = argv[1];
    return qpu_path;
}

int main(int argc, char** argv)
{
    huira::Logger::enable_console_debug();

    fs::path qpu_path = parse_input_paths(argc, argv);

    huira::Scene<TSpectral> scene;

    auto bsdf = scene.new_bsdf_mcewen();
    auto moon_material = scene.new_material(bsdf);
    moon_material.set_albedo_factor(TSpectral{1.0f});

    auto moon = scene.root.new_subframe();
    huira::qld::add_tiles(scene, moon, qpu_path, moon_material, 0, 1737400.0, "moon", 2000.0);

    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(35_mm);
    camera_model.configure_sensor_from_size({1024, 1024}, 12_mm);
    camera_model.set_fstop(5.6f);
    camera_model.set_sensor_gain(1.0f);
    camera_model.set_sensor_bit_depth(12);
    camera_model.set_sensor_quantum_efficiency(0.8f);
    camera_model.set_sensor_full_well_capacity(20000);
    camera_model.set_sensor_simulate_noise(false);
    camera_model.use_blender_convention();

    auto camera = scene.root.new_instance(camera_model);
    camera.set_position(12000_Km, 0_Km, 0_Km);
    camera.set_euler_angles(90_deg, 0_deg, 90_deg);

    auto sun_light = scene.new_sun_light();
    auto sun = scene.root.new_instance(sun_light);
    sun.set_position(150000000_Km, 0_Km, 0_Km);

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();

    huira::Renderer<TSpectral> renderer;
    renderer.set_max_bounces(2);
    renderer.set_samples_per_pixel(32);

    huira::Time time("2025-06-07T12:00:00");
    huira::Interval exposure_interval{time, time + 0.00005_s};
    std::size_t num_blur_samples = 1;
    auto scene_view = huira::SceneView<TSpectral>(scene,
                                                  exposure_interval,
                                                  camera,
                                                  huira::ObservationMode::GEOMETRIC_STATE,
                                                  num_blur_samples);

    renderer.render(scene_view, frame_buffer);

    fs::create_directories("output");
    huira::write_image_png("output/moon.png", huira::linear_to_srgb(frame_buffer.sensor_response()));

    return 0;
}
