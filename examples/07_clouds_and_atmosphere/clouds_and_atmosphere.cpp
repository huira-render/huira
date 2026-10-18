// 07 - Clouds and atmosphere
//
// Demonstrates:
//   - Alpha: a texture that makes parts of a surface transparent (the clouds)
//   - Layered shells: one geometry inside another, each with its own material
//   - Participating media: a constant density field (absorption and scattering), an isotropic
//     phase function, and a medium made of the two
//   - A primitive with both a material and a medium; the null BSDF and a transmission factor,
//     for a boundary that light crosses unchanged
//
// Builds on: 06 Materials and textures
// Data: the Earth's daymap and cloud map (see README.md)
//
// Run:
//   ./07_clouds_and_atmosphere 8k_earth_daymap.jpg 8k_earth_clouds.jpg
//
// Writes:
//   output/07_clouds_and_atmosphere.png
//
// Note: volumetric rendering is still noisy; improvements are coming.

#include <filesystem>
#include <iostream>
#include <utility>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

constexpr double EQUATORIAL_RADIUS_KM = 6378.137;
constexpr double POLAR_RADIUS_KM = 6356.752;

// An ellipsoid the Earth's shape, altitude_km above its surface.
static huira::EllipsoidHandle<TSpectral> earth_shell(huira::Scene<TSpectral>& scene,
                                                     double altitude_km)
{
    const huira::units::Kilometer a(EQUATORIAL_RADIUS_KM + altitude_km);
    const huira::units::Kilometer b(POLAR_RADIUS_KM + altitude_km);
    return scene.add_ellipsoid(a, a, b);
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "Usage: 07_clouds_and_atmosphere <daymap> <cloud_map>\n"
                     "README.md says where to get them.\n";
        return 1;
    }
    const fs::path daymap_path = argv[1];
    const fs::path cloud_map_path = argv[2];

    huira::Scene<TSpectral> scene;

    // The surface: the daymap, on a Lambertian BSDF. (Example 06 builds the full material.)
    auto surface = scene.new_material(scene.new_bsdf_lambertian());
    auto daymap_srgb = huira::read_image(daymap_path);
    auto daymap = huira::srgb_to_linear(daymap_srgb); // JPEG textures are in sRGB color space
    surface.set_albedo_image(scene.add_texture(huira::rgb_to_spectral<TSpectral>(daymap.image)));
    scene.root.new_instance(scene.add_primitive(earth_shell(scene, 0.0), surface));

    // The clouds are a second shell, 6 km up, with a white Lambertian material. Its alpha image
    // (the cloud map, read as one channel) says how opaque each point is: where it is 0, light
    // passes through to the surface below.
    auto cloud_material = scene.new_material(scene.new_bsdf_lambertian());
    auto cloud_map = huira::read_image_mono(cloud_map_path);
    cloud_material.set_alpha_image(scene.add_texture(std::move(cloud_map.image)));
    scene.root.new_instance(scene.add_primitive(earth_shell(scene, 6.0), cloud_material));

    // The atmosphere is a medium: a volume that absorbs and scatters light along the paths
    // through it. A medium is a density field (how much it absorbs and scatters, per meter, in
    // each spectral bin) with a phase function (which way scattered light goes). This one is
    // uniform, scattering blue more than red, and scatters equally in all directions.
    const TSpectral absorption{0.f};                       // per meter
    const TSpectral scattering{0.5e-6f, 1.0e-6f, 2.0e-6f}; // per meter (red, green, blue)
    auto density = scene.new_constant_density_field(absorption, scattering);
    auto phase_function = scene.new_isotropic_phase_function();
    auto air = scene.new_medium(density, phase_function);

    // The medium fills a shell 100 km up. Its surface has the null BSDF, which reflects nothing,
    // and a transmission of 1, so light crosses the boundary unchanged and only the medium
    // inside acts on it.
    auto boundary = scene.new_material(scene.new_bsdf_null());
    boundary.set_transmission_factor(TSpectral{1.f});
    scene.root.new_instance(scene.add_primitive(earth_shell(scene, 100.0), boundary, air));

    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0.866_au, 0.5_au, 0_m);

    auto camera_model = scene.new_camera_model();
    camera_model.use_blender_convention();
    camera_model.set_focal_length(25_mm);
    camera_model.configure_sensor_from_size({1080, 1080}, 6_mm);
    camera_model.set_fstop(5);
    camera_model.set_sensor_bit_depth(12);
    camera_model.set_sensor_conversion_gain(1.f);
    camera_model.set_sensor_quantum_efficiency(0.8);
    camera_model.set_sensor_full_well_capacity(20000.f);
    camera_model.enable_sensor_noise(false);

    auto camera = scene.root.new_instance(camera_model);
    camera.set_position(100000_Km, 0_m, 0_m);
    camera.set_euler_angles(90_deg, 0_deg, 90_deg);

    huira::Time time("2026-01-01T00:00:00");
    huira::Interval exposure = huira::Interval::from_start(time, 25_us);
    huira::SceneView<TSpectral> scene_view(
        scene, exposure, camera, huira::ObservationMode::GEOMETRIC_STATE);

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();

    // Light that scatters in the atmosphere needs bounces to reach the camera, and volumes need
    // more samples than surfaces to settle.
    huira::Renderer<TSpectral> renderer;
    renderer.set_samples_per_pixel(256);
    renderer.set_max_bounces(4);

    camera_model.precompute();
    renderer.render(scene_view, frame_buffer);

    fs::create_directories("output");
    huira::write_image_png("output/07_clouds_and_atmosphere.png",
                           huira::linear_to_srgb(frame_buffer.sensor_response()));
}
