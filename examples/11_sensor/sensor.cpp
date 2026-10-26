// 11 - Sensor
//
// Demonstrates:
//   - The sensor's geometry: resolution, pixel pitch and principal point
//   - Turning photons into digital numbers (DN): quantum efficiency, full well capacity,
//     conversion gain (or gain in dB), bias level and bit depth
//   - Noise: shot noise and read noise (on by default), dark current, and the noise seed
//   - The physical signal chain (huira::Visible8): each pixel collects one value from every
//     spectral bin, as a real photosite does, where huira::RGB fills three color channels
//   - Saving the raw DN: 16-bit FITS with header metadata and a world coordinate system (WCS),
//     and 16-bit TIFF; reading the FITS back
//   - The same frame without noise, for comparison
//   - Calibration maps: each pixel's own dark current, from a master dark and a master bias,
//     and its own bias
//
// Builds on: 10 Optics
// Data: tycho2.hrsc, the Tycho-2 star catalog (see README.md)
//
// Run:
//   ./11_sensor tycho2.hrsc
//
// Writes:
//   output/11_sensor.png             the readout, as a picture
//   output/11_sensor.fits            ...as DN, with a header
//   output/11_sensor.tif             ...as DN
//   output/11_sensor_noiseless.png   the same frame without shot or read noise
//   output/11_sensor_calibrated.png  the frame with each pixel's own dark current and bias

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <vector>

#include "huira/huira.hpp"

namespace fs = std::filesystem;

using namespace huira::units::literals;

// This example renders with huira::Visible8 rather than huira::RGB. huira::RGB is a special case
// for making pictures: it fills the sensor response's red, green and blue channels separately,
// as renderers usually do, though no real photosite works that way. The other spectral types
// (huira::Visible8, huira::SWIR8) run the real signal chain: each pixel collects one pile of
// electrons, from every spectral bin, weighted by the quantum efficiency, so the sensor response
// is one value per pixel, as from a monochrome sensor. Their eight bins are the spectral
// sampling, not color channels; color from them takes filters, with an exposure through each,
// as on a real instrument.
using TSpectral = huira::Visible8;

// The unit vector toward a right ascension and declination, in the J2000 frame.
static huira::Vec3<double> direction(double ra_deg, double dec_deg)
{
    const double ra = ra_deg * huira::PI<double>() / 180.0;
    const double dec = dec_deg * huira::PI<double>() / 180.0;
    return {std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec)};
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: 11_sensor <tycho2.hrsc>\nREADME.md says where to get it.\n";
        return 1;
    }
    const fs::path catalog = argv[1];
    fs::create_directories("output");

    huira::Scene<TSpectral> scene;
    huira::Time time("2026-01-01T00:00:00");
    scene.load_stars(catalog, time, 12.f);

    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(100_mm);
    camera_model.set_fstop(4);

    // The sensor's geometry: 1024 x 768 pixels, 6.5 um square, with the principal point (where
    // the optical axis meets the sensor) given in pixels. Here it is the center, which is also
    // the default; a calibrated camera would give its own.
    const int width = 1024;
    const int height = 768;
    const float cx = static_cast<float>(width - 1) / 2.f;
    const float cy = static_cast<float>(height - 1) / 2.f;
    camera_model.configure_sensor_from_pitch({width, height}, 6.5_um, 6.5_um, cx, cy);

    // How light becomes a digital number (DN), in order:
    //   - quantum efficiency: the fraction of photons that free an electron (one number here, or
    //     a value for each spectral bin: TSpectral::integrate_over_data averages a measured QE
    //     curve over each bin)
    //   - full well capacity: the most electrons a pixel holds before it saturates
    //   - dark current: electrons per second that build up with no light at all
    //   - conversion gain: electrons per DN. (Gain can also be set in decibels with
    //     set_sensor_gain_db, relative to the level set_sensor_unity_db marks as unity.)
    //   - bias level: a constant offset in DN, so noise below it is not clipped at 0
    //   - bit depth: the range of DN, from 0 to 2^bits - 1
    camera_model.set_sensor_quantum_efficiency(0.7);
    camera_model.set_sensor_full_well_capacity(30000.f);
    camera_model.set_sensor_dark_current(5.f);
    camera_model.set_sensor_conversion_gain(2.f);
    camera_model.set_sensor_bias_level(100.f);
    camera_model.set_sensor_bit_depth(14);

    // Noise: shot noise (the randomness of the photons and dark current's electrons) and read
    // noise (electrons RMS, added at readout) are on by default. The seed makes the noise
    // repeatable: the same seed gives the same frames, in the same order.
    camera_model.set_sensor_read_noise(8.f);
    camera_model.enable_sensor_noise(true);
    camera_model.set_sensor_noise_seed(42);

    const double pointing_ra = 56.75; // the Pleiades
    const double pointing_dec = 24.12;
    auto camera = scene.root.new_instance(camera_model);
    camera.look_at(direction(pointing_ra, pointing_dec), huira::Vec3<double>{0.0, 0.0, 1.0});

    const float exposure_seconds = 4.f;
    const huira::Interval exposure =
        huira::Interval::from_centered(time, huira::units::Second(exposure_seconds));
    huira::SceneView<TSpectral> scene_view(scene, exposure, camera);
    huira::Renderer<TSpectral> renderer;

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    renderer.render(scene_view, frame_buffer);

    // The sensor response holds DN / (2^bits - 1), from 0 to 1. As a picture:
    huira::write_image_png("output/11_sensor.png",
                           huira::linear_to_srgb(frame_buffer.sensor_response()));

    // FITS stores the DN themselves (16-bit unsigned here), with a header. The WCS keywords say
    // where the image points: the pointing at the reference pixel, in FITS's pixel convention
    // (1-based, y up), and degrees per pixel (negative in x, since east is left with north up).
    const double degrees_per_pixel = std::atan(6.5e-3 / 100.0) * 180.0 / huira::PI<double>();
    huira::FitsMetadata metadata;
    metadata.object = "Pleiades";
    metadata.telescop = "Huira example 11";
    metadata.instrume = "100 mm f/4, 14-bit";
    metadata.date_obs = time.to_iso_8601();
    metadata.exptime = exposure_seconds;
    metadata.ctype1 = "RA---TAN";
    metadata.ctype2 = "DEC--TAN";
    metadata.crval1 = pointing_ra;
    metadata.crval2 = pointing_dec;
    metadata.crpix1 = static_cast<double>(cx) + 1.0;
    metadata.crpix2 = static_cast<double>(height) - static_cast<double>(cy);
    metadata.cdelt1 = -degrees_per_pixel;
    metadata.cdelt2 = degrees_per_pixel;
    metadata.custom_keywords = {
        {"GAIN", static_cast<double>(camera_model.sensor_conversion_gain()), "e-/DN"},
        {"RDNOISE", static_cast<double>(camera_model.sensor_read_noise()), "e- RMS"},
    };
    metadata.comments = {"Rendered by Huira's sensor example"};
    huira::write_image_fits("output/11_sensor.fits", frame_buffer.sensor_response(), 16, metadata);

    // TIFF stores the DN too, at the bundle's bit depth:
    huira::ImageBundle<float> bundle(frame_buffer.sensor_response());
    bundle.bit_depth = 16;
    huira::write_image_tiff("output/11_sensor.tif", bundle);

    // read_image_fits gives back the image (as DN / (2^bits - 1) again) and its header:
    auto [image, header] = huira::read_image_fits("output/11_sensor.fits");
    const double full_scale = std::pow(2.0, image.sensor_bit_depth()) - 1.0;
    float brightest = 0.f;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            brightest = std::max(brightest, image(x, y));
        }
    }
    std::cout << "Read back " << header.object << ", " << header.exptime << " s: brightest pixel "
              << std::lround(brightest * full_scale) << " DN of " << full_scale << "\n";

    // The same frame without shot and read noise. The dark current and bias are still added: they
    // are not noise.
    camera_model.enable_sensor_noise(false);
    auto noiseless_buffer = camera_model.make_frame_buffer();
    noiseless_buffer.enable_sensor_response();
    renderer.render(scene_view, noiseless_buffer);
    huira::write_image_png("output/11_sensor_noiseless.png",
                           huira::linear_to_srgb(noiseless_buffer.sensor_response()));

    // Calibration maps. A real sensor's pixels differ: each has its own dark current, some far
    // above the rest (hot pixels), and the bias has fixed patterns. Calibration frames measure
    // them: a master bias is the mean of many frames taken with no exposure, so each pixel reads
    // its bias, and a master dark the mean of many taken with the shutter closed, so each pixel
    // reads its bias and its dark current's charge. Here they are made up, in DN, standing in
    // for a real camera's, which huira::read_image_fits() would read.
    const float gain = camera_model.sensor_conversion_gain();
    const double dark_exposure = 30.0;
    std::mt19937 random(7);
    std::normal_distribution<float> column_offset(0.f, 1.5f);
    std::uniform_int_distribution<int> random_x(0, width - 1);
    std::uniform_int_distribution<int> random_y(0, height - 1);

    std::vector<float> column_bias(static_cast<std::size_t>(width)); // a pattern by column
    for (float& bias : column_bias) {
        bias = 100.f + column_offset(random);
    }
    huira::Image<float> rates(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double corner = std::hypot(width - x, height - y); // glow in a corner
            rates(x, y) = static_cast<float>(5.0 + 60.0 * std::exp(-corner / 150.0));
        }
    }
    for (int i = 0; i < 300; ++i) {
        rates(random_x(random), random_y(random)) = 300.f; // hot pixels
    }
    huira::Image<float> master_bias(width, height);
    huira::Image<float> master_dark(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float bias = column_bias[static_cast<std::size_t>(x)];
            master_bias(x, y) = bias;
            master_dark(x, y) =
                bias + static_cast<float>(rates(x, y) * dark_exposure / static_cast<double>(gain));
        }
    }

    // Each pixel's dark current is (dark - bias) * gain / exposure, with the gain of the camera
    // that took the frames:
    const huira::Image<float> dark_map = huira::dark_current_from_master_dark(
        master_dark, master_bias, gain, huira::units::Second(dark_exposure));
    camera_model.set_sensor_dark_current_map(dark_map);

    // The master bias is each pixel's bias, in place of the bias level (BiasMapKind::Absolute,
    // the default); a pattern of offsets from the bias level would be BiasMapKind::Relative.
    // Like the single values, the maps are not noise, and apply with noise off too.
    camera_model.set_sensor_bias_map(master_bias);

    camera_model.enable_sensor_noise(true);
    auto calibrated_buffer = camera_model.make_frame_buffer();
    calibrated_buffer.enable_sensor_response();
    renderer.render(scene_view, calibrated_buffer);
    huira::write_image_png("output/11_sensor_calibrated.png",
                           huira::linear_to_srgb(calibrated_buffer.sensor_response()));
}
