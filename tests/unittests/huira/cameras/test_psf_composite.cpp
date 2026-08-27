#include <algorithm>
#include <cmath>
#include <variant>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/optics.hpp"
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

    DiffractionCore core;
    core.radius = 4;
    core.banks = 2;

    HarveyShack scatter_params;
    scatter_params.fraction = f_s;
    scatter_params.exponent = b;
    scatter_params.r0 = r0;
    scatter_params.kernel_radius = radius;

    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));
    camera.set_fstop(5.f);
    camera.set_core(core);

    // Core-only kernel first. Without scatter the kernel spans the stamping radius, so
    // request the same span explicitly:
    HarveyShack zero_scatter = scatter_params;
    zero_scatter.fraction = 0.f;
    camera.set_scatter(zero_scatter);
    Image<RGB> core_only = camera.psf_convolution_kernel();

    // Add scatter; the cached kernel must invalidate and rebuild as the composite:
    camera.set_scatter(scatter_params);
    Image<RGB> composite = camera.psf_convolution_kernel();

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

    SECTION("Removing scatter invalidates the cache and restores the core kernel")
    {
        camera.set_scatter(zero_scatter);
        const Image<RGB>& restored = camera.psf_convolution_kernel();
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
    const int radius = 32;

    HarveyShack scatter_params;
    scatter_params.fraction = 0.02f;
    scatter_params.exponent = 2.5f;
    scatter_params.r0 = 0.5f;
    scatter_params.kernel_radius = radius;

    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));
    camera.set_optics(Optics<RGB>::ideal());
    camera.set_scatter(scatter_params);

    SECTION("Builds a delta-core composite with unit energy")
    {
        const Image<RGB>& kernel = camera.psf_convolution_kernel();
        REQUIRE(kernel.width() == 2 * radius + 1);
        for (std::size_t c = 0; c < 3; ++c) {
            REQUIRE(std::fabs(kernel_energy(kernel, c) - 1.0) < 1e-4);
        }
        // The center pixel holds the unscattered delta plus the wings' center sample:
        REQUIRE(kernel(radius, radius)[0] > 0.9f);
    }

    SECTION("Ideal optics have no kernel at all")
    {
        camera.set_ideal_optics();
        REQUIRE_THROWS(camera.psf_convolution_kernel());
    }
}

TEST_CASE("Optics validation", "[cameras][psf][scatter]")
{
    SECTION("An exponent at or below 2 carries unbounded energy")
    {
        HarveyShack scatter;
        scatter.fraction = 0.02f;
        scatter.exponent = 2.0f;
        REQUIRE_THROWS(scatter.validate());

        // A cutoff makes the truncated profile integrable again:
        scatter.cutoff_radius = 64.f;
        REQUIRE_NOTHROW(scatter.validate());
    }

    SECTION("The stray-light budget must leave energy in the core")
    {
        StrayLight stray;
        stray.veiling_glare = 0.6f;
        HarveyShack scatter;
        scatter.fraction = 0.5f;
        stray.scatter = scatter;
        REQUIRE_THROWS(stray.validate());
    }

    SECTION("Enclosed energy and its inverse agree")
    {
        const float b = 2.5f;
        const float r0 = 0.5f;
        const float radius = HarveyShack::radius_for_energy(0.95f, b, r0);
        REQUIRE(std::fabs(HarveyShack::energy_within(radius, b, r0) - 0.95f) < 1e-3f);
    }
}

TEST_CASE("Optics presets", "[cameras][psf][scatter]")
{
    SECTION("ideal() has no core and no stray light")
    {
        Optics<RGB> optics = Optics<RGB>::ideal();
        REQUIRE(std::holds_alternative<IdealCore>(optics.core));
        REQUIRE_FALSE(optics.stray_light.scatter.has_value());
        REQUIRE(optics.stray_light.veiling_glare == 0.f);
    }

    SECTION("diffraction_limited() has a core and no stray light")
    {
        Optics<RGB> optics = Optics<RGB>::diffraction_limited();
        REQUIRE(std::holds_alternative<DiffractionCore>(optics.core));
        REQUIRE_FALSE(optics.stray_light.scatter.has_value());
    }

    SECTION("realistic() adds scatter and glare, and validates")
    {
        Optics<RGB> optics = Optics<RGB>::realistic();
        REQUIRE(std::holds_alternative<DiffractionCore>(optics.core));
        REQUIRE(optics.stray_light.scatter.has_value());
        REQUIRE(optics.stray_light.veiling_glare > 0.f);
        REQUIRE_NOTHROW(optics.validate());
    }
}

TEST_CASE("Optical kernels survive reconfiguration in any order", "[cameras][psf]")
{
    // Setting the optics before the sensor must give the same result as setting them after,
    // since the kernels are built lazily from whatever the camera holds at first use.
    DiffractionCore core;
    core.radius = 6;
    core.banks = 2;

    CameraModel<RGB> before;
    before.set_core(core);
    before.set_focal_length(units::Millimeter(25.0));
    before.configure_sensor_from_pitch(Resolution{128, 128}, units::Micrometer(3.0));
    before.set_fstop(5.f);

    CameraModel<RGB> after;
    after.set_focal_length(units::Millimeter(25.0));
    after.configure_sensor_from_pitch(Resolution{128, 128}, units::Micrometer(3.0));
    after.set_fstop(5.f);
    after.set_core(core);

    const Image<RGB>& a = before.psf_convolution_kernel();
    const Image<RGB>& b = after.psf_convolution_kernel();

    REQUIRE(a.width() == b.width());
    float max_err = 0.f;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            max_err = std::max(max_err, std::fabs(a(x, y)[0] - b(x, y)[0]));
        }
    }
    REQUIRE(max_err < 1e-7f);
}

TEST_CASE("Focus parameterization", "[cameras][focus]")
{
    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(125.0));
    camera.configure_sensor_from_pitch(Resolution{256, 256}, units::Micrometer(8.5));
    camera.set_fstop(3.3f);

    SECTION("Distance and vergence are the same setting")
    {
        camera.set_focus(units::Meter(400.0));
        REQUIRE(std::fabs(camera.focus_vergence().to_si_f() - 0.0025f) < 1e-6f);

        camera.set_focus(units::Diopter(0.0025));
        REQUIRE(std::fabs(camera.focus_distance().to_si_f() - 400.f) < 1e-2f);
    }

    SECTION("Zero vergence focuses at infinity and produces no blur")
    {
        camera.set_focus(units::Diopter(0.0));
        REQUIRE(std::isinf(camera.focus_distance().to_si_f()));
        REQUIRE(camera.defocus_blur_pixels() == 0.f);
        camera.build_optics();
        REQUIRE_FALSE(camera.has_defocus());
    }

    SECTION("Focusing beyond infinity blurs by the same amount as focusing nearer")
    {
        camera.set_focus(units::Diopter(0.0025));
        const float near_blur = camera.defocus_blur_pixels();
        camera.set_focus(units::Diopter(-0.0025));
        REQUIRE(std::fabs(camera.defocus_blur_pixels() - near_blur) < 1e-6f);
        REQUIRE(camera.focus_distance().to_si_f() < 0.f);
    }

    SECTION("Aperture sampling follows the focus setting")
    {
        camera.set_focus(units::Diopter(0.0));
        REQUIRE_FALSE(camera.aperture_sampling_active());

        camera.set_focus(units::Diopter(0.05));
        REQUIRE(camera.defocus_blur_pixels() >= 0.5f);
        REQUIRE(camera.aperture_sampling_active());
    }
}

TEST_CASE("Wings-only kernel for unresolved sources", "[cameras][psf][scatter]")
{
    DiffractionCore core;
    core.radius = 4;
    core.banks = 2;

    CameraModel<RGB> camera;
    camera.set_focal_length(units::Millimeter(25.0));
    camera.configure_sensor_from_size(Resolution{256, 256}, units::Millimeter(6.0));
    camera.set_core(core);

    SECTION("Requires a scatter component")
    {
        REQUIRE_THROWS(camera.psf_wings_kernel());
    }

    SECTION("Wings kernel is the unit-energy scatter component")
    {
        const float b = 2.5f;
        const float r0 = 0.5f;

        HarveyShack scatter_params;
        scatter_params.fraction = 0.05f;
        scatter_params.exponent = b;
        scatter_params.r0 = r0;
        scatter_params.kernel_radius = 48;
        camera.set_scatter(scatter_params);

        const Image<RGB>& wings = camera.psf_wings_kernel();
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
        HarveyShack zero_scatter = scatter_params;
        zero_scatter.fraction = 0.f;
        camera.set_scatter(zero_scatter);
        Image<RGB> core_kernel = camera.psf_convolution_kernel();
        camera.set_scatter(scatter_params);
        const Image<RGB>& composite = camera.psf_convolution_kernel();
        const Image<RGB>& wings2 = camera.psf_wings_kernel();
        max_err = 0.f;
        for (int y = 0; y < composite.height(); ++y) {
            for (int x = 0; x < composite.width(); ++x) {
                const float e = core_kernel(x, y)[0] * 0.95f + wings2(x, y)[0] * 0.05f;
                max_err = std::max(max_err, std::fabs(composite(x, y)[0] - e));
            }
        }
        REQUIRE(max_err < 1e-6f);
    }
}
