// 10 - Optics
//
// Demonstrates:
//   - Diffraction: the aperture's pattern, set by its f-number (or diameter), as every star's
//     point spread function (PSF)
//   - Focus: set_focus_distance (or set_focus_diopters, set_focus_sensor_offset), and
//     defocus_blur_radius
//   - Scattered light (set_scatter) and veiling glare (set_veiling_glare)
//   - A measured PSF in place of diffraction (set_measured_psf), and going back (clear_psf)
//   - Viewing a PSF: psf_image, diffraction_image, scatter_image and defocus_image
//   - Lens distortion: Brown-Conrady, OpenCV (from OpenCV's own coefficients) and Owen models,
//     and project_point to see where they move a star
//   - describe(), which summarizes a camera model
//
// Builds on: 09 Stars
// Data: tycho2.hrsc, the Tycho-2 star catalog (see README.md)
//
// Run:
//   ./10_optics tycho2.hrsc
//
// Writes:
//   output/10_optics.png                 the Pleiades, diffraction-limited at f/16
//   output/10_optics_defocus.png         focused at 50 m instead of infinity
//   output/10_optics_scatter.png         with scattered light and veiling glare
//   output/10_optics_psf.png             that PSF, as an image
//   output/10_optics_measured_psf.png    with a measured (here, made-up Gaussian) PSF
//   output/10_optics_distortion.png      a wide lens with barrel distortion

#include <algorithm>
#include <array>
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

// An image scaled so its brightest value is 1.
static huira::Image<huira::RGB> normalized(const huira::Image<huira::RGB>& image)
{
    float peak = 0.f;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            peak = std::max(peak, image(x, y).max());
        }
    }
    huira::Image<huira::RGB> output(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            huira::RGB value = image(x, y);
            value /= peak;
            output(x, y) = value;
        }
    }
    return output;
}

// A made-up measured PSF: a round Gaussian, sampled finely, the same in every channel.
static huira::Image<huira::RGB>
gaussian_psf(int samples_per_pixel, float sigma_pixels, int radius_pixels)
{
    const int n = 2 * radius_pixels * samples_per_pixel + 1;
    huira::Image<huira::RGB> psf(n, n);
    float total = 0.f;
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const float dx = static_cast<float>(x - n / 2) / static_cast<float>(samples_per_pixel);
            const float dy = static_cast<float>(y - n / 2) / static_cast<float>(samples_per_pixel);
            const float value =
                std::exp(-(dx * dx + dy * dy) / (2.f * sigma_pixels * sigma_pixels));
            psf(x, y) = huira::RGB{value};
            total += value;
        }
    }
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            psf(x, y) /= total;
        }
    }
    return psf;
}

static void print_pixel(const std::string& label, const huira::Pixel& pixel)
{
    std::cout << label << "(" << pixel.x << ", " << pixel.y << ")\n";
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: 10_optics <tycho2.hrsc>\nREADME.md says where to get it.\n";
        return 1;
    }
    const fs::path catalog = argv[1];
    fs::create_directories("output");

    huira::Scene<TSpectral> scene;
    huira::Time time("2026-01-01T00:00:00");
    scene.load_stars(catalog, time, 12.f);
    const huira::Interval exposure = huira::Interval::from_centered(time, 2_s);
    huira::Renderer<TSpectral> renderer;

    auto render = [&](const huira::InstanceHandle<TSpectral>& camera,
                      const huira::CameraModelHandle<TSpectral>& camera_model,
                      const std::string& filename) {
        // The camera's optics are rebuilt after any change; doing it here keeps that out of the
        // render.
        camera_model.precompute();
        huira::SceneView<TSpectral> scene_view(
            scene, exposure, camera, huira::ObservationMode::GEOMETRIC_STATE);
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_sensor_response();
        renderer.render(scene_view, frame_buffer);
        huira::write_image_png(fs::path("output") / filename,
                               huira::linear_to_srgb(frame_buffer.sensor_response()));
    };

    // A 200 mm lens on 5.5 um pixels, aimed at the Pleiades: about 1.6 degrees across.
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(200_mm);
    camera_model.configure_sensor_from_pitch({1024, 1024}, 5.5_um);
    auto camera = scene.root.new_instance(camera_model);
    camera.look_at(direction(56.75, 24.12), huira::Vec3<double>{0.0, 0.0, 1.0});

    // Diffraction: by default every star is drawn with the diffraction pattern of the camera's
    // circular aperture, averaged over each spectral bin's wavelengths (so red stars' patterns
    // are larger). At f/16 its first dark ring is about two pixels out. The aperture can be set by
    // its diameter instead (set_aperture_diameter), and diffraction turned off
    // (enable_diffraction(false)), which puts each star's light in the pixel it falls in.
    camera_model.set_fstop(16);
    std::cout << camera_model.describe() << "\n";
    render(camera, camera_model, "10_optics.png");

    // Focus: the camera focuses at infinity by default, where the stars are. Focused at 50 m
    // instead, every star blurs into a disc, the aperture's image out of focus. The same focus
    // can be given in diopters (set_focus_diopters(0.02_dpt)), or as how far the sensor sits
    // from the focal plane (set_focus_sensor_offset).
    camera_model.set_focus_distance(50_m);
    auto blur_radius = camera_model.defocus_blur_radius();
    std::cout << "Defocus blur radius: " << blur_radius << " px\n";
    render(camera, camera_model, "10_optics_defocus.png");
    camera_model.set_focus_diopters(0_dpt); // back to infinity

    // Scattered light: a fraction of each star's light spreads into wide wings, flat within the
    // shoulder angle and falling off beyond it as angle^-slope (and faster beyond an outer
    // angle). Veiling glare spreads a fraction of all the light evenly over the image.
    camera_model.set_scatter(0.05f, 2.5f, 0.005_deg, 2.0_deg);
    camera_model.set_veiling_glare(0.01f);
    render(camera, camera_model, "10_optics_scatter.png");

    // psf_image draws the PSF as the renderer does, centered in an image 2 * radius + 1 pixels
    // wide; diffraction_image, scatter_image and defocus_image draw each part alone.
    huira::write_image_png("output/10_optics_psf.png",
                           huira::linear_to_srgb(normalized(camera_model.psf_image(32))));
    camera_model.disable_scatter();
    camera_model.disable_veiling_glare();

    // A measured PSF replaces the diffraction pattern: an image of the PSF, centered, with how
    // many samples it has per sensor pixel. PointSampled says the values are the PSF's intensity
    // at points (from optical design software, or measured finely); PixelIntegrated would say
    // they are what pixels of this sensor collected, as in a star image. InFocus says the camera's
    // focus should still blur it. Here, a Gaussian 1.5 pixels wide, sampled 4 times per pixel:
    const int samples_per_pixel = 4;
    const huira::Image<huira::RGB> psf = gaussian_psf(samples_per_pixel, 1.5f, 8);
    camera_model.set_measured_psf(psf,
                                  static_cast<float>(samples_per_pixel),
                                  huira::PSFSampling::PointSampled,
                                  huira::PSFFocus::InFocus);
    render(camera, camera_model, "10_optics_measured_psf.png");
    camera_model.clear_psf(); // back to the aperture's diffraction pattern

    // Distortion moves where things land on the image, and matters most far from the center. A
    // wide 16 mm lens shows it, with its own camera model in the same scene:
    auto wide_model = scene.new_camera_model();
    wide_model.set_focal_length(16_mm);
    wide_model.set_fstop(4);
    wide_model.configure_sensor_from_pitch({1024, 1024}, 10_um);
    auto wide_camera = scene.root.new_instance(wide_model);
    wide_camera.look_at(direction(62.0, 20.0), huira::Vec3<double>{0.0, 0.0, 1.0});

    // project_point gives where a point in the camera's frame lands on the image. A star near the
    // corner, with each distortion model in turn:
    const huira::Vec3<float> corner{0.25f, 0.25f, 1.0f};
    print_pixel("No distortion:  ", wide_model.project_point(corner));

    // Brown-Conrady: radial (k1, k2, k3) and tangential (p1, p2) terms, in normalized image
    // coordinates. A negative k1 is barrel distortion.
    const huira::BrownCoefficients brown(-0.2, 0.02, 0.0, 0.0, 0.0);
    wide_model.set_brown_conrady_distortion(brown);
    print_pixel("Brown-Conrady:  ", wide_model.project_point(corner));

    // OpenCV's model, built from the distortion vector cv::calibrateCamera returns
    // (k1, k2, p1, p2, k3, ...):
    const std::array<double, 5> opencv_vector = {-0.2, 0.02, 0.0, 0.0, 0.0};
    wide_model.set_opencv_distortion(huira::OpenCVCoefficients::from_opencv(opencv_vector));
    print_pixel("OpenCV:         ", wide_model.project_point(corner));

    // Owen's model: radial terms (e2 times r^2, e4 times r^4), rotational terms (e1 and e3), and
    // terms linear in x and y (e6 and e5):
    wide_model.set_owen_distortion(huira::OwenCoefficients(0.0, -0.2, 0.0, 0.02, 0.0, 0.0));
    print_pixel("Owen:           ", wide_model.project_point(corner));

    wide_model.set_brown_conrady_distortion(brown);
    render(wide_camera, wide_model, "10_optics_distortion.png");

    // delete_distortion() removes it again.
    wide_model.delete_distortion();
}
