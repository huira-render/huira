#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

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

TEST_CASE("The aperture's diffraction pattern is the PSF by default", "[cameras][psf][defaults]")
{
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch({64, 64}, 10_um);
    camera.set_focal_length(50_mm);
    CHECK(camera.diffraction_enabled());
    CHECK_FALSE(camera.has_set_psf());
    CHECK_FALSE(camera.scatter_enabled());

    // Unresolved sources are drawn with it, as it is on its own:
    const Image<RGB> psf = camera.psf_image(4, 0.2f, -0.3f);
    const Image<RGB> pattern = camera.diffraction_image(4, 0.2f, -0.3f);
    double worst = 0.0;
    for (std::size_t i = 0; i < psf.size(); ++i) {
        worst = std::max(worst, std::abs(static_cast<double>(psf[i][1] - pattern[i][1])));
    }
    CHECK(worst < 1e-6);

    // Without it, unresolved sources in focus put all their light in one pixel:
    camera.enable_diffraction(false);
    CHECK_FALSE(camera.diffraction_enabled());
    const Image<RGB> point = camera.psf_image(4, 0.2f, -0.3f);
    CHECK(point(4, 4)[1] == 1.f);
    camera.enable_diffraction();
    CHECK(camera.diffraction_enabled());
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
            // In focus: out of focus, even by a twentieth of a pixel, the star is a small disc.
            camera_model.enable_diffraction(false);
            camera_model.enable_depth_of_field(false);
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
    camera.enable_diffraction(false); // only the defocus blur is built here
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
    CHECK_FALSE(camera.is_precomputed()); // the tables are for the blur
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
    CHECK_FALSE(camera.uses_blender_convention());
    camera.use_blender_convention();
    CHECK(camera.uses_blender_convention());
    CHECK(camera.depth_of_field_enabled());
    camera.enable_depth_of_field(false);
    CHECK_FALSE(camera.depth_of_field_enabled());
    CHECK(camera.psf_convolution_enabled());
    camera.enable_psf_convolution(false);
    CHECK_FALSE(camera.psf_convolution_enabled());
    CHECK(camera.motion_blur_enabled());
    camera.enable_motion_blur(false);
    CHECK_FALSE(camera.motion_blur_enabled());

    // Sensor:
    camera.set_sensor_quantum_efficiency(0.7);
    camera.set_sensor_full_well_capacity(30000.f);
    camera.enable_sensor_noise(false);
    camera.set_sensor_read_noise(3.5f);
    camera.set_sensor_dark_current(0.25f);
    camera.set_sensor_bias_level(100.f);
    camera.set_sensor_bit_depth(14);
    camera.set_sensor_conversion_gain(2.5f);
    camera.set_sensor_unity_db(5.f);
    camera.set_sensor_roll(units::Degree(90.0));
    CHECK(close(camera.sensor_quantum_efficiency()[1], 0.7));
    CHECK(camera.sensor_full_well_capacity() == 30000.f);
    CHECK_FALSE(camera.sensor_noise_enabled());
    CHECK(camera.sensor_read_noise() == 3.5f);
    CHECK(camera.sensor_dark_current() == 0.25f);
    CHECK(camera.sensor_bias_level() == 100.f);
    CHECK(camera.sensor_bit_depth() == 14);
    CHECK(camera.sensor_conversion_gain() == 2.5f);
    CHECK(camera.sensor_unity_db() == 5.f);
    CHECK(close(camera.sensor_gain_db(), 5.0 - 20.0 * std::log10(2.5)));
    CHECK(close(camera.sensor_roll().to_si(), PI<double>() / 2.0));

    // PSF:
    CHECK(camera.diffraction_enabled());
    camera.set_scatter(0.05f, 2.5f, units::Arcsecond(20.0));
    CHECK(camera.scatter_enabled());
    CHECK(camera.psf_image(4).width() == 9);
    CHECK(camera.diffraction_image(4).width() == 9);
    CHECK(camera.scatter_image(6).width() == 13);
    CHECK(camera.defocus_image(3, 0.f, 0.f, units::Meter(10.0)).width() == 7);
    CHECK(camera.defocus_blur_radius(units::Meter(10.0)) == 0.f); // depth of field is off
    camera.disable_scatter();
    CHECK_FALSE(camera.scatter_enabled());

    Image<RGB> data(9, 9, RGB{0.f});
    data(4, 4) = RGB{1.f};
    camera.set_measured_psf(data, 1.f, PSFSampling::PixelIntegrated);
    CHECK(camera.has_set_psf());
    CHECK(camera.diffraction_enabled());
    camera.clear_psf();
    CHECK_FALSE(camera.has_set_psf());
    camera.enable_diffraction(false);
    CHECK_FALSE(camera.diffraction_enabled());
}

TEST_CASE("A sensor's roll turns the scene the other way in the image", "[cameras][render][roll]")
{
    // A 32x32 sensor of 10 um pixels behind a 50 mm lens: one pixel is 1/5000 rad, and the
    // optical axis meets the sensor at its center, (16, 16) in sensor coordinates.
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({32, 32}, 10_um);
    camera_model.set_focal_length(50_mm);
    camera_model.enable_diffraction(false); // all of a source's light in one pixel
    camera_model.set_sensor_roll(units::Degree(30.0));
    CHECK(close(camera_model.sensor_roll().to_si(), PI<double>() / 6.0));

    // The sensor's x axis is turned toward its y axis:
    const Vec3<double> x_axis = camera_model.sensor_orientation().x_axis();
    CHECK(close(x_axis.x, std::cos(PI<double>() / 6.0), 1e-9));
    CHECK(close(x_axis.y, std::sin(PI<double>() / 6.0), 1e-9));
    CHECK(std::abs(x_axis.z) < 1e-12);

    // A source 5 px to the right of the axis, in the camera's own axes. Without the roll it would
    // land at (21, 16); with it, the scene turns counterclockwise in the image (y down), to
    // (16 + 5 cos 30, 16 - 5 sin 30) = (20.33, 13.5): pixel (20, 13).
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
    source.set_position(units::Meter(1.0), units::Meter(0.0), units::Meter(1000.0));

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    SceneView<RGB> scene_view(scene,
                              Interval{Time::from_et(0.0), Time::from_et(0.001)},
                              camera,
                              ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);

    const Image<RGB>& power = frame_buffer.received_power();
    int brightest_x = -1;
    int brightest_y = -1;
    float brightest = 0.f;
    for (int y = 0; y < power.height(); ++y) {
        for (int x = 0; x < power.width(); ++x) {
            if (power(x, y)[0] > brightest) {
                brightest = power(x, y)[0];
                brightest_x = x;
                brightest_y = y;
            }
        }
    }
    CHECK(brightest_x == 20);
    CHECK(brightest_y == 13);
}

TEST_CASE("A camera describes its settings", "[cameras][describe]")
{
    Scene<RGB> scene;
    auto camera = scene.new_camera_model();
    camera.configure_sensor_from_pitch({32, 32}, 10_um);
    camera.set_focal_length(50_mm);
    camera.set_fstop(4.f);
    camera.set_sensor_roll(units::Degree(90.0));
    camera.set_brown_conrady_distortion(BrownCoefficients(-0.05, 0.01, 0.0, 0.0, 0.0));

    const std::string text = camera.describe();
    INFO(text);
    CHECK(text.find("focal length 50 mm, f/4 (aperture 12.5 mm)") != std::string::npos);
    CHECK(text.find("32 x 32 px of 10 um (0.32 x 0.32 mm), roll 90 deg") != std::string::npos);
    CHECK(text.find("fx 5000, fy 5000, cx 15.5, cy 15.5 px") != std::string::npos);
    CHECK(text.find("0.3667 x 0.3667 deg") != std::string::npos); // 2 atan(16 / 5000)
    CHECK(text.find("Brown-Conrady, k1 -0.05, k2 0.01") != std::string::npos);
    CHECK(text.find("Focus:       infinity") != std::string::npos);
    CHECK(text.find("Noise:       on") != std::string::npos);
    CHECK(text.find("Motion blur: on") != std::string::npos);

    // It builds nothing:
    CHECK(text.find("Precomputed: no") != std::string::npos);
    CHECK_FALSE(camera.is_precomputed());
}
