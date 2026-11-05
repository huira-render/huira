// 15 - Satellites
//
// Demonstrates:
//   - Reading a catalog of NORAD element sets, as TLEs or OMMs, from CelesTrak or Space-Track
//     (ElementSetCatalog), and what it leaves out
//   - Placing one satellite by its element set, with SGP4 (set_tle_position): the camera rides
//     the ISS, in the scene's TEME frame
//   - A constellation: every satellite in the catalog at once, each seen as a sunlit sphere,
//     with the element sets it holds brought up to date from the catalog (new_constellation)
//   - A satellite is placed once: the constellation leaves out the ISS, which the camera's node
//     holds, and says so
//   - Truth data for a point source: where each satellite in view is, from the scene view
//   - A satellite SGP4 can't place during the exposure (decayed, most often) is left out of
//     the view with a warning, rather than stopping it
//
// Builds on: 13 SPICE, 14 Unresolved objects
// Data: an Earth ephemeris (de440s.bsp), a catalog of element sets from CelesTrak, and the Earth's
// daymap (see README.md)
//
// Run:
//   ./15_satellites de440s.bsp active.csv /path/to/8k_earth_daymap.jpg
//
// Writes:
//   output/15_satellites.png   the sky ahead of the ISS, with the satellites in view as streaks
//   output/15_satellites.csv   each satellite in view: its catalog number, name and pixel

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "glm/glm.hpp"
#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

int main(int argc, char** argv)
{
    if (argc != 4) {
        std::cerr << "Usage: 15_satellites <de440s.bsp> <catalog> <8k_earth_daymap.jpg>\n"
                     "README.md says where to get them.\n";
        return 1;
    }
    const fs::path planets_kernel = argv[1];
    const fs::path catalog_path = argv[2];
    const fs::path daymap_path = argv[3];

    huira::enable_console_info();

    // SGP4 gives positions relative to the Earth, in the TEME frame, so rendering anything it
    // places needs the Earth's ephemeris: de440s, as in example 13. NAIF's generic PCK, which
    // ships with Huira, orients the Earth itself (its IAU_EARTH frame).
    huira::spice::furnsh(planets_kernel);
    huira::spice::load_default_pck();

    // A catalog of element sets, in any of the formats CelesTrak and Space-Track publish: TLEs,
    // with or without name lines, or OMMs as CSV, KVN or JSON (read() works the format out from
    // the text). Each entry is checked as SGP4 needs it. An entry that isn't an element set is
    // left out and reported, with why, rather than failing the read; and a satellite listed more
    // than once is kept once, with its newest element set. (One element set alone is read with
    // SatRec::from_tle(line1, line2) or SatRec::from_omm({...}).)
    const huira::ElementSetCatalog catalog = huira::ElementSetCatalog::read(catalog_path);
    std::cout << "Read " << catalog.element_sets.size() << " element sets, leaving out "
              << catalog.skipped.size() << " entries\n";
    for (std::size_t i = 0; i < std::min<std::size_t>(catalog.skipped.size(), 5); ++i) {
        const huira::SkippedElementSet& skipped = catalog.skipped[i];
        std::cout << "  entry " << skipped.entry << ", line " << skipped.line << ": "
                  << skipped.message << "\n";
    }

    // The ISS, by its NORAD catalog number
    const auto found =
        std::find_if(catalog.element_sets.begin(),
                     catalog.element_sets.end(),
                     [](const huira::SatRec& s) { return s.catalog_number() == 25544; });
    if (found == catalog.element_sets.end()) {
        std::cerr << "The catalog has no element set for the ISS (25544)\n";
        return 1;
    }
    const huira::SatRec iss = *found;

    // The scene is viewed at the ISS's element set's epoch, when SGP4 places it best: an element
    // set is good to about a kilometer at its epoch, and drifts from there. The rest of the
    // catalog's element sets are hours to days from theirs.
    const huira::Time time = iss.epoch();
    std::cout << "The ISS's element set: " << iss.name() << ", epoch " << time.to_utc_string()
              << "\n";

    huira::Scene<TSpectral> scene;

    // The Earth, placed and turned by SPICE as in example 13, lit by the Sun where it really is.
    // It hides the satellites behind it, and shades those in its shadow.
    auto surface = scene.new_material(scene.new_bsdf_lambertian());
    auto daymap = huira::srgb_to_linear(huira::read_image(daymap_path));
    surface.set_albedo_image(scene.add_texture(huira::rgb_to_spectral<TSpectral>(daymap.image)));
    auto earth_shape = scene.add_ellipsoid(6378.137_Km, 6378.137_Km, 6356.752_Km);
    auto ecef = scene.root.new_spice_subframe("EARTH", "IAU_EARTH");
    ecef.new_instance(scene.add_primitive(earth_shape, surface));

    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_spice_origin("SUN");

    // One satellite, placed by its element set. SGP4 places things in the TEME frame, so a node
    // it places is a child of the scene's TEME frame: centered on the Earth, with the axes SGP4
    // uses. Whatever the node holds moves with it: here, the camera.
    auto teme = scene.teme_frame();
    auto iss_node = teme.new_subframe();
    iss_node.set_tle_position(iss);

    // Every other satellite, as a constellation. Each is placed by SGP4 as a node would be, but
    // without a node: a scene view places them all at once, in parallel. Each is seen as an
    // instance of one unresolved object; here, a sunlit Lambertian sphere 3 m in radius with an
    // albedo of 0.3, bright or faint by its phase and dark in the Earth's shadow (see example
    // 14 for the others, such as a fixed magnitude).
    auto body = scene.new_unresolved_sphere(3_m, sun, 0.3f);
    auto constellation = scene.new_constellation(body, "catalog");

    // load() reads the catalog again, with options: here, only element sets within a month of
    // the time viewed, which leaves out stale ones for satellites that have since decayed. It
    // adds the satellites new to the constellation, and brings up to date those it holds, so
    // that a newer catalog can be loaded over it. A satellite is placed once: the ISS, which the
    // camera's node holds, is left out and reported as CLAIMED. (add() takes SatRecs directly.)
    const huira::ConstellationReport report = constellation.load(
        catalog_path, {.reference_time = time, .max_age = huira::units::Day(30)});
    std::cout << "The constellation holds " << constellation.size()
              << " satellites: " << report.added << " added, " << report.skipped.size()
              << " left out\n";
    for (const huira::SkippedElementSet& skipped : report.skipped) {
        if (skipped.reason == huira::SkipReason::CLAIMED) {
            std::cout << "  " << skipped.catalog_number << ": " << skipped.message << "\n";
        }
    }

    // The camera looks ahead along the ISS's orbit, tilted 12 degrees down, so that the Earth's
    // limb, 20 degrees below the horizontal at the ISS's height, is in the lower part of the
    // frame. The TLE node keeps the TEME frame's axes, so the camera's rotation is built there,
    // from the ISS's position and velocity at the time viewed: the camera's +z axis looks out,
    // and its +y axis points down the image (see example 01).
    const huira::PositionState state = iss.evaluate(time).value();
    const huira::Vec3<double> forward = glm::normalize(state.velocity);
    const huira::Vec3<double> down = glm::normalize(-state.position);
    const double tilt = 12.0 * huira::PI<double>() / 180.0;
    const huira::Vec3<double> look = forward * std::cos(tilt) + down * std::sin(tilt);
    const huira::Vec3<double> image_down = down * std::cos(tilt) - forward * std::sin(tilt);
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(35_mm);
    camera_model.configure_sensor_from_pitch({1920, 1200}, 10_um);
    camera_model.set_fstop(1.4f);
    camera_model.set_sensor_bit_depth(12);
    camera_model.enable_sensor_noise(false);
    camera_model.precompute();
    auto camera = iss_node.new_instance(camera_model);
    camera.set_rotation(huira::Rotation<double>::from_basis_vectors(
        glm::cross(image_down, look), image_down, look));

    // A second's exposure, at f/1.4: satellites a few hundred kilometers away, faint as they are,
    // cross a few pixels, and are drawn as streaks, each with the temporal samples its own motion
    // needs, while the Earth below smears. A satellite SGP4 can't place during the exposure, one
    // that has decayed since its element set most often, is left out of the view with a warning,
    // and listed by skipped_satellites(); a node placed by a TLE, asked for by name, throws
    // instead.
    const huira::Interval exposure = huira::Interval::from_centered(time, 1_s);
    huira::SceneView<TSpectral> scene_view(scene, exposure, camera);
    for (const huira::SkippedSatellite& skipped : scene_view.skipped_satellites()) {
        std::cout << "  left out of the view, " << skipped.catalog_number << ": " << skipped.message
                  << "\n";
    }

    huira::Renderer<TSpectral> renderer;
    renderer.set_samples_per_pixel(64);
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    renderer.render(scene_view, frame_buffer);

    fs::create_directories("output");
    huira::write_image_png("output/15_satellites.png",
                           huira::linear_to_srgb(frame_buffer.sensor_response()));

    // Truth data: the scene view holds every satellite in front of the camera, with where it is
    // in the camera's frame at each of its temporal samples. Projected through the camera model,
    // that is its pixel; those outside the field of view project to nan. Each satellite's
    // irradiance at the camera is there too, as the view resolved it.
    std::size_t in_view = 0;
    std::ofstream csv("output/15_satellites.csv");
    csv << "catalog_number,name,x,y\n";
    for (const huira::UnresolvedInstance<TSpectral>& instance : scene_view.unresolved_instances()) {
        if (instance.constellation_id != constellation.id()) {
            continue;
        }
        const huira::Vec3<float> middle =
            instance.transforms[instance.transforms.size() / 2].position;
        const huira::Pixel pixel = camera_model.try_project_point(middle);
        if (std::isnan(pixel.x)) {
            continue;
        }
        csv << instance.catalog_number << ","
            << constellation.element_set(instance.catalog_number).name() << "," << pixel.x << ","
            << pixel.y << "\n";
        ++in_view;
    }
    std::cout << in_view << " satellites in the field of view; see output/15_satellites.csv\n";
}
