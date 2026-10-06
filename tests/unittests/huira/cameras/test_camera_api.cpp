#include <algorithm>
#include <cmath>
#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

bool close(double a, double b, double tolerance = 1e-5)
{
    return std::abs(a - b) <= tolerance * std::max(std::abs(b), 1.0);
}

} // namespace

TEST_CASE("The aperture's diffraction pattern is the PSF by default, with an automatic stamp size",
          "[cameras][psf][defaults]")
{
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch({64, 64}, 10_um);
    camera.set_focal_length(50_mm);
    CHECK(camera.has_psf());
    CHECK(camera.uses_aperture_psf());

    // The radius holding 99% of an Airy pattern's light, 20.4 lambda N at RGB's longest
    // wavelength (675 nm), is 3.9 px at f/2.8 here, so the stamps take the smallest automatic
    // radius:
    CHECK(camera.get_psf_radius() == CameraModel<RGB>::MIN_AUTO_PSF_RADIUS);

    // At f/16 with 5 um pixels it is 44.1 px, and one more covers the subpixel offset:
    camera.configure_sensor_from_pitch({64, 64}, 5_um);
    camera.set_fstop(16.f);
    CHECK(camera.get_psf_radius() == 46);

    // At f/64 it would be 177 px, beyond the largest automatic radius:
    camera.set_fstop(64.f);
    CHECK(camera.get_psf_radius() == CameraModel<RGB>::DEFAULT_PSF_RADIUS);

    // A size can still be given, and 0 goes back to the automatic one:
    camera.use_aperture_psf(8, 4);
    CHECK(camera.get_psf_radius() == 8);
    camera.use_aperture_psf(0);
    CHECK(camera.get_psf_radius() == CameraModel<RGB>::DEFAULT_PSF_RADIUS);

    // Without a PSF, unresolved sources put all their light in one pixel:
    camera.delete_psf();
    CHECK_FALSE(camera.has_psf());
    CHECK_FALSE(camera.uses_aperture_psf());
    CHECK(camera.get_psf_radius() == 0);
}

TEST_CASE("A star spreads over the pixels around it by default", "[render][psf][defaults]")
{
    // A star on the corner of four pixels. Without a PSF, which used to be the default, all of
    // its light went into one of them: stars looked like pin-pricks.
    auto brightest_share = [](bool psf) {
        Scene<RGB> scene;
        auto camera_model = scene.new_camera_model();
        camera_model.configure_sensor_from_pitch({32, 32}, 10_um);
        camera_model.set_focal_length(50_mm);
        if (!psf) {
            camera_model.delete_psf();
        }
        auto camera = scene.root.new_instance(camera_model);

        // On the corner of the four pixels around the center, 5000 px from the axis per radian:
        auto star = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
        star.set_position(units::Meter(0.0), units::Meter(0.0), units::Meter(1000.0));

        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);

        const Image<RGB>& image = frame_buffer.received_power();
        double total = 0.0;
        double brightest = 0.0;
        for (std::size_t i = 0; i < image.size(); ++i) {
            total += static_cast<double>(image[i][1]);
            brightest = std::max(brightest, static_cast<double>(image[i][1]));
        }
        REQUIRE(total > 0.0);
        return brightest / total;
    };

    CHECK(brightest_share(true) < 0.5);
    CHECK(brightest_share(false) == 1.0);
}

TEST_CASE("Depth of field is on by default, and unresolved sources' defocus follows it",
          "[cameras][focus][defaults]")
{
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch({32, 32}, 10_um);
    camera.set_focal_length(50_mm);
    camera.set_fstop(8.f);
    camera.delete_psf(); // only the defocus stamps are built here
    CHECK(camera.depth_of_field_enabled());

    // About a 3 px blur:
    camera.set_focus_sensor_offset(units::Micrometer(480.0));
    const float blur = camera.defocus_blur_radius();
    CHECK(blur > 2.5f);
    camera.precompute();
    CHECK(camera.is_precomputed());

    // Off, everything is in focus, stars included. Unresolved sources used to stay defocused,
    // whatever the switch:
    camera.enable_depth_of_field(false);
    CHECK_FALSE(camera.depth_of_field_enabled());
    CHECK(camera.defocus_blur_radius() == 0.f);
    CHECK_FALSE(camera.is_precomputed()); // the defocus stamps are left over
    camera.precompute();
    CHECK(camera.is_precomputed());

    camera.enable_depth_of_field(true);
    CHECK(camera.defocus_blur_radius() == blur);
    CHECK_FALSE(camera.is_precomputed());
}

TEST_CASE("The camera handle reads back what was set", "[cameras][api]")
{
    Scene<RGB> scene;
    auto camera = scene.new_camera_model();

    // Geometry:
    camera.configure_sensor_from_pitch({64, 48}, 5_um, 6_um);
    CHECK(camera.resolution() == Resolution{64, 48});
    const auto [pitch_x, pitch_y] = camera.pixel_pitch();
    CHECK(close(pitch_x.raw_value(), 5.0));
    CHECK(close(pitch_y.raw_value(), 6.0));

    // The camera matrix, in the camera's pixel convention, in either:
    for (const PixelConvention& convention : {PixelConvention::opencv(), PixelConvention::fits()}) {
        camera.set_pixel_convention(convention);
        camera.set_intrinsics(1000.f, 1100.f, 30.5f, 20.25f, {64, 48}, 50_mm, 2.f);
        const Mat3<float> k = camera.intrinsic_matrix(); // K[column][row]
        CHECK(close(k[0][0], 1000.0));
        CHECK(close(k[1][1], 1100.0));
        CHECK(close(k[1][0], 2.0));
        CHECK(close(k[2][0], 30.5));
        CHECK(close(k[2][1], 20.25));
        CHECK(k[2][2] == 1.f);
        CHECK((k[0][1] == 0.f && k[0][2] == 0.f && k[1][2] == 0.f));
    }
    camera.set_pixel_convention(PixelConvention::opencv());

    // Projection and rays:
    const Ray<RGB> ray = camera.cast_ray(Pixel{10.25f, 30.75f});
    const Vec3<float> point = ray.direction() * 10.f;
    const Pixel projected = camera.project_point(point);
    CHECK(close(projected.x, 10.25, 1e-4));
    CHECK(close(projected.y, 30.75, 1e-4));
    CHECK(camera.in_fov(point));
    CHECK(camera.try_project_point(point).x == projected.x);
    const Vec3<float> behind{0.f, 0.f, -1.f};
    CHECK_FALSE(camera.in_fov(behind));
    CHECK(std::isnan(camera.try_project_point(behind).x));
    const Ray<RGB> centered = camera.cast_ray(10, 30);
    CHECK(glm::length(centered.direction() - camera.cast_ray(Pixel{10.f, 30.f}).direction()) ==
          0.f);

    // Switches:
    CHECK_FALSE(camera.is_blender_convention());
    camera.use_blender_convention();
    CHECK(camera.is_blender_convention());
    CHECK(camera.depth_of_field_enabled());
    camera.enable_depth_of_field(false);
    CHECK_FALSE(camera.depth_of_field_enabled());
    CHECK(camera.psf_convolution_enabled());
    camera.enable_psf_convolution(false);
    CHECK_FALSE(camera.psf_convolution_enabled());

    // Sensor:
    camera.set_sensor_quantum_efficiency(0.7);
    camera.set_sensor_full_well_capacity(30000.f);
    camera.set_sensor_simulate_noise(false);
    camera.set_sensor_read_noise(3.5f);
    camera.set_sensor_dark_current(0.25f);
    camera.set_sensor_bias_level(100.f);
    camera.set_sensor_bit_depth(14);
    camera.set_sensor_conversion_gain(2.5f);
    camera.set_sensor_unity_db(5.f);
    camera.set_sensor_rotation(units::Degree(90.0));
    CHECK(close(camera.sensor_quantum_efficiency()[1], 0.7));
    CHECK(camera.sensor_full_well_capacity() == 30000.f);
    CHECK_FALSE(camera.sensor_simulate_noise());
    CHECK(camera.sensor_read_noise() == 3.5f);
    CHECK(camera.sensor_dark_current() == 0.25f);
    CHECK(camera.sensor_bias_level() == 100.f);
    CHECK(camera.sensor_bit_depth() == 14);
    CHECK(camera.sensor_conversion_gain() == 2.5f);
    CHECK(camera.sensor_unity_db() == 5.f);
    CHECK(close(camera.sensor_gain_db(), 5.0 - 20.0 * std::log10(2.5)));
    CHECK(close(camera.sensor_rotation().to_si(), PI<double>() / 2.0));

    // PSF:
    CHECK(camera.uses_aperture_psf());
    camera.use_aperture_psf(4, 2); // small, so quick to build
    camera.set_harvey_shack_scatter(0.05f, 2.5f);
    camera.set_psf_convolution_radius(6);
    CHECK(camera.get_psf_kernel(0.f, 0.f).width() == 9);
    CHECK(camera.get_psf_convolution_kernel().width() == 13);
    CHECK(camera.get_psf_wings_kernel().width() == 13);

    Image<RGB> data(9, 9, RGB{0.f});
    data(4, 4) = RGB{1.f};
    camera.set_measured_psf(data, 1.f, PSFSampling::PixelIntegrated, 4);
    CHECK(camera.has_psf());
    CHECK_FALSE(camera.uses_aperture_psf());
}
