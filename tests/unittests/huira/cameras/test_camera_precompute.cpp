#include <atomic>
#include <cmath>
#include <cstddef>
#include <limits>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"
#include "tbb/parallel_for.h"

using namespace huira;

namespace {

constexpr double INF = std::numeric_limits<double>::infinity();

bool identical(const Image<RGB>& a, const Image<RGB>& b)
{
    if (a.width() != b.width() || a.height() != b.height()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t c = 0; c < RGB::size(); ++c) {
            if (a[i][c] != b[i][c]) {
                return false;
            }
        }
    }
    return true;
}

double total(const Image<RGB>& image)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        sum += static_cast<double>(image[i][0]);
    }
    return sum;
}

/// A 32x32 sensor of 10 um pixels behind a 50 mm f/8 lens.
void configure_sensor(CameraModel<RGB>& camera)
{
    camera.configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(10.0));
    camera.set_focal_length(units::Millimeter(50.0));
    camera.set_fstop(8.f);
}

/// Renders a single unresolved source, a little off-axis, with a camera configured by
/// `configure`, precomputing first if asked.
template <typename Configure>
Image<RGB> render_source(Configure configure, bool precompute)
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    configure(camera_model);
    auto camera = scene.root.new_instance(camera_model);

    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
    source.set_position(units::Meter(0.6), units::Meter(-1.3), units::Meter(1000.0));

    if (precompute) {
        camera_model.precompute();
        REQUIRE(camera_model.is_precomputed());
    }

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);

    CHECK(camera_model.is_precomputed());
    return frame_buffer.received_power();
}

} // namespace

TEST_CASE("PSF stamps are built on first use, not on construction", "[cameras][psf][precompute]")
{
    auto make_psf = [] {
        return AiryDisk<RGB>(units::Millimeter(50.0),
                             units::Micrometer(10.0),
                             units::Micrometer(10.0),
                             units::Millimeter(50.0 / 8.0),
                             6,
                             4);
    };

    AiryDisk<RGB> reference = make_psf();
    CHECK_FALSE(reference.has_polyphase_cache());
    const Image<RGB>& stamp = reference.get_kernel(0.5f, 0.5f);
    CHECK(reference.has_polyphase_cache());
    CHECK(stamp.width() == 13);

    // Many threads using a fresh PSF at once build it once, and all see the same stamps:
    AiryDisk<RGB> shared = make_psf();
    std::atomic<int> mismatches{0};
    tbb::parallel_for(0, 64, [&](int i) {
        const float u = static_cast<float>(i % 8) / 8.f;
        const float v = static_cast<float>(i / 8) / 8.f;
        if (!identical(shared.get_kernel(u, v), reference.get_kernel(u, v))) {
            ++mismatches;
        }
    });
    CHECK(mismatches.load() == 0);
}

TEST_CASE("Setters record settings, and precompute() builds what a render would use",
          "[cameras][psf][precompute]")
{
    CameraModel<RGB> camera;
    configure_sensor(camera);

    // No PSF and in focus: nothing to build, even though resolved bodies are convolved by
    // default (with nothing). Until that is turned back on below, only unresolved sources use
    // the PSF.
    CHECK(camera.is_precomputed());
    camera.enable_psf_convolution(false);

    camera.use_aperture_psf(6, 4);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // Settings no kernel in use depends on, and settings set to what they already are, change
    // nothing:
    camera.set_pixel_convention(PixelConvention::fits());
    camera.use_blender_convention(true);
    camera.use_blender_convention(false);
    camera.set_veiling_glare(0.1f);
    camera.set_psf_convolution_radius(12); // only used by convolution, which is off
    camera.set_fstop(8.f);
    camera.set_focal_length(units::Millimeter(50.0));
    camera.use_aperture_psf(6, 4);
    camera.configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(10.0));
    camera.set_focus_distance(units::Meter(1e6)); // under half a pixel of blur
    REQUIRE(camera.defocus_blur_radius() == 0.f);
    CHECK(camera.is_precomputed());

    // The PSF's stamps do not depend on the resolution...
    camera.configure_sensor_from_pitch(Resolution{40, 36}, units::Micrometer(10.0));
    CHECK(camera.is_precomputed());

    // ...but do on the aperture, focal length, pixel pitch and their own size:
    camera.set_fstop(5.6f);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    camera.set_focal_length(units::Millimeter(60.0));
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();

    camera.configure_sensor_from_pitch(Resolution{40, 36}, units::Micrometer(8.0));
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();

    camera.set_aperture<CircularAperture<RGB>>(units::Millimeter(8.0));
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();

    camera.use_aperture_psf(8, 4);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // Out of focus, unresolved sources get the defocus blur's stamps instead of the PSF's,
    // blurred by the PSF through the convolution kernel:
    camera.set_focus_sensor_offset(units::Micrometer(400.0));
    REQUIRE(camera.defocus_blur_radius() > 1.f);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    camera.set_focus_sensor_offset(units::Micrometer(500.0));
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // so a new PSF needs a new convolution kernel while it is (and not its stamps)...
    camera.use_aperture_psf(6, 4);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // ...and its stamps when it is back in focus:
    camera.set_focus_distance(units::Meter(INF));
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // Scattered light reaches unresolved sources whether or not bodies are convolved, through
    // the wings' kernel and its spectrum:
    camera.set_harvey_shack_scatter(0.05f, 2.5f);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // Convolving bodies needs the convolution kernel and its spectrum, which depend on the
    // scatter settings and convolution radius, and on the resolution too:
    camera.enable_psf_convolution();
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    camera.set_harvey_shack_scatter(0.02f, 2.5f);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();

    camera.set_psf_convolution_radius(10);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();

    camera.configure_sensor_from_pitch(Resolution{36, 36}, units::Micrometer(8.0));
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    camera.enable_psf_convolution(false);
    CHECK(camera.is_precomputed());

    // Removing the PSF resizes the wings, which take the convolution radius instead...
    camera.delete_psf();
    CHECK_FALSE(camera.has_psf());
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // ...and without scattered light there is nothing left to build:
    camera.disable_harvey_shack_scatter();
    CHECK(camera.is_precomputed());
}

TEST_CASE("Kernels are rebuilt only when a setting they depend on changes",
          "[cameras][psf][precompute]")
{
    CameraModel<RGB> camera;
    configure_sensor(camera);
    camera.use_aperture_psf(6, 4);
    camera.enable_psf_convolution();
    camera.set_psf_convolution_radius(12);
    camera.precompute();

    const Image<RGB>* stamp = &camera.get_psf_kernel(0.5f, 0.5f);
    const Image<RGB> stamp_at_f8 = *stamp;
    const Image<RGB> core_only = camera.get_psf_convolution_kernel();

    // Scattering changes the convolution kernel but not the PSF's stamps, which are left as
    // they were:
    camera.set_harvey_shack_scatter(0.05f, 2.5f);
    camera.precompute();
    CHECK(&camera.get_psf_kernel(0.5f, 0.5f) == stamp);
    CHECK_FALSE(identical(camera.get_psf_convolution_kernel(), core_only));

    // A narrower aperture rebuilds the stamps, with a wider Airy pattern that puts less of the
    // light in the center pixel:
    camera.set_fstop(16.f);
    camera.precompute();
    const Image<RGB>& stamp_at_f16 = camera.get_psf_kernel(0.5f, 0.5f);
    CHECK(stamp_at_f16(6, 6)[1] < stamp_at_f8(6, 6)[1]);

    // And back again gives exactly what it did before:
    camera.set_fstop(8.f);
    camera.precompute();
    CHECK(identical(camera.get_psf_kernel(0.5f, 0.5f), stamp_at_f8));
}

TEST_CASE("Invalid PSF settings are rejected when made, and inconsistent ones when built",
          "[cameras][psf][precompute]")
{
    CameraModel<RGB> camera;
    configure_sensor(camera);

    CHECK_THROWS(camera.use_aperture_psf(0, 16));
    CHECK_THROWS(camera.use_aperture_psf(8, 0));
    CHECK_THROWS(camera.use_aperture_psf(4096, 16)); // stamps beyond 4 GiB
    CHECK_FALSE(camera.has_psf());
    CHECK(camera.get_psf_radius() == 0);
    CHECK_THROWS(camera.get_psf_kernel(0.5f, 0.5f));

    // With neither a PSF nor scattering, convolution has nothing to do:
    camera.enable_psf_convolution();
    CHECK(camera.is_precomputed());
    CHECK_NOTHROW(camera.precompute());

    // Scattering without a PSF has no size until a convolution radius is set:
    camera.set_harvey_shack_scatter(0.05f, 2.5f);
    CHECK_FALSE(camera.is_precomputed());
    CHECK_THROWS(camera.precompute());
    camera.set_psf_convolution_radius(8);
    CHECK_NOTHROW(camera.precompute());
    CHECK(camera.is_precomputed());
}

TEST_CASE("A render precomputes a camera that is out of date, unless told not to",
          "[render][precompute]")
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(10.0));
    camera_model.set_focal_length(units::Millimeter(50.0));
    camera_model.set_fstop(8.f);
    camera_model.use_aperture_psf(6, 4);
    camera_model.enable_psf_convolution();
    camera_model.set_psf_convolution_radius(12);
    camera_model.set_harvey_shack_scatter(0.05f, 2.5f);
    auto camera = scene.root.new_instance(camera_model);

    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
    source.set_position(units::Meter(0.6), units::Meter(-1.3), units::Meter(1000.0));

    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    auto render = [&] {
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);
        return Image<RGB>(frame_buffer.received_power());
    };

    // By default the render builds what it needs:
    CHECK(camera_model.auto_precompute());
    CHECK_FALSE(camera_model.is_precomputed());
    const Image<RGB> automatic = render();
    CHECK(camera_model.is_precomputed());
    CHECK(total(automatic) > 0.0);

    // Without auto precompute, an up-to-date camera renders...
    camera_model.set_auto_precompute(false);
    CHECK_FALSE(camera_model.auto_precompute());
    CHECK(identical(render(), automatic));

    // ...and one that has changed since throws, until precomputed:
    camera_model.set_fstop(5.6f);
    CHECK_THROWS(render());
    camera_model.precompute();
    CHECK_NOTHROW(render());

    camera_model.set_fstop(8.f);
    camera_model.precompute();
    CHECK(identical(render(), automatic));
}

TEST_CASE("The image does not depend on the order of the settings or on how they are built",
          "[render][precompute]")
{
    for (bool defocus : {false, true}) {
        // Sensor and optics first, then the PSF settings:
        auto sensor_first = [defocus](CameraModelHandle<RGB>& camera) {
            camera.configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(10.0));
            camera.set_focal_length(units::Millimeter(50.0));
            camera.set_fstop(8.f);
            camera.use_aperture_psf(6, 4);
            camera.set_harvey_shack_scatter(0.05f, 2.5f);
            camera.set_psf_convolution_radius(12);
            camera.enable_psf_convolution();
            if (defocus) {
                camera.set_focus_sensor_offset(units::Micrometer(400.0));
            }
        };

        // The reverse:
        auto psf_first = [defocus](CameraModelHandle<RGB>& camera) {
            if (defocus) {
                camera.set_focus_sensor_offset(units::Micrometer(400.0));
            }
            camera.enable_psf_convolution();
            camera.set_psf_convolution_radius(12);
            camera.set_harvey_shack_scatter(0.05f, 2.5f);
            camera.use_aperture_psf(6, 4);
            camera.set_fstop(8.f);
            camera.set_focal_length(units::Millimeter(50.0));
            camera.configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(10.0));
        };

        INFO((defocus ? "defocused" : "in focus"));
        const Image<RGB> reference = render_source(sensor_first, true);
        CHECK(total(reference) > 0.0);
        CHECK(identical(render_source(sensor_first, false), reference));
        CHECK(identical(render_source(psf_first, true), reference));
        CHECK(identical(render_source(psf_first, false), reference));
    }
}
