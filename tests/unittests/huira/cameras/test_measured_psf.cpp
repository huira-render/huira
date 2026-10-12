#include <algorithm>
#include <cmath>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/psfs/measured_psf.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/units/units.hpp"

using namespace huira;

namespace {

constexpr float SIGMA_X = 1.5f; // sensor pixels
constexpr float SIGMA_Y = 2.5f;

float gaussian(float x, float y)
{
    return std::exp(-0.5f * ((x * x) / (SIGMA_X * SIGMA_X) + (y * y) / (SIGMA_Y * SIGMA_Y)));
}

// Analytic integral of the Gaussian over a 1x1 sensor pixel centered at (cx, cy):
double gaussian_pixel_integral(double cx, double cy)
{
    const double sq2 = std::sqrt(2.0);
    const double ix =
        std::erf((cx + 0.5) / (sq2 * SIGMA_X)) - std::erf((cx - 0.5) / (sq2 * SIGMA_X));
    const double iy =
        std::erf((cy + 0.5) / (sq2 * SIGMA_Y)) - std::erf((cy - 0.5) / (sq2 * SIGMA_Y));
    return ix * iy;
}

// A 4x-oversampled, centered measurement of the Gaussian, covering +/- 16 sensor pixels:
Image<RGB> make_measurement(float samples_per_pixel = 4.f, int extent_px = 16)
{
    const int n = 2 * static_cast<int>(samples_per_pixel) * extent_px + 1;
    Image<RGB> data(n, n);
    const float center = static_cast<float>(n - 1) * 0.5f;
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const float px = (static_cast<float>(x) - center) / samples_per_pixel;
            const float py = (static_cast<float>(y) - center) / samples_per_pixel;
            data(x, y) = RGB{gaussian(px, py)};
        }
    }
    return data;
}
/// A camera of 25 mm over 23.4 um pixels, drawing unresolved sources with a measured PSF as
/// psf_image() gives them, out to radius pixels around the source, offset as given.
Image<RGB> camera_image(const Image<RGB>& data,
                        float samples_per_pixel,
                        PSFSampling sampling,
                        int radius,
                        float x_offset = 0.f,
                        float y_offset = 0.f)
{
    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));
    camera.set_measured_psf(data, samples_per_pixel, sampling);
    return camera.psf_image(radius, x_offset, y_offset);
}

std::pair<double, double> centroid(const Image<RGB>& image, int radius)
{
    double cx = 0.0;
    double cy = 0.0;
    double total = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double v = static_cast<double>(image(x, y)[0]);
            cx += v * (x - radius);
            cy += v * (y - radius);
            total += v;
        }
    }
    return {cx / total, cy / total};
}
} // namespace

TEST_CASE("MeasuredPSF interpolation and extent", "[cameras][psf][measured]")
{
    Image<RGB> data = make_measurement();
    MeasuredPSF<RGB> psf(data, 4.f, PSFSampling::PointSampled);

    SECTION("Evaluate matches the underlying measurement")
    {
        float max_err = 0.f;
        for (float y = -10.f; y <= 10.f; y += 0.37f) {
            for (float x = -10.f; x <= 10.f; x += 0.41f) {
                max_err = std::max(max_err, std::fabs(psf.evaluate(x, y)[0] - gaussian(x, y)));
            }
        }
        // Bilinear interpolation of a smooth function at 4x oversampling:
        REQUIRE(max_err < 5e-3f);
    }

    SECTION("Evaluate is zero outside the measured extent, which is taken whole")
    {
        REQUIRE(psf.evaluate(17.f, 0.f)[0] == 0.f);
        REQUIRE(psf.evaluate(0.f, -17.f)[0] == 0.f);
        REQUIRE(psf.radius() == 16);
    }

    SECTION("The camera draws it as the pixel-integrated Gaussian")
    {
        const int radius = 12;
        const Image<RGB> image = camera_image(data, 4.f, PSFSampling::PointSampled, radius);

        // The analytic expectation, normalized over the measurement:
        double total = 0.0;
        for (int y = -16; y <= 16; ++y) {
            for (int x = -16; x <= 16; ++x) {
                total += gaussian_pixel_integral(x, y);
            }
        }
        double max_err = 0.0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const double expected = gaussian_pixel_integral(x - radius, y - radius) / total;
                max_err =
                    std::max(max_err, std::fabs(static_cast<double>(image(x, y)[0]) - expected));
            }
        }
        // The cubic spline through samples 4 to a pixel follows the Gaussian closely:
        REQUIRE(max_err < 1e-4);
    }
}

TEST_CASE("A measured PSF's image follows the source's subpixel position",
          "[cameras][psf][measured]")
{
    Image<RGB> data = make_measurement();
    for (const float frac : {0.f, 0.25f, 0.5f, 0.75f}) {
        INFO("offset " << frac);
        const auto [cx, cy] =
            centroid(camera_image(data, 4.f, PSFSampling::PointSampled, 12, frac, 0.f), 12);
        CHECK(std::fabs(cx - static_cast<double>(frac)) < 1e-3);
        CHECK(std::fabs(cy) < 1e-3);
    }
}

TEST_CASE("MeasuredPSF input validation", "[cameras][psf][measured]")
{
    Image<RGB> data = make_measurement();

    constexpr PSFSampling POINTS = PSFSampling::PointSampled;
    REQUIRE_THROWS(MeasuredPSF<RGB>(data, 0.f, POINTS));             // non-positive sampling
    REQUIRE_THROWS(MeasuredPSF<RGB>(Image<RGB>(1, 1), 1.f, POINTS)); // degenerate data
    REQUIRE_THROWS(MeasuredPSF<RGB>(Image<RGB>(5, 5), 4.f, POINTS)); // under a pixel across

    // It reaches its farthest sample: of a 9 x 17 measurement at 2 per pixel, 2 px across from
    // its center and 4 down. It used to be cut at the nearer edge, and at 64 px.
    MeasuredPSF<RGB> psf(Image<RGB>(9, 17, RGB{1.f}), 2.f, POINTS);
    CHECK(psf.radius() == 4);
    MeasuredPSF<RGB> large(make_measurement(1.f, 100), 1.f, POINTS);
    CHECK(large.radius() == 100);
}

TEST_CASE("MeasuredPSF in place of diffraction, with scattered light", "[cameras][psf][measured]")
{
    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));

    Image<RGB> data = make_measurement();
    camera.set_psf<MeasuredPSF<RGB>>(data, 4.f, PSFSampling::PointSampled);
    camera.set_scatter(0.03f, 2.5f, units::Radian(0.5 * 23.4375e-6 / 0.025));

    // All of its light, with the scattered light's within the image:
    const Image<RGB> image = camera.psf_image(24);
    double total = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            total += static_cast<double>(image(x, y)[0]);
        }
    }
    CHECK(total > 0.995);
    CHECK(total < 1.0);
}

TEST_CASE("A pixel-integrated measurement is not integrated over the pixel again",
          "[cameras][psf][measured]")
{
    // A star imaged into a single pixel, at the sensor's own resolution. Taken as the PSF's
    // intensity at points, it is spread over the pixels around it. Before the sampling had to be
    // given, every measurement was taken that way.
    Image<RGB> data(9, 9, RGB{0.f});
    data(4, 4) = RGB{1.f};

    // A source on a pixel's center puts all its light in that pixel:
    const Image<RGB> centered = camera_image(data, 1.f, PSFSampling::PixelIntegrated, 4);
    CHECK(std::fabs(centered(4, 4)[0] - 1.f) < 1e-6f);
    CHECK(std::fabs(centered(5, 4)[0]) < 1e-6f);
    CHECK(std::fabs(centered(4, 5)[0]) < 1e-6f);

    // As point samples, it is not:
    const Image<RGB> points = camera_image(data, 1.f, PSFSampling::PointSampled, 4);
    CHECK(points(4, 4)[0] < 0.9f);
}

TEST_CASE("An oversampled pixel-integrated measurement gives the pixel response it holds",
          "[cameras][psf][measured]")
{
    // An effective PSF: at 4 samples per pixel, the light a pixel centered at each sample
    // receives from a Gaussian star. Sources at quarter-pixel offsets land on the samples.
    const int extent = 16;
    const int n = 2 * 4 * extent + 1;
    Image<RGB> data(n, n);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const double px = (x - (n - 1) / 2) / 4.0;
            const double py = (y - (n - 1) / 2) / 4.0;
            data(x, y) = RGB{static_cast<float>(gaussian_pixel_integral(px, py))};
        }
    }

    // Largest difference from the pixel response of a source frac pixels right of the middle
    // pixel's center (gaussian_pixel_integral() is 4 times the share of the light):
    const int radius = 12;
    auto error = [&](const Image<RGB>& image, double frac) {
        double worst = 0.0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const double expected =
                    0.25 * gaussian_pixel_integral(x - radius - frac, y - radius);
                worst = std::max(worst, std::fabs(static_cast<double>(image(x, y)[0]) - expected));
            }
        }
        return worst;
    };

    for (const float frac : {0.f, 0.25f, 0.5f, 0.75f}) {
        INFO("offset " << frac);
        CHECK(error(camera_image(data, 4.f, PSFSampling::PixelIntegrated, radius, frac), frac) <
              1e-5);

        // Taken as point samples, it is integrated over the pixel a second time, and blurred:
        CHECK(error(camera_image(data, 4.f, PSFSampling::PointSampled, radius, frac), frac) > 5e-4);
    }
}

TEST_CASE("A measured PSF's last row and column are part of it", "[cameras][psf][measured]")
{
    // They were taken as outside the measurement, while the first were not: at the default
    // radius (the measurement's extent) a pixel-integrated PSF lost its right and bottom edge
    // pixels, which moved the centroid of a source on a pixel's center by 0.11 px.
    Image<RGB> data(9, 9);
    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 9; ++x) {
            const float dx = static_cast<float>(x - 4);
            const float dy = static_cast<float>(y - 4);
            data(x, y) = RGB{std::exp(-0.125f * (dx * dx + dy * dy))}; // sigma = 2 px
        }
    }
    MeasuredPSF<RGB> psf(data, 1.f, PSFSampling::PixelIntegrated);
    REQUIRE(psf.radius() == 4);

    CHECK(psf.evaluate(4.f, 0.f)[0] == data(8, 4)[0]);
    CHECK(psf.evaluate(0.f, 4.f)[0] == data(4, 8)[0]);
    CHECK(psf.evaluate(4.01f, 0.f)[0] == 0.f);

    const auto [x, y] = centroid(camera_image(data, 1.f, PSFSampling::PixelIntegrated, 4), 4);
    CHECK(std::fabs(x) < 1e-5);
    CHECK(std::fabs(y) < 1e-5);
}
