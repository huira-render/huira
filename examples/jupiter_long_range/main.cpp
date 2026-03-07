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
        std::cerr << "Usage: jupiter_long_range <tycho2.hrsc_path> <kernel_path>" << std::endl;
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
    huira::spice::furnsh(kernel_path / "spk/de440s.bsp");
    huira::spice::furnsh(kernel_path / "spk/jup365.bsp");

    // Create the scene
    huira::Scene<TSpectral> scene;

    // Configure a camera model
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(55_mm);
    camera_model.set_fstop(3.30f);
    camera_model.set_sensor_pixel_pitch(8.5_um, 8.5_um);
    camera_model.set_sensor_resolution(1920, 1080);
    camera_model.set_sensor_bit_depth(14);
    camera_model.use_aperture_psf(64, 16);
    
    huira::Time time("2016-09-19T16:22:05.728");
    huira::Interval exposure_interval = huira::Interval::from_centered(time, 2_s);

    // Load stars
    scene.load_stars(star_catalog_path, time);

    // Create the sun
    auto sun_light = scene.new_sun_light();
    auto sun = scene.root.new_instance(sun_light);
    sun.set_spice_origin("SUN");

    // Create unresolved objects for Jupiter and its moons
    //auto jupiter_model = scene.new_unresolved_sphere(69911000_m, sun, TSpectral{ 0.5f });
    auto jupiter_model = scene.new_unresolved_object_from_magnitude(-1.44, "Jupiter");
    //auto jupiter_model = scene.new_unresolved_object(TSpectral{ 1e-8 });
    auto io_model = scene.new_unresolved_object_from_magnitude(5.02);
    auto europa_model = scene.new_unresolved_object_from_magnitude(5.29);
    auto ganymede_model = scene.new_unresolved_object_from_magnitude(4.61);
    auto callisto_model = scene.new_unresolved_object_from_magnitude(5.65);

    // Create new instances of the unresolved objects
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

    // Create the ECI J2000 Reference Frame
    auto eci = scene.root.new_spice_subframe("EARTH_BARYCENTER", "J2000");

    // Create a camera instance in the ECI frame and set it's position, velocity, and orientation
    auto navcam = eci.new_instance(camera_model);
    navcam.set_position(7000_Km, 0_Km, 0_Km);
    navcam.set_velocity(0_Kmps, 8_Kmps, 0_Kmps);
    navcam.set_angular_velocity(1_degps, 1_degps, 1_degps);
    navcam.set_euler_angles(0_deg, 0_deg, 0_deg);

    //auto quat = huira::Quaternion<double>(0.50865, -0.50865, 0.491198, 0.491198);
    //navcam.set_rotation(huira::Rotation<double>::from_parent_to_local(quat));
    
    // Configure the render buffers
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    frame_buffer.enable_sensor_response();

    // Create the renderer
    huira::Renderer<TSpectral> renderer;

    // Create a scene view over the exposure interval
    std::size_t num_blur_samples = 1;
    auto scene_view = huira::SceneView<TSpectral>(scene, exposure_interval, navcam, huira::ObservationMode::ABERRATED_STATE, num_blur_samples);

    // Render the current scene view
    renderer.render(scene_view, frame_buffer);

    // Save the results
    huira::write_image_png("output/jupiter_long_range.png", frame_buffer.sensor_response(), 8);
    
}
