// 14 - Unresolved objects
//
// Demonstrates:
//   - Unresolved objects: bodies too small or too far away to cover a pixel, drawn like stars,
//     with the camera's PSF. Jupiter is an unresolved sphere, whose brightness follows from its
//     size, its albedo and where the Sun is (new_unresolved_sphere); its moons are given by
//     their magnitudes
//   - The other ways to make one: by irradiance, by power, or by an asteroid's H and G
//     magnitudes (in comments)
//   - Bodies placed by ephemeris (set_spice_origin), and a camera in an inertial frame with a
//     position, a velocity and an attitude given by a quaternion
//   - Observation modes: ABERRATED_STATE (light time and stellar aberration, as a camera really
//     sees) against TRUE_STATE (where each body is at the instant)
//
// Builds on: 13 SPICE
// Data: two SPICE kernels and tycho2.hrsc, the Tycho-2 star catalog (see README.md)
//
// Run:
//   ./14_unresolved_objects de440s.bsp jup365.bsp /path/to/tycho2.hrsc
//
// Writes:
//   output/14_unresolved_objects.png              as the camera sees it
//   output/14_unresolved_objects_true_state.png   without light time or aberration

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::Visible8;

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::cerr << "Usage: 14_unresolved_objects <de440s.bsp> <jup365.bsp> <tycho2.hrsc>\n"
                     "README.md says where to get them.\n";
        return 1;
    }
    const fs::path planets_kernel = argv[1];
    const fs::path jupiter_kernel = argv[2];
    const fs::path catalog = argv[3];

    // de440s places Jupiter's system as a whole (its barycenter), and jup365 places Jupiter and
    // its moons within it.
    huira::spice::furnsh(planets_kernel);
    huira::spice::furnsh(jupiter_kernel);

    huira::Scene<TSpectral> scene;
    const huira::Time time("2016-09-19T16:22:05.728");
    scene.load_stars(catalog, time);

    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_spice_origin("SUN");

    // An unresolved sphere is lit by the light instance given, here the Sun. Its brightness
    // follows from its radius and albedo, its distances from the Sun and the camera, and the
    // angle between them as seen from it, wherever they all are when the scene is viewed. It
    // reflects as a Lambertian sphere, whose geometric albedo is two thirds of its albedo: 0.8
    // gives Jupiter's geometric albedo of 0.54, and so about its real brightness when it is seen
    // nearly full, as here.
    auto jupiter_model = scene.new_unresolved_sphere(69911_Km, sun, 0.8f);

    // An unresolved object can also be given its visual magnitude, which stays as given however
    // the scene moves: these are the moons' magnitudes on this day, with Jupiter 6.5 AU away,
    // about one fainter than at opposition. (The others: new_unresolved_object takes the
    // irradiance at the camera, new_unresolved_emitter a total power that falls off with
    // distance, and new_unresolved_asteroid(H, G, sun) an asteroid's H and G magnitudes.)
    auto io_model = scene.new_unresolved_object_from_magnitude(6.05, "Io");
    auto europa_model = scene.new_unresolved_object_from_magnitude(6.32, "Europa");
    auto ganymede_model = scene.new_unresolved_object_from_magnitude(5.64, "Ganymede");
    auto callisto_model = scene.new_unresolved_object_from_magnitude(6.68, "Callisto");

    // Each is placed by ephemeris:
    const std::vector<std::pair<huira::UnresolvedObjectHandle<TSpectral>, std::string>> bodies = {
        {jupiter_model, "JUPITER"},
        {io_model, "IO"},
        {europa_model, "EUROPA"},
        {ganymede_model, "GANYMEDE"},
        {callisto_model, "CALLISTO"},
    };
    for (const auto& [model, body] : bodies) {
        scene.root.new_instance(model).set_spice_origin(body);
    }

    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(125_mm);
    camera_model.set_fstop(3.3f);
    camera_model.configure_sensor_from_pitch({1920, 1080}, 8.5_um);
    camera_model.set_sensor_bit_depth(12);
    camera_model.precompute();

    // The camera rides a satellite in low Earth orbit: 7000 km from the Earth's center, moving
    // at 8 km/s, in an Earth-centered frame with the J2000 axes. Its attitude is a quaternion
    // (w, x, y, z), from the J2000 axes to the camera's, which points it at Jupiter.
    auto eci = scene.root.new_spice_subframe("EARTH", "J2000");
    auto camera = eci.new_instance(camera_model);
    camera.set_position(7000_Km, 0_Km, 0_Km);
    camera.set_velocity(0_Kmps, 8_Kmps, 0_Kmps);
    const huira::Quaternion<double> attitude(0.50865, -0.50865, 0.491198, 0.491198);
    camera.set_rotation(huira::Rotation<double>::from_parent_to_local(attitude));

    // Unresolved objects behind resolved geometry are hidden by it (set_unresolved_occlusion);
    // here there is none to hide them.
    huira::Renderer<TSpectral> renderer;

    fs::create_directories("output");

    // The observation mode decides where each thing appears:
    //   - TRUE_STATE: where it is at the instant of the exposure
    //   - GEOMETRIC_STATE: where it was when the light now arriving left it (light time)
    //   - ABERRATED_STATE, the default: that, shifted by stellar aberration from the camera's
    //     own velocity, as a real camera sees it
    // Between the two images here, aberration from the Earth's 30 km/s about the Sun and the
    // camera's 8 km/s about the Earth moves everything by about 25 arcseconds, nearly two
    // pixels, and light time moves Jupiter 9 arcseconds more.
    const huira::Interval exposure = huira::Interval::from_centered(time, 1_s);
    const std::vector<std::pair<huira::ObservationMode, std::string>> renders = {
        {huira::ObservationMode::ABERRATED_STATE, "output/14_unresolved_objects.png"},
        {huira::ObservationMode::TRUE_STATE, "output/14_unresolved_objects_true_state.png"},
    };
    for (const auto& [mode, filename] : renders) {
        huira::SceneView<TSpectral> scene_view(scene, exposure, camera, mode);
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_sensor_response();
        renderer.render(scene_view, frame_buffer);
        huira::write_image_png(filename, huira::linear_to_srgb(frame_buffer.sensor_response()));
    }
}
