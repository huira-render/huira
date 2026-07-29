#include <cmath>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/psfs/airy_disk.hpp"
#include "huira/cameras/psfs/harvey_shack_scatter.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/units/units.hpp"

using namespace huira;

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
} // namespace

TEST_CASE("HarveyShackScatter profile", "[cameras][psf][scatter]")
{
    SECTION("Convolution kernel is normalized to unit energy per channel")
    {
        HarveyShackScatter<RGB> scatter(2.5f, 0.5f);
        Image<RGB> kernel = scatter.generate_convolution_kernel(64);
        REQUIRE(kernel.width() == 129);
        for (std::size_t c = 0; c < 3; ++c) {
            REQUIRE(std::fabs(kernel_energy(kernel, c) - 1.0) < 1e-4);
        }
    }

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

TEST_CASE("Composite total-system PSF via CameraModel", "[cameras][psf][scatter]")
{
    const int radius = 48;
    const float f_s = 0.05f;
    const float b = 2.5f;
    const float r0 = 0.5f;

    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));
    camera.set_fstop(5.f);
    camera.use_aperture_psf(4, 2);
    camera.set_psf_convolution_radius(radius);

    // Core-only kernel first:
    Image<RGB> core_only = camera.get_psf_convolution_kernel();

    // Enable scatter; the cached kernel must invalidate and rebuild as the composite:
    camera.set_harvey_shack_scatter(f_s, b, r0);
    Image<RGB> composite = camera.get_psf_convolution_kernel();

    REQUIRE(composite.width() == 2 * radius + 1);

    SECTION("Composite conserves unit energy")
    {
        for (std::size_t c = 0; c < 3; ++c) {
            REQUIRE(std::fabs(kernel_energy(composite, c) - 1.0) < 1e-4);
        }
    }

    SECTION("Composite matches (1 - f_s) * core + f_s * wings everywhere")
    {
        HarveyShackScatter<RGB> scatter(b, r0);
        Image<RGB> wings = scatter.generate_convolution_kernel(radius);

        float max_err = 0.f;
        for (int y = 0; y < composite.height(); ++y) {
            for (int x = 0; x < composite.width(); ++x) {
                for (std::size_t c = 0; c < 3; ++c) {
                    const float expected = core_only(x, y)[c] * (1.f - f_s) + wings(x, y)[c] * f_s;
                    max_err = std::max(max_err, std::fabs(composite(x, y)[c] - expected));
                }
            }
        }
        REQUIRE(max_err < 1e-6f);
    }

    SECTION("Disabling scatter invalidates the cache and restores the core kernel")
    {
        camera.disable_harvey_shack_scatter();
        const Image<RGB>& restored = camera.get_psf_convolution_kernel();
        float max_err = 0.f;
        for (int y = 0; y < restored.height(); ++y) {
            for (int x = 0; x < restored.width(); ++x) {
                max_err = std::max(max_err, std::fabs(restored(x, y)[0] - core_only(x, y)[0]));
            }
        }
        REQUIRE(max_err < 1e-7f);
    }
}

TEST_CASE("Scatter-only camera (no core PSF)", "[cameras][psf][scatter]")
{
    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));
    camera.set_harvey_shack_scatter(0.02f, 2.0f, 0.5f);

    SECTION("Requires an explicit convolution radius")
    {
        REQUIRE_THROWS(camera.get_psf_convolution_kernel());
    }

    SECTION("Builds a delta-core composite with unit energy")
    {
        const int radius = 32;
        camera.set_psf_convolution_radius(radius);
        const Image<RGB>& kernel = camera.get_psf_convolution_kernel();
        REQUIRE(kernel.width() == 2 * radius + 1);
        for (std::size_t c = 0; c < 3; ++c) {
            REQUIRE(std::fabs(kernel_energy(kernel, c) - 1.0) < 1e-4);
        }
        // The center pixel holds the unscattered delta plus the wings' center sample:
        REQUIRE(kernel(radius, radius)[0] > 0.9f);
    }
}

TEST_CASE("Wings-only kernel for unresolved sources", "[cameras][psf][scatter]")
{
    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));
    camera.use_aperture_psf(4, 2);
    camera.set_psf_convolution_radius(48);

    SECTION("Requires scattering to be enabled")
    {
        REQUIRE_THROWS(camera.get_psf_wings_kernel());
    }

    SECTION("Wings kernel is the unit-energy scatter component")
    {
        const float b = 2.5f;
        const float r0 = 0.5f;
        camera.set_harvey_shack_scatter(0.05f, b, r0);

        const Image<RGB>& wings = camera.get_psf_wings_kernel();
        REQUIRE(wings.width() == 97);
        for (std::size_t c = 0; c < 3; ++c) {
            REQUIRE(std::fabs(kernel_energy(wings, c) - 1.0) < 1e-4);
        }

        // It must match a directly generated scatter kernel (not scaled by f_s):
        HarveyShackScatter<RGB> scatter(b, r0);
        Image<RGB> expected = scatter.generate_convolution_kernel(48);
        float max_err = 0.f;
        for (int y = 0; y < wings.height(); ++y) {
            for (int x = 0; x < wings.width(); ++x) {
                max_err = std::max(max_err, std::fabs(wings(x, y)[0] - expected(x, y)[0]));
            }
        }
        REQUIRE(max_err < 1e-7f);

        // And the identity used by the renderer must hold:
        //     composite == (1 - f_s) * core + f_s * wings
        camera.disable_harvey_shack_scatter();
        Image<RGB> core = camera.get_psf_convolution_kernel();
        camera.set_harvey_shack_scatter(0.05f, b, r0);
        const Image<RGB>& composite = camera.get_psf_convolution_kernel();
        const Image<RGB>& wings2 = camera.get_psf_wings_kernel();
        max_err = 0.f;
        for (int y = 0; y < composite.height(); ++y) {
            for (int x = 0; x < composite.width(); ++x) {
                const float e = core(x, y)[0] * 0.95f + wings2(x, y)[0] * 0.05f;
                max_err = std::max(max_err, std::fabs(composite(x, y)[0] - e));
            }
        }
        REQUIRE(max_err < 1e-6f);
    }
}
