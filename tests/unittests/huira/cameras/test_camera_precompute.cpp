#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

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

/// Scattered light with a shoulder of half a pixel for a 50 mm lens over 10 um pixels.
const units::Radian HALF_PIXEL(0.5 * 10e-6 / 0.05);

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

TEST_CASE("Setters record settings, and precompute() builds what a render would use",
          "[cameras][psf][precompute]")
{
    CameraModel<RGB> camera;
    configure_sensor(camera);

    // The aperture's diffraction pattern is on by default, and resolved bodies are convolved
    // with it by default:
    CHECK(camera.diffraction_enabled());
    CHECK_FALSE(camera.has_set_psf());
    CHECK_FALSE(camera.is_precomputed());

    // Without diffraction, and in focus, there are no optics kernels to build, only the geometry
    // tables. Until convolution is turned back on below, only unresolved sources use the PSF.
    camera.enable_diffraction(false);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());
    camera.enable_psf_convolution(false);

    camera.enable_diffraction();
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // Settings no kernel in use depends on, and settings set to what they already are, change
    // nothing:
    camera.set_pixel_convention(PixelConvention::fits());
    camera.use_blender_convention(true);
    camera.use_blender_convention(false);
    camera.set_veiling_glare(0.1f);
    camera.set_fstop(8.f);
    camera.set_focal_length(units::Millimeter(50.0));
    camera.enable_diffraction();
    camera.configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(10.0));
    camera.set_focus_distance(units::Meter(1e8)); // too little blur to change a pixel
    REQUIRE(camera.defocus_blur_radius() == 0.f);
    CHECK(camera.is_precomputed());

    // The PSF's tables do not depend on the resolution, though the geometry tables do...
    camera.configure_sensor_from_pitch(Resolution{40, 36}, units::Micrometer(10.0));
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // ...but do on the aperture, focal length and pixel pitch:
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

    // Out of focus, the PSF's tables are blurred:
    camera.set_focus_sensor_offset(units::Micrometer(400.0));
    REQUIRE(camera.defocus_blur_radius() > 1.f);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    camera.set_focus_sensor_offset(units::Micrometer(500.0));
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    camera.enable_diffraction();
    CHECK(camera.is_precomputed());

    // Coming back into focus rebuilds them:
    camera.set_focus_distance(units::Meter(INF));
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // Scattered light reaches unresolved sources whether or not bodies are convolved: the
    // PSF's tables hold it:
    camera.set_scatter(0.05f, 2.5f, HALF_PIXEL);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // Convolving bodies with the aperture's PSF needs it across the frame, from tables in focus
    // with the scattered light, which depend on the scatter settings, and on the resolution
    // too:
    camera.enable_psf_convolution();
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    camera.set_scatter(0.02f, 2.5f, HALF_PIXEL);
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    camera.configure_sensor_from_pitch(Resolution{36, 36}, units::Micrometer(8.0));
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    camera.enable_psf_convolution(false);
    CHECK(camera.is_precomputed());

    // Turning diffraction off leaves the scattered light, in tables of its own...
    camera.enable_diffraction(false);
    CHECK_FALSE(camera.diffraction_enabled());
    CHECK_FALSE(camera.is_precomputed());
    camera.precompute();
    CHECK(camera.is_precomputed());

    // ...and without scattered light there is nothing left to build:
    camera.disable_scatter();
    CHECK(camera.is_precomputed());
}

TEST_CASE("The PSF's images follow the settings they depend on", "[cameras][psf][precompute]")
{
    CameraModel<RGB> camera;
    configure_sensor(camera);
    camera.precompute();
    const Image<RGB> at_f8 = camera.psf_image(6, 0.5f, 0.5f);
    const Image<RGB> diffraction_at_f8 = camera.diffraction_image(6, 0.5f, 0.5f);
    CHECK(identical(at_f8, diffraction_at_f8));

    // Scattering changes the PSF but not the diffraction pattern:
    camera.set_scatter(0.05f, 2.5f, HALF_PIXEL);
    camera.precompute();
    CHECK_FALSE(identical(camera.psf_image(6, 0.5f, 0.5f), at_f8));
    CHECK(identical(camera.diffraction_image(6, 0.5f, 0.5f), diffraction_at_f8));

    // A narrower aperture widens the Airy pattern, which puts less of the light in the center
    // pixel:
    camera.set_fstop(16.f);
    camera.precompute();
    CHECK(camera.diffraction_image(6, 0.5f, 0.5f)(6, 6)[1] < diffraction_at_f8(6, 6)[1]);

    // And back again gives exactly what it did before:
    camera.set_fstop(8.f);
    camera.disable_scatter();
    camera.precompute();
    CHECK(identical(camera.psf_image(6, 0.5f, 0.5f), at_f8));
}

TEST_CASE("Invalid PSF settings are rejected when made, and inconsistent ones when built",
          "[cameras][psf][precompute]")
{
    CameraModel<RGB> camera;
    configure_sensor(camera);

    camera.enable_diffraction(false);
    CHECK_FALSE(camera.diffraction_enabled());
    CHECK_FALSE(camera.has_set_psf());
    CHECK_THROWS(camera.psf_image(-1));
    CHECK_THROWS(camera.diffraction_image(-1));
    CHECK_THROWS(camera.scatter_image(4)); // there is none

    // With neither a PSF nor scattering, convolution has nothing to do:
    camera.enable_psf_convolution();
    CHECK_NOTHROW(camera.precompute());
    CHECK(camera.is_precomputed());

    // Scattering without a PSF is drawn from its own tables:
    camera.set_scatter(0.05f, 2.5f, HALF_PIXEL);
    CHECK_FALSE(camera.is_precomputed());
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
    camera_model.enable_psf_convolution();
    camera_model.set_scatter(0.05f, 2.5f, HALF_PIXEL);
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
    CHECK(camera_model.auto_precompute_enabled());
    CHECK_FALSE(camera_model.is_precomputed());
    const Image<RGB> automatic = render();
    CHECK(camera_model.is_precomputed());
    CHECK(total(automatic) > 0.0);

    // Without auto precompute, an up-to-date camera renders...
    camera_model.enable_auto_precompute(false);
    CHECK_FALSE(camera_model.auto_precompute_enabled());
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
            camera.set_scatter(0.05f, 2.5f, HALF_PIXEL);
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
            camera.set_scatter(0.05f, 2.5f, HALF_PIXEL);
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

namespace {

/// A wide camera with strong barrel distortion, whose geometry tables take a while to build.
void distorted_camera(CameraModel<RGB>& camera, int resolution)
{
    camera.configure_sensor_from_pitch(Resolution{resolution, resolution},
                                       units::Micrometer(4096.0 / resolution));
    camera.set_focal_length(units::Millimeter(12.0));
    camera.set_brown_conrady_distortion(BrownCoefficients(-0.05, 0.01, 0.0, 0.0, 0.0));
    camera.enable_diffraction(false);
}

} // namespace

TEST_CASE("Geometry setters are cheap, and the tables are built once when needed",
          "[cameras][precompute][geometry]")
{
    // Every setter that changes the geometry rebuilt each pixel's ray direction and solid angle:
    // about 2 s per call for this camera on 2 cores.
    CameraModel<RGB> camera;
    distorted_camera(camera, 2048);

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i) {
        camera.set_focal_length(units::Millimeter(12.0 + 0.01 * i));
        camera.configure_sensor_from_pitch(Resolution{2048, 2048},
                                           units::Micrometer(2.0),
                                           std::nullopt,
                                           1024.f + static_cast<float>(i),
                                           1024.f);
    }
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    CHECK(elapsed.count() < 1.0);
    CHECK_FALSE(camera.is_precomputed());

    camera.precompute();
    CHECK(camera.is_precomputed());

    // Setting the geometry to what it is, or changing it and back, rebuilds nothing:
    camera.set_focal_length(units::Millimeter(12.19));
    camera.use_blender_convention(true);
    camera.use_blender_convention(false);
    CHECK(camera.is_precomputed());
}

TEST_CASE("Geometry getters give the same values whether the tables are built or not",
          "[cameras][precompute][geometry]")
{
    CameraModel<RGB> camera;
    distorted_camera(camera, 64);

    struct Values {
        std::vector<float> radiance_to_power;
        std::vector<Vec3<float>> rays;
        std::vector<Pixel> projections;
        std::vector<int> in_view;
    };
    auto read = [&] {
        Values v;
        for (int y = 0; y < 64; y += 7) {
            for (int x = 0; x < 64; x += 5) {
                v.radiance_to_power.push_back(camera.pixel_radiance_to_power(x, y));
                const Ray<RGB> ray = camera.cast_ray(
                    Pixel{static_cast<float>(x) + 0.3f, static_cast<float>(y) - 0.2f});
                v.rays.push_back(ray.direction());
                const Vec3<float> point = ray.direction() * 7.f;
                v.projections.push_back(camera.try_project_point(point));
                v.in_view.push_back(
                    camera.in_fov(Vec3<float>{0.1f * static_cast<float>(x - 32), 0.f, 1.f}) ? 1
                                                                                            : 0);
            }
        }
        return v;
    };

    const Values lazy = read();
    CHECK_FALSE(camera.is_precomputed()); // the getters built no per-pixel table
    camera.precompute();
    const Values built = read();

    CHECK(lazy.radiance_to_power == built.radiance_to_power);
    CHECK(lazy.rays == built.rays);
    for (std::size_t i = 0; i < lazy.projections.size(); ++i) {
        CHECK(lazy.projections[i].x == built.projections[i].x);
        CHECK(lazy.projections[i].y == built.projections[i].y);
    }
    CHECK(lazy.in_view == built.in_view);
}

TEST_CASE("Geometry getters can be called from several threads at once",
          "[cameras][precompute][geometry]")
{
    CameraModel<RGB> camera;
    distorted_camera(camera, 256);
    camera.precompute();
    const float expected = camera.pixel_radiance_to_power(10, 20);
    camera.set_focal_length(units::Millimeter(13.0));
    camera.set_focal_length(units::Millimeter(12.0)); // back: the tables are current again
    REQUIRE(camera.is_precomputed());
    camera.set_focal_length(units::Millimeter(12.5));

    std::atomic<int> mismatches{0};
    const float changed = [&] {
        CameraModel<RGB> reference;
        distorted_camera(reference, 256);
        reference.set_focal_length(units::Millimeter(12.5));
        return reference.pixel_radiance_to_power(10, 20);
    }();
    CHECK(changed != expected);
    tbb::parallel_for(0, 64, [&](int i) {
        if (i % 8 == 0) {
            camera.precompute(); // builds the tables while others read
        }
        if (camera.pixel_radiance_to_power(10, 20) != changed || !camera.in_fov({0.f, 0.f, 1.f})) {
            mismatches.fetch_add(1);
        }
    });
    CHECK(mismatches.load() == 0);
}
