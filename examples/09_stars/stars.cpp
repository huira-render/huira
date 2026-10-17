// 09 - Stars
//
// Demonstrates:
//   - Loading stars, down to a faintest magnitude, with their positions at an epoch
//   - The scene's axes: x, y and z of the J2000 (ICRF) frame, so a right ascension and
//     declination give a direction
//   - Pointing a camera at a direction with look_at, with celestial north up
//   - Exposing for stars: seconds rather than milliseconds, and a fast lens
//   - Dynamic stars: moving the catalog to another epoch with update_star_epoch, without
//     reloading it (Barnard's star, which moves fastest across the sky, over a century)
//
// Builds on: 01 First render
// Data: tycho2.hrsc, the Tycho-2 star catalog (see README.md)
//
// Run:
//   ./09_stars /path/to/tycho2.hrsc
//
// Writes:
//   output/09_stars.png                 Orion
//   output/09_stars_barnard_2000.png    Barnard's star in 2000...
//   output/09_stars_barnard_2100.png    ...and in 2100

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

using TSpectral = huira::RGB;

// The unit vector toward a right ascension and declination, in the J2000 frame.
static huira::Vec3<double> direction(double ra_deg, double dec_deg)
{
    const double ra = ra_deg * huira::PI<double>() / 180.0;
    const double dec = dec_deg * huira::PI<double>() / 180.0;
    return {std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec)};
}

static void render(huira::Scene<TSpectral>& scene,
                   const huira::InstanceHandle<TSpectral>& camera,
                   const huira::CameraModelHandle<TSpectral>& camera_model,
                   const huira::Interval& exposure,
                   const std::string& filename)
{
    huira::SceneView<TSpectral> scene_view(
        scene, exposure, camera, huira::ObservationMode::GEOMETRIC_STATE);
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    huira::Renderer<TSpectral> renderer;
    renderer.render(scene_view, frame_buffer);

    fs::create_directories("output");
    huira::write_image_png(fs::path("output") / filename,
                           huira::linear_to_srgb(frame_buffer.sensor_response()));
}

static void orion(const fs::path& catalog)
{
    huira::Scene<TSpectral> scene;

    // load_stars reads the catalog, keeping the stars down to the faintest magnitude given, and
    // moves each by its proper motion to the time given. Each star's spectrum comes from its
    // temperature, so stars have their colors.
    huira::Time time("2026-01-01T00:00:00");
    scene.load_stars(catalog, time, 11.f);

    // A 35 mm lens at f/2.8 on a sensor of 10 um pixels: about 25 x 16 degrees.
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(35_mm);
    camera_model.set_fstop(2.8f);
    camera_model.configure_sensor_from_pitch({1536, 1024}, 10_um);

    // The scene's axes are those of J2000, so a direction from right ascension and declination
    // can be looked at directly (look_at takes a position; a unit vector is a point 1 m away).
    // Up is the celestial north pole, +z. Orion's belt is at about RA 84, Dec -1 degrees:
    auto camera = scene.root.new_instance(camera_model);
    camera.look_at(direction(84.0, -1.0), huira::Vec3<double>{0.0, 0.0, 1.0});

    // Stars are faint: expose for seconds.
    render(scene, camera, camera_model, huira::Interval::from_centered(time, 2_s), "09_stars.png");
}

static void barnards_star(const fs::path& catalog)
{
    huira::Scene<TSpectral> scene;

    // load_dynamic_stars keeps the catalog's proper motions, so update_star_epoch can move every
    // star to another time later, without reading the catalog again.
    huira::Time year_2000("2000-01-01T12:00:00");
    huira::Time year_2100("2100-01-01T12:00:00");
    scene.load_dynamic_stars(catalog, year_2000, 12.f);

    // A 300 mm lens: about 1 degree across. Barnard's star (RA 269.45, Dec 4.69 in 2000) moves
    // about 10 arcseconds a year, so a century carries it 17 arcminutes north: about 150 pixels.
    // The camera points halfway along that path.
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(300_mm);
    camera_model.set_fstop(4);
    camera_model.configure_sensor_from_pitch({512, 512}, 10_um);

    auto camera = scene.root.new_instance(camera_model);
    camera.look_at(direction(269.45, 4.83), huira::Vec3<double>{0.0, 0.0, 1.0});

    render(scene,
           camera,
           camera_model,
           huira::Interval::from_centered(year_2000, 5_s),
           "09_stars_barnard_2000.png");

    scene.update_star_epoch(year_2100);
    render(scene,
           camera,
           camera_model,
           huira::Interval::from_centered(year_2100, 5_s),
           "09_stars_barnard_2100.png");
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: 09_stars <tycho2.hrsc>\nREADME.md says where to get it.\n";
        return 1;
    }
    const fs::path catalog = argv[1];

    orion(catalog);
    barnards_star(catalog);
}
