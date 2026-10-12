#include <algorithm>
#include <cmath>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/psfs/airy_disk.hpp"
#include "huira/cameras/psfs/harvey_shack_scatter.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/units/units.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {
double kernel_energy(const Image<RGB>& kernel, std::size_t channel)
{
    double total = 0.0;
    for (int y = 0; y < kernel.height(); ++y) {
        for (int x = 0; x < kernel.width(); ++x) {
            total += static_cast<double>(kernel(x, y)[channel]);
        }
    }
    return total;
}

double peak(const Image<RGB>& image)
{
    double most = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            most = std::max(most, static_cast<double>(image[i][c]));
        }
    }
    return most;
}

/// The area of [x0, x1] x [y0, y1] within the circle of radius r about the origin, independently
/// of the renderer: the rectangle's width within the circle's chord, integrated over the height
/// y = r sin(theta) by the midpoint rule, fine enough for 1e-9.
double disc_area_reference(double x0, double x1, double y0, double y1, double r)
{
    const double from = std::asin(std::clamp(y0 / r, -1.0, 1.0));
    const double to = std::asin(std::clamp(y1 / r, -1.0, 1.0));
    constexpr int STEPS = 200000;
    const double step = (to - from) / STEPS;
    double area = 0.0;
    for (int k = 0; k < STEPS; ++k) {
        const double theta = from + (k + 0.5) * step;
        const double half_chord = r * std::cos(theta);
        const double width = std::min(x1, half_chord) - std::max(x0, -half_chord);
        area += std::max(width, 0.0) * half_chord * step;
    }
    return area;
}

/// 25 mm at f/5 with 6 mm across 256 pixels: 23.4 um pixels, a diffraction pattern well under
/// a pixel, so that its images are small.
void configure(CameraModel<RGB>& camera)
{
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));
    camera.set_fstop(5.f);
}
} // namespace

TEST_CASE("HarveyShackScatter profile", "[cameras][psf][scatter]")
{
    SECTION("Wings fall off with the configured power-law exponent")
    {
        const float b = 2.0f;
        const float r0 = 0.5f;
        HarveyShackScatter<RGB> scatter(b, r0);

        // Far from the shoulder, evaluate(r) ~ r^-b, so the ratio between two radii should
        // match (r1/r2)^b:
        const float v50 = scatter.evaluate(50.f, 0.f)[0];
        const float v100 = scatter.evaluate(100.f, 0.f)[0];
        const float expected_ratio = std::pow(100.f / 50.f, b); // = 4
        REQUIRE(std::fabs((v50 / v100) / expected_ratio - 1.f) < 0.01f);
    }

    SECTION("Cutoff radius zeroes the profile beyond it")
    {
        HarveyShackScatter<RGB> scatter(2.0f, 0.5f, 30.f);
        REQUIRE(scatter.evaluate(29.f, 0.f)[0] > 0.f);
        REQUIRE(scatter.evaluate(31.f, 0.f)[0] == 0.f);
        REQUIRE(scatter.evaluate(0.f, 31.f)[0] == 0.f);
    }

    SECTION("Invalid parameters are rejected")
    {
        REQUIRE_THROWS(HarveyShackScatter<RGB>(0.f, 0.5f));       // non-positive exponent
        REQUIRE_THROWS(HarveyShackScatter<RGB>(2.f, 0.f));        // non-positive r0
        REQUIRE_THROWS(HarveyShackScatter<RGB>(2.f, 0.5f, -1.f)); // negative cutoff
    }
}

TEST_CASE("psf_image() is diffraction_image() and scatter_image() mixed by the scattered "
          "fraction",
          "[cameras][psf][scatter]")
{
    constexpr int RADIUS = 24;
    constexpr float FRACTION = 0.05f;
    CameraModel<RGB> camera;
    configure(camera);
    const Image<RGB> diffraction = camera.diffraction_image(RADIUS, 0.3f, -0.2f);
    // About a thousandth of the light is in the rings beyond the image:
    for (std::size_t c = 0; c < 3; ++c) {
        CHECK(kernel_energy(diffraction, c) > 0.997);
        CHECK(kernel_energy(diffraction, c) < 1.0);
    }

    // Without scattered light, the PSF is the diffraction pattern, and there is no scattered
    // light to show:
    const Image<RGB> unscattered = camera.psf_image(RADIUS, 0.3f, -0.2f);
    CHECK_THROWS(camera.scatter_image(RADIUS));
    double worst = 0.0;
    for (std::size_t i = 0; i < diffraction.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            worst = std::max(worst,
                             std::fabs(static_cast<double>(unscattered[i][c] - diffraction[i][c])));
        }
    }
    CHECK(worst < 1e-6 * peak(diffraction));

    // With it, the PSF holds 1 - f of the pattern and f of the scattered light, which on its own
    // holds all of its light:
    camera.set_scatter(FRACTION, 2.5f, units::Arcsecond(200.0));
    CHECK(camera.scatter_enabled());
    const Image<RGB> scatter = camera.scatter_image(RADIUS, 0.3f, -0.2f);
    const Image<RGB> composite = camera.psf_image(RADIUS, 0.3f, -0.2f);
    // Its shoulder is 1.03 px, and as it falls as r^-2.5, about (1.03 / R)^0.5 of it lies beyond
    // R, here the radius of a disc as large as the image, 49 / sqrt(pi) px:
    const double beyond = std::sqrt(1.03 / (49.0 / std::sqrt(3.14159265358979323846)));
    for (std::size_t c = 0; c < 3; ++c) {
        CHECK(std::fabs(kernel_energy(scatter, c) - (1.0 - beyond)) < 0.005);
    }
    worst = 0.0;
    for (std::size_t i = 0; i < composite.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            const double expected = (1.0 - FRACTION) * static_cast<double>(diffraction[i][c]) +
                                    FRACTION * static_cast<double>(scatter[i][c]);
            worst = std::max(worst, std::fabs(static_cast<double>(composite[i][c]) - expected));
        }
    }
    CHECK(worst < 1e-4 * peak(composite));

    // Turning it off brings the pattern back:
    camera.disable_scatter();
    CHECK_FALSE(camera.scatter_enabled());
    const Image<RGB> restored = camera.psf_image(RADIUS, 0.3f, -0.2f);
    worst = 0.0;
    for (std::size_t i = 0; i < restored.size(); ++i) {
        worst = std::max(worst, std::fabs(static_cast<double>(restored[i][0] - unscattered[i][0])));
    }
    CHECK(worst < 1e-7);
}

TEST_CASE("Without diffraction, a point keeps the light that is not scattered",
          "[cameras][psf][scatter]")
{
    constexpr int RADIUS = 16;
    CameraModel<RGB> camera;
    configure(camera);
    camera.enable_diffraction(false);
    CHECK_FALSE(camera.diffraction_enabled());
    camera.set_scatter(0.02f, 2.5f, units::Arcsecond(200.0));

    const Image<RGB> image = camera.psf_image(RADIUS);
    const Image<RGB> scatter = camera.scatter_image(RADIUS);
    double worst = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double point = (x == RADIUS && y == RADIUS) ? 0.98 : 0.0;
            const double expected = point + 0.02 * static_cast<double>(scatter(x, y)[0]);
            worst = std::max(worst, std::fabs(static_cast<double>(image(x, y)[0]) - expected));
        }
    }
    CHECK(worst < 1e-6);

    // The diffraction pattern can still be seen on its own:
    CHECK(kernel_energy(camera.diffraction_image(RADIUS), 0) > 0.99);
}

TEST_CASE("defocus_image() is the uniform disc of defocus_blur_radius()", "[cameras][psf][defocus]")
{
    constexpr int RADIUS = 12;
    CameraModel<RGB> camera;
    configure(camera);

    // In focus, the point falls in one pixel:
    const Image<RGB> focused = camera.defocus_image(RADIUS, 0.3f, 0.7f);
    CHECK(focused(RADIUS, RADIUS + 1)[0] == 1.f);
    CHECK(kernel_energy(focused, 0) == 1.0);

    // 25 mm at f/5 is 5 mm across: 1 diopter blurs a star 25 mm * 2.5 mm / 23.4 um = 2.67 px.
    camera.set_focus_diopters(units::Diopter(3.0));
    const double blur = static_cast<double>(camera.defocus_blur_radius());
    CHECK(std::fabs(blur - 3.0 * 0.025 * 0.0025 / (6e-3 / 256.0)) < 1e-4);
    const Image<RGB> disc = camera.defocus_image(RADIUS, 0.3f, -0.2f);
    double worst = 0.0;
    for (int y = 0; y < disc.height(); ++y) {
        for (int x = 0; x < disc.width(); ++x) {
            const double dx = x - RADIUS - 0.3;
            const double dy = y - RADIUS + 0.2;
            const double expected = grid_psf_detail::circle_rectangle_area(
                                        dx - 0.5, dx + 0.5, dy - 0.5, dy + 0.5, blur) /
                                    (3.14159265358979323846 * blur * blur);
            worst = std::max(worst, std::fabs(static_cast<double>(disc(x, y)[0]) - expected));
        }
    }
    CHECK(worst < 1e-6);
    CHECK(std::fabs(kernel_energy(disc, 0) - 1.0) < 1e-5);

    // A blur under half a pixel is the one sources are drawn with, not none, as it was given:
    camera.set_focus_diopters(units::Diopter(0.1));
    CHECK(std::fabs(camera.defocus_blur_radius() - 0.1 * 0.025 * 0.0025 / (6e-3 / 256.0)) < 1e-5);
    camera.set_focus_diopters(units::Diopter(3.0));

    // A point focused on has none, and one nearer than that more:
    CHECK(camera.defocus_blur_radius(units::Meter(1.0 / 3.0)) == 0.f);
    CHECK(camera.defocus_blur_radius(units::Meter(0.1)) > camera.defocus_blur_radius());
    CHECK_THROWS(camera.defocus_image(RADIUS, 0.f, 0.f, units::Meter(0.0)));
    CHECK_THROWS(camera.defocus_blur_radius(units::Meter(-1.0)));
}

TEST_CASE("A disc's light in a pixel is exact where the disc's edge touches the pixel's",
          "[cameras][psf][defocus]")
{
    // Where the disc's edge just touched one of the pixel's sides, the pixel was given the
    // rectangle's area within the disc's height rather than the disc's: a disc of exactly half a
    // pixel, centered, put 4/pi = 1.273 of its light in its pixel, and samples of a larger disc
    // whose edge touched a pixel's were off by a few percent. 62.5 mm at f/1 with pixels of
    // 2^-16 m blurs a point at 128 m by exactly 1 px, focused at infinity, and every blur here is
    // exact in binary.
    constexpr int SIZE = 64;
    CameraModel<RGB> camera;
    camera.set_focal_length(units::Meter(0.0625));
    camera.configure_sensor_from_pitch(Resolution{SIZE, SIZE}, units::Meter(std::ldexp(1.0, -16)));
    camera.set_fstop(1.f);
    camera.enable_diffraction(false);
    for (const double blur : {0.5, 1.0, 2.0}) {
        const units::Meter range(128.0 / blur);
        REQUIRE(camera.defocus_blur_radius(range) == static_cast<float>(blur));
        // The source's offset from the center pixel's center at which the disc's edge touches
        // that pixel's side:
        const double offset = blur - 0.5;
        const int radius = static_cast<int>(std::ceil(blur)) + 2;
        for (const bool defocus_only : {false, true}) {
            INFO("blur " << blur << " px, offset " << offset << " px, "
                         << (defocus_only ? "defocus_image()" : "psf_image()"));
            const Image<RGB> image =
                defocus_only ? camera.defocus_image(radius, static_cast<float>(offset), 0.f, range)
                             : camera.psf_image(radius, static_cast<float>(offset), 0.f, range);
            double worst = 0.0;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const double dx = x - radius - offset;
                    const double dy = y - radius;
                    const double expected =
                        disc_area_reference(dx - 0.5, dx + 0.5, dy - 0.5, dy + 0.5, blur) /
                        (3.14159265358979323846 * blur * blur);
                    worst =
                        std::max(worst, std::fabs(static_cast<double>(image(x, y)[0]) - expected));
                }
            }
            // defocus_image() is the disc's exact area; psf_image() draws a disc of half a pixel
            // or more from samples of it, to within 1e-4 of the light:
            CHECK(worst < (defocus_only ? 1e-6 : 1e-4));
        }
    }
}

TEST_CASE("Without diffraction or a set PSF, psf_image() is defocus_image(), however small the "
          "blur",
          "[cameras][psf][defocus]")
{
    // Before, a disc under about a quarter of a pixel was drawn from a spline through samples of
    // its light, which rang across the pixels' edges: at 0.003 px, a pixel was off by a tenth of
    // the light, and the image summed to 1.008 or 0.976 of it, depending on the offset.
    constexpr int RADIUS = 4;
    CameraModel<RGB> camera;
    configure(camera);
    camera.enable_diffraction(false);
    // 2.67 px per diopter (see the test case above):
    const double per_diopter = 0.025 * 0.0025 / (6e-3 / 256.0);
    for (const double blur : {0.0, 0.003, 0.03, 0.3, 0.7}) {
        camera.set_focus_diopters(units::Diopter(blur / per_diopter));
        for (const float offset : {0.f, 0.21f, 0.49f, 0.5f}) {
            INFO("blur " << blur << " px, offset " << offset);
            const Image<RGB> image = camera.psf_image(RADIUS, offset, -0.37f);
            const Image<RGB> disc = camera.defocus_image(RADIUS, offset, -0.37f);
            double worst = 0.0;
            for (std::size_t i = 0; i < image.size(); ++i) {
                worst = std::max(worst, std::fabs(static_cast<double>(image[i][0] - disc[i][0])));
            }
            // Larger discs are drawn from samples, to within 1e-4 of the light:
            CHECK(worst < (blur < GridPsf<RGB>::EXACT_DISC ? 1e-6 : 1e-4));
            CHECK(std::fabs(kernel_energy(image, 0) - 1.0) < 1e-4);
        }
    }
}

TEST_CASE("A PSF set without a radius holds its light within the default one", "[cameras][psf]")
{
    // HarveyShackScatter, like a user's own PSF class, does not set its radius: the camera gives
    // it DEFAULT_PSF_RADIUS, within which it holds all of a source's light.
    CameraModel<RGB> camera;
    configure(camera);
    camera.set_psf<HarveyShackScatter<RGB>>(2.5f, 0.5f);
    CHECK(camera.has_set_psf());

    constexpr int RADIUS = CameraModel<RGB>::DEFAULT_PSF_RADIUS;
    const Image<RGB> image = camera.psf_image(RADIUS + 4);
    CHECK(std::fabs(kernel_energy(image, 0) - 1.0) < 1e-4);
    CHECK(image(RADIUS + 4, 1)[0] == 0.f);
    CHECK(image(RADIUS + 4, 5)[0] > 0.f);

    // An Airy disk sets its own:
    camera.set_psf<AiryDisk<RGB>>(units::Millimeter(25.0),
                                  units::Micrometer(23.4375),
                                  units::Micrometer(23.4375),
                                  units::Millimeter(5.0),
                                  8);
    const Image<RGB> airy = camera.psf_image(12);
    CHECK(std::fabs(kernel_energy(airy, 0) - 1.0) < 1e-4);
    CHECK(airy(0, 12)[0] == 0.f);
}
