// 13 - SPICE
//
// Demonstrates:
//   - Loading SPICE kernels (huira::spice::furnsh): an ephemeris (SPK) and a binary PCK for the
//     Earth's orientation. Huira loads its own leap seconds kernel when it first needs one.
//   - SPICE frames in the scene graph (new_spice_subframe): an Earth-centered inertial frame
//     (J2000) and an Earth-fixed one (ITRF93) that turns with the Earth
//   - Placing an instance by ephemeris (set_spice_origin): the Sun, where it really was
//   - Placing a node on a Keplerian orbit (set_keplerian_orbit), with no kernel for it: the
//     camera, on a satellite orbiting the Earth, kept pointed at the Earth with look_at
//   - Time driving the scene: the same scene rendered at two times, six hours apart
//
// Builds on: 06 Materials and textures
// Data: two SPICE kernels and the Earth's daymap (see README.md)
//
// Run:
//   ./13_spice de440s.bsp earth_latest_high_prec.bpc 8k_earth_daymap.jpg
//
// Writes:
//   output/13_spice.png             2019-02-06 10:27 UTC
//   output/13_spice_6h_later.png    six hours later

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::cerr << "Usage: 13_spice <de440s.bsp> <earth_latest_high_prec.bpc> <daymap>\n"
                     "README.md says where to get them.\n";
        return 1;
    }
    const fs::path planets_kernel = argv[1];
    const fs::path earth_kernel = argv[2];
    const fs::path daymap_path = argv[3];

    // SPICE kernels hold ephemerides, orientations, frames and clocks. de440s gives the positions
    // of the Sun, Moon and planets; earth_latest_high_prec gives the Earth's precise orientation,
    // for the ITRF93 frame. (huira::spice::load_default_pck() loads NAIF's generic PCK, which
    // ships with Huira: its IAU_EARTH frame needs no download, but is less precise.)
    huira::spice::furnsh(planets_kernel);
    huira::spice::furnsh(earth_kernel);

    huira::Scene<TSpectral> scene;

    // A SPICE subframe takes its origin and orientation from SPICE, at whatever time the scene is
    // viewed. Both of these are centered on the Earth: eci keeps the J2000 orientation, and ecef
    // turns with the Earth.
    auto eci = scene.root.new_spice_subframe("EARTH", "J2000");
    auto ecef = scene.root.new_spice_subframe("EARTH", "ITRF93");

    // The Earth is built in the Earth-fixed frame, so its surface turns with it. (Example 06
    // builds the full material; the daymap alone is enough here. Its colors are sRGB-encoded, so
    // they are made linear first, as in example 06.)
    auto surface = scene.new_material(scene.new_bsdf_lambertian());
    auto daymap = huira::srgb_to_linear(huira::read_image(daymap_path));
    surface.set_albedo_image(scene.add_texture(huira::rgb_to_spectral<TSpectral>(daymap.image)));
    auto earth_shape = scene.add_ellipsoid(6378.137_Km, 6378.137_Km, 6356.752_Km);
    auto earth = ecef.new_instance(scene.add_primitive(earth_shape, surface));

    // The Sun's position comes from the ephemeris, rather than set by hand:
    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_spice_origin("SUN");

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

    // The camera rides a satellite on a circular orbit 100,000 km from the Earth's center,
    // inclined 30 degrees to the equator. set_keplerian_orbit places a node by its orbital
    // elements about its parent, here the inertial frame: no kernel is needed. The elements are
    // the semi-major axis, eccentricity, inclination, right ascension of the ascending node,
    // argument of periapsis and mean anomaly at an epoch, and the central body's gravitational
    // parameter, in km^3/s^2. The orbit takes about 87 hours, so in six hours the satellite
    // moves about 25 degrees along it.
    const huira::Time time("2019-02-06T10:27:00");
    auto camera = eci.new_instance(camera_model);
    camera.set_keplerian_orbit(100000_Km, 0.0, 30_deg, 0_deg, 0_deg, 0_deg, time, 398600.4418);

    // A node can also be placed by a table of times and positions (set_position_table), as from
    // a trajectory file, or by any function of time (set_position_function), and turned the
    // same ways (set_rotation_table, set_rotation_function).

    // look_at keeps the camera pointed at the Earth as both move, with celestial north up:
    camera.look_at(earth, huira::Vec3<double>{0.0, 0.0, 1.0});

    huira::Renderer<TSpectral> renderer;
    renderer.set_samples_per_pixel(64);
    camera_model.precompute();

    fs::create_directories("output");

    // Every SPICE-driven frame and instance, and every orbit, is evaluated at the scene view's
    // time. Six hours later, the Earth has turned a quarter of the way around, the satellite has
    // moved along its orbit, and the Sun has hardly moved.
    const std::vector<std::pair<huira::Time, std::string>> renders = {
        {time, "output/13_spice.png"},
        {time + 6_h, "output/13_spice_6h_later.png"},
    };
    for (const auto& [when, filename] : renders) {
        const huira::Interval exposure = huira::Interval::from_start(when, 25_us);
        huira::SceneView<TSpectral> scene_view(scene, exposure, camera);
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_sensor_response();
        renderer.render(scene_view, frame_buffer);
        huira::write_image_png(filename, huira::linear_to_srgb(frame_buffer.sensor_response()));
    }
}
