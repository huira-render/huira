#include <algorithm>
#include <cmath>

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
} // namespace

TEST_CASE("MeasuredPSF interpolation and extent", "[cameras][psf][measured]")
{
    Image<RGB> data = make_measurement();
    MeasuredPSF<RGB> psf(data, 4.f, PSFSampling::PointSampled, 8, 4);

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

    SECTION("Evaluate is zero outside the measured extent")
    {
        REQUIRE(psf.evaluate(17.f, 0.f)[0] == 0.f);
        REQUIRE(psf.evaluate(0.f, -17.f)[0] == 0.f);
        REQUIRE(psf.measured_radius() == 16);
    }

    SECTION("Convolution kernel matches the analytic pixel-integrated Gaussian")
    {
        const int radius = 12;
        Image<RGB> kernel = psf.generate_convolution_kernel(radius);

        // Build and normalize the analytic expectation over the same support:
        const int dim = 2 * radius + 1;
        std::vector<double> expected(static_cast<std::size_t>(dim) * dim);
        double total = 0.0;
        for (int y = 0; y < dim; ++y) {
            for (int x = 0; x < dim; ++x) {
                const double v = gaussian_pixel_integral(x - radius, y - radius);
                expected[static_cast<std::size_t>(y) * dim + static_cast<std::size_t>(x)] = v;
                total += v;
            }
        }

        float max_err = 0.f;
        for (int y = 0; y < dim; ++y) {
            for (int x = 0; x < dim; ++x) {
                const float e = static_cast<float>(
                    expected[static_cast<std::size_t>(y) * dim + static_cast<std::size_t>(x)] /
                    total);
                max_err = std::max(max_err, std::fabs(kernel(x, y)[0] - e));
            }
        }
        // The dominant error is bilinear interpolation bias, which converges as O(h^2) in
        // the sampling density: measured 1.3e-4 at 4x oversampling, 3.2e-5 at 8x.
        REQUIRE(max_err < 2.5e-4f);
    }
}

TEST_CASE("MeasuredPSF polyphase banks shift the centroid", "[cameras][psf][measured]")
{
    Image<RGB> data = make_measurement();
    const int banks = 4;
    MeasuredPSF<RGB> psf(data, 4.f, PSFSampling::PointSampled, 12, banks);

    // The bank selected for a subpixel fraction f should hold a kernel whose centroid sits
    // at +f relative to the kernel center (matching stamping semantics):
    for (int b = 0; b < banks; ++b) {
        const float frac = static_cast<float>(b) / static_cast<float>(banks);
        const Image<RGB>& kernel = psf.get_kernel(frac + 1e-4f, 1e-4f);

        double cx = 0.0, cy = 0.0, total = 0.0;
        for (int y = 0; y < kernel.height(); ++y) {
            for (int x = 0; x < kernel.width(); ++x) {
                const double v = static_cast<double>(kernel(x, y)[0]);
                cx += v * (x - 12);
                cy += v * (y - 12);
                total += v;
            }
        }
        cx /= total;
        cy /= total;

        REQUIRE(std::fabs(cx - static_cast<double>(frac)) < 0.02);
        REQUIRE(std::fabs(cy) < 0.02);
    }
}

TEST_CASE("MeasuredPSF input validation", "[cameras][psf][measured]")
{
    Image<RGB> data = make_measurement();

    constexpr PSFSampling POINTS = PSFSampling::PointSampled;
    REQUIRE_THROWS(MeasuredPSF<RGB>(data, 0.f, POINTS));     // non-positive sampling
    REQUIRE_THROWS(MeasuredPSF<RGB>(data, 4.f, POINTS, 32)); // radius beyond measured extent
    REQUIRE_THROWS(MeasuredPSF<RGB>(Image<RGB>(1, 1), 1.f, POINTS)); // degenerate data

    // Auto radius selects the measured extent (capped at 64):
    MeasuredPSF<RGB> psf(data, 4.f, POINTS);
    REQUIRE(psf.get_radius() == 16);
}

TEST_CASE("MeasuredPSF as the camera's core PSF", "[cameras][psf][measured]")
{
    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));

    Image<RGB> data = make_measurement();
    camera.set_psf<MeasuredPSF<RGB>>(data, 4.f, PSFSampling::PointSampled);
    camera.set_psf_convolution_radius(24);
    camera.set_harvey_shack_scatter(0.03f, 2.5f, 0.5f);

    const Image<RGB>& kernel = camera.get_psf_convolution_kernel();
    REQUIRE(kernel.width() == 49);

    double total = 0.0;
    for (int y = 0; y < kernel.height(); ++y) {
        for (int x = 0; x < kernel.width(); ++x) {
            total += static_cast<double>(kernel(x, y)[0]);
        }
    }
    REQUIRE(std::fabs(total - 1.0) < 1e-4);
}

TEST_CASE("A pixel-integrated measurement is not integrated over the pixel again",
          "[cameras][psf][measured]")
{
    // A star imaged into a single pixel, at the sensor's own resolution. Taken as the PSF's
    // intensity at points, interpolation makes it a 2 px tent, and integrating that over each
    // pixel keeps only 0.75^2 of the light in the center pixel. Before the sampling had to be
    // given, every measurement was taken that way.
    Image<RGB> data(9, 9, RGB{0.f});
    data(4, 4) = RGB{1.f};
    MeasuredPSF<RGB> pixels(data, 1.f, PSFSampling::PixelIntegrated, 4, 4);
    MeasuredPSF<RGB> points(data, 1.f, PSFSampling::PointSampled, 4, 4);

    // A source on a pixel's center (bank 0) puts all its light in that pixel, in the stamp and
    // in the convolution kernel:
    const Image<RGB>& centered = pixels.get_kernel(0.f, 0.f);
    CHECK(std::fabs(centered(4, 4)[0] - 1.f) < 1e-6f);
    CHECK(centered(5, 4)[0] == 0.f);
    CHECK(centered(4, 5)[0] == 0.f);
    const Image<RGB> kernel = pixels.generate_convolution_kernel(4);
    CHECK(std::fabs(kernel(4, 4)[0] - 1.f) < 1e-6f);
    CHECK(kernel(5, 4)[0] == 0.f);

    // Halfway between two pixel centers (bank 2 of 4), it shares the light between them:
    const Image<RGB>& halfway = pixels.get_kernel(0.5f, 0.f);
    CHECK(std::fabs(halfway(4, 4)[0] - 0.5f) < 1e-6f);
    CHECK(std::fabs(halfway(5, 4)[0] - 0.5f) < 1e-6f);

    // As point samples:
    CHECK(std::fabs(points.get_kernel(0.f, 0.f)(4, 4)[0] - 0.5625f) < 2e-3f);
    CHECK(std::fabs(points.generate_convolution_kernel(4)(4, 4)[0] - 0.5625f) < 2e-3f);
}

TEST_CASE("An oversampled pixel-integrated measurement gives the pixel response it holds",
          "[cameras][psf][measured]")
{
    // An effective PSF: at 4 samples per pixel, the light a pixel centered at each sample
    // receives from a Gaussian star. Stamps at quarter-pixel offsets land on the samples.
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

    const int radius = 12;
    const int banks = 4;
    MeasuredPSF<RGB> pixels(data, 4.f, PSFSampling::PixelIntegrated, radius, banks);
    MeasuredPSF<RGB> points(data, 4.f, PSFSampling::PointSampled, radius, banks);

    // Largest difference from the pixel response of a source frac pixels right of the middle
    // pixel's center, normalized over the stamp:
    auto error = [&](const Image<RGB>& stamp, double frac) {
        double total = 0.0;
        for (int y = 0; y < stamp.height(); ++y) {
            for (int x = 0; x < stamp.width(); ++x) {
                total += gaussian_pixel_integral(x - radius - frac, y - radius);
            }
        }
        double worst = 0.0;
        for (int y = 0; y < stamp.height(); ++y) {
            for (int x = 0; x < stamp.width(); ++x) {
                const double expected = gaussian_pixel_integral(x - radius - frac, y - radius);
                worst = std::max(worst,
                                 std::fabs(static_cast<double>(stamp(x, y)[0]) - expected / total));
            }
        }
        return worst;
    };

    for (int b = 0; b < banks; ++b) {
        const float frac = static_cast<float>(b) / static_cast<float>(banks);
        INFO("bank " << b);
        CHECK(error(pixels.get_kernel(frac + 1e-4f, 1e-4f), frac) < 1e-6);

        // Taken as point samples, it is integrated over the pixel a second time, and blurred:
        CHECK(error(points.get_kernel(frac + 1e-4f, 1e-4f), frac) > 5e-4);
    }
}
