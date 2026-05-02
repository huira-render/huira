#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

static fs::path parse_input_paths(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: earth <data_path>" << std::endl;
        std::exit(1);
    }
    fs::path data_path = argv[1];
    return data_path;
}

int main(int argc, char** argv)
{
    huira::Logger::enable_console_debug();

    // Parsing input paths
    fs::path data_path = parse_input_paths(argc, argv);

    // Load the require SPICE kernels
    huira::spice::furnsh(data_path / "kernels/spk/de440s.bsp");
    huira::spice::furnsh(data_path / "kernels/pck/earth_latest_high_prec.bpc");
    huira::spice::furnsh(data_path / "kernels/pck/earth_fixed.tf");

    // Create the scene
    huira::Scene<TSpectral> scene;

    // Create the refernce frames:
    auto eci = scene.root.new_spice_subframe("EARTH", "J2000");
    auto ecef = scene.root.new_spice_subframe("EARTH", "ITRF93");

    // Set the observation time
    huira::Time time("2019-02-06T10:27:00");
    huira::Interval exposure_interval{time, time + 0.00005_s};

    // Configure a camera model
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(25_mm);
    camera_model.configure_sensor_from_size({1080, 1080}, 6_mm);

    // Set camera exposure settings
    camera_model.set_fstop(5);
    camera_model.set_sensor_gain(1.f);
    camera_model.set_sensor_bit_depth(12);
    camera_model.set_sensor_quantum_efficiency(0.8);
    camera_model.set_sensor_full_well_capacity(20000);
    // camera_model.set_veiling_glare(0.001f);
    // camera_model.use_aperture_psf(64, 16);
    // camera_model.enable_psf_convolution(true);
    camera_model.set_sensor_simulate_noise(false);

    // Huira uses the OpenCV convention by default, which is
    // +z forward, +y down.  Blender uses -z forward, +y up.
    // This flag allows you to match Blender's for easier
    // comparison with blender generated images.
    camera_model.use_blender_convention();

    // Create the Earth material:
    auto ct_bsdf = scene.new_bsdf_cook_torrance();
    auto earth_material = scene.new_material(ct_bsdf);

    // fs::path huira_data = "C:/Users/chris/huira_data";
    auto earth_albedo_rgb = huira::read_image(data_path / "models/earth/8k_earth_daymap.jpg");
    auto earth_albedo_spec = huira::rgb_to_spectral<TSpectral>(earth_albedo_rgb.image);
    auto earth_albedo_tex = scene.add_texture(std::move(earth_albedo_spec));
    earth_material.set_albedo_image(earth_albedo_tex);

    auto earth_roughness =
        huira::read_image_mono(data_path / "models/earth/8k_earth_roughness_map.tif");
    auto earth_roughness_tex = scene.add_texture(std::move(earth_roughness.image));
    earth_material.set_roughness_image(earth_roughness_tex);
    earth_material.set_metallic_factor(0.f);

    auto earth_normal = huira::read_image(data_path / "models/earth/8k_earth_normal_map.tif");
    auto earth_normal_tex = scene.add_normal_texture(std::move(earth_normal.image));
    earth_material.set_normal_image(earth_normal_tex);

    auto R_e = 6378.137_Km;
    auto earth_ellipsoid = scene.add_ellipsoid(R_e, R_e, R_e);
    auto earth_primitive = scene.add_primitive(earth_ellipsoid, earth_material);
    eci.new_instance(earth_primitive);

    auto earth_cloud_alpha =
        huira::read_image_mono(data_path / "models/earth/8k_earth_clouds.jpg");
    auto lam_bsdf = scene.new_bsdf_lambertian();
    auto earth_clouds_material = scene.new_material(lam_bsdf);
    auto earth_clouds_tex = scene.add_texture(std::move(earth_cloud_alpha.image));
    earth_clouds_material.set_alpha_image(earth_clouds_tex);

    auto alt_clouds = 6_Km;
    auto earth_clouds_ellipsoid =
        scene.add_ellipsoid(R_e + alt_clouds, R_e + alt_clouds, R_e + alt_clouds);
    auto earth_clouds_primitive =
        scene.add_primitive(earth_clouds_ellipsoid, earth_clouds_material);
    eci.new_instance(earth_clouds_primitive);

    auto alt_atmosphere = 100_Km;
    auto atmosphere_ellipsoid =
        scene.add_ellipsoid(R_e + alt_atmosphere, R_e + alt_atmosphere, R_e + alt_atmosphere);
    auto null_bsdf = scene.new_bsdf_null();
    auto atmosphere_material = scene.new_material(null_bsdf);
    atmosphere_material.set_transmission_factor(TSpectral{1.f});

    auto constant_density_field = scene.new_constant_density_field(TSpectral{0}, TSpectral{2e-6f});
    auto isotropic_phase_function = scene.new_isotropic_phase_function();
    auto atmosphere_medium = scene.new_medium(constant_density_field, isotropic_phase_function);
    auto atmosphere_primitive =
        scene.add_primitive(atmosphere_ellipsoid, atmosphere_material, atmosphere_medium);
    eci.new_instance(atmosphere_primitive);

    // Create instance of the camera:
    auto navcam = eci.new_instance(camera_model);
    navcam.set_position(100000_Km, 0_Km, 0_m);
    navcam.set_euler_angles(90_deg, 0_deg, 90_deg);

    // scene.load_stars(data_path / "tycho2/tycho2.hrsc", time);

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
    auto scene_view = huira::SceneView<TSpectral>(scene,
                                                  exposure_interval,
                                                  navcam,
                                                  huira::ObservationMode::ABERRATED_STATE,
                                                  num_blur_samples);

    // Render the current scene view
    renderer.render(scene_view, frame_buffer);

    // Save the results
    huira::write_image_png("output/earth.png",
                           huira::linear_to_srgb(frame_buffer.sensor_response()));
    huira::write_image_png("output/earth_normals.png",
                           huira::normal_map(frame_buffer.camera_normals()));

    huira::Logger::dump_to_file("output/earth_render_log.txt");
}
