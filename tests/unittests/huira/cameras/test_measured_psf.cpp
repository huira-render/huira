#include <algorithm>
#include <cmath>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/optics.hpp"
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
    MeasuredPSF<RGB> psf(data, 4.f, 8, 4);

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
    MeasuredPSF<RGB> psf(data, 4.f, 12, banks);

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

    REQUIRE_THROWS(MeasuredPSF<RGB>(data, 0.f));             // non-positive sampling
    REQUIRE_THROWS(MeasuredPSF<RGB>(data, 4.f, 32));         // radius beyond measured extent
    REQUIRE_THROWS(MeasuredPSF<RGB>(Image<RGB>(1, 1), 1.f)); // degenerate data

    // Auto radius selects the measured extent (capped at 64):
    MeasuredPSF<RGB> psf(data, 4.f);
    REQUIRE(psf.get_radius() == 16);
}

TEST_CASE("MeasuredPSF as the camera's core PSF", "[cameras][psf][measured]")
{
    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));

    MeasuredCore<RGB> core;
    core.data = make_measurement();
    core.samples_per_pixel = 4.f;

    HarveyShack scatter;
    scatter.fraction = 0.03f;
    scatter.exponent = 2.5f;
    scatter.r0 = 0.5f;
    scatter.kernel_radius = 24;

    camera.set_core(core);
    camera.set_scatter(scatter);

    const Image<RGB>& kernel = camera.psf_convolution_kernel();
    REQUIRE(kernel.width() == 49);

    double total = 0.0;
    for (int y = 0; y < kernel.height(); ++y) {
        for (int x = 0; x < kernel.width(); ++x) {
            total += static_cast<double>(kernel(x, y)[0]);
        }
    }
    REQUIRE(std::fabs(total - 1.0) < 1e-4);
}
