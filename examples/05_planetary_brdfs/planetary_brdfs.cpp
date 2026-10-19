// 05 - Planetary BRDFs
//
// Demonstrates:
//   - The BSDFs made for planetary surfaces, side by side on spheres of the same albedo:
//     Lambertian, Oren-Nayar (with a roughness), Lommel-Seeliger, McEwen and Hapke
//   - Hapke's parameters: opposition surge width and amplitude, and the phase function's
//     asymmetry and forward/backward fraction
//   - How the phase angle (Sun-target-camera) changes each one: rendered near opposition, where
//     regolith brightens sharply, and at 90 degrees, where limb darkening differs most
//   - Moving a light between renders of the same scene
//
// Builds on: 02 Scene graph
// Data: none
//
// Run:
//   ./05_planetary_brdfs
//
// Writes:
//   output/05_planetary_brdfs.png              at a 90 degree phase angle
//   output/05_planetary_brdfs_opposition.png   at a 5 degree phase angle

#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

int main()
{
    huira::Scene<TSpectral> scene;
    auto sphere = scene.add_ellipsoid(1_m, 1_m, 1_m);

    // The planetary BSDFs, left to right. Each takes the albedo from its material:
    //   - Lambertian: ideal diffuse; brightness depends only on the incidence angle
    //   - Oren-Nayar: rough diffuse, with the roughness from the material (0 is Lambertian)
    //   - Lommel-Seeliger: single scattering in a dark, dusty medium; no limb darkening
    //   - McEwen: blends Lommel-Seeliger and Lambert with phase angle, as fits the Moon
    //   - Hapke: the five-parameter regolith model. h and B0 are the width and amplitude of the
    //     opposition surge; b and c the asymmetry and forward/backward fraction of its
    //     double Henyey-Greenstein phase function. (These values are illustrative, of the order
    //     reported for lunar regolith.) Its albedo is the single scattering albedo.
    const float h = 0.06f;
    const float B0 = 1.0f;
    const float b = 0.25f;
    const float c = 0.6f;
    std::vector<std::pair<std::string, huira::BSDFHandle<TSpectral>>> bsdfs = {
        {"lambertian", scene.new_bsdf_lambertian()},
        {"oren_nayar", scene.new_bsdf_oren_nayar()},
        {"lommel_seeliger", scene.new_bsdf_lommel_seeliger()},
        {"mcewen", scene.new_bsdf_mcewen()},
        {"hapke", scene.new_bsdf_hapke(h, B0, b, c)},
    };

    const double spacing = 2.5;
    for (std::size_t i = 0; i < bsdfs.size(); ++i) {
        const auto& [name, bsdf] = bsdfs[i];
        auto material = scene.new_material(bsdf, name);
        material.set_albedo_factor(TSpectral{0.3f});
        if (name == "oren_nayar") {
            material.set_roughness_factor(0.5f);
        }

        auto primitive = scene.add_primitive(sphere, material, name);
        auto instance = scene.root.new_instance(primitive);
        const double x = (static_cast<double>(i) - 2.0) * spacing;
        instance.set_position(huira::units::Meter(x), 0_m, 0_m);
    }

    // A long lens from far away, so every sphere is seen from nearly the same direction:
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(100_mm);
    camera_model.configure_sensor_from_size({1600, 400}, 36_mm);
    camera_model.set_fstop(16);

    auto camera = scene.root.new_instance(camera_model);
    camera.set_position(0_m, 0_m, -40_m);

    auto sun = scene.root.new_instance(scene.new_sun_light());

    huira::Time time("2026-01-01T00:00:00");
    huira::Interval exposure = huira::Interval::from_start(time, 400_us);

    huira::Renderer<TSpectral> renderer;
    renderer.set_samples_per_pixel(64);

    fs::create_directories("output");

    // The camera looks along +z, with +y down the image. The Sun, 1 AU away in the y-z plane and
    // phase_angle away from straight behind the camera, lights the spheres from above at that
    // phase angle (from the side, along the row, each sphere would shadow the next). A scene can
    // be rendered again after changes: each SceneView takes the scene as it is when it is made.
    const std::vector<std::pair<double, std::string>> renders = {
        {90.0, "output/05_planetary_brdfs.png"},
        {5.0, "output/05_planetary_brdfs_opposition.png"},
    };
    for (const auto& [phase_angle_deg, filename] : renders) {
        const double phase = phase_angle_deg * huira::PI<double>() / 180.0;
        sun.set_position(0_m,
                         huira::units::AstronomicalUnit(-std::sin(phase)),
                         huira::units::AstronomicalUnit(-std::cos(phase)));

        huira::SceneView<TSpectral> scene_view(scene, exposure, camera);
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_sensor_response();
        renderer.render(scene_view, frame_buffer);

        huira::write_image_png(filename, huira::linear_to_srgb(frame_buffer.sensor_response()));
    }
}
