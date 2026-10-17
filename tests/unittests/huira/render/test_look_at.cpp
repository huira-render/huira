#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// Where an image's light is centered, in pixels from its center, and how many columns hold more
/// than 5% of its brightest.
struct Spot {
    double dx;
    double dy;
    int lit_columns;
};

Spot measure(const Image<RGB>& image)
{
    double sum = 0.0;
    double sum_x = 0.0;
    double sum_y = 0.0;
    float brightest = 0.f;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const float power = image(x, y).total();
            sum += static_cast<double>(power);
            sum_x += static_cast<double>(power) * static_cast<double>(x);
            sum_y += static_cast<double>(power) * static_cast<double>(y);
            brightest = std::max(brightest, power);
        }
    }
    int lit_columns = 0;
    for (int x = 0; x < image.width(); ++x) {
        bool lit = false;
        for (int y = 0; y < image.height(); ++y) {
            lit = lit || image(x, y).total() > 0.05f * brightest;
        }
        lit_columns += lit ? 1 : 0;
    }
    const double center_x = 0.5 * static_cast<double>(image.width() - 1);
    const double center_y = 0.5 * static_cast<double>(image.height() - 1);
    return {sum_x / sum - center_x, sum_y / sum - center_y, lit_columns};
}

/// A camera 200 pixels square, about 40 degrees across.
CameraModelHandle<RGB> wide_camera(Scene<RGB>& scene)
{
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_size({200, 200}, 36_mm);
    camera_model.set_focal_length(50_mm);
    camera_model.enable_diffraction(false);
    camera_model.enable_depth_of_field(false);
    return camera_model;
}

/// A glowing ball, 1 m in radius by default (about 3 pixels in the wide camera at 100 m). It
/// glows evenly, so its image is a disk whatever way it faces and wherever it is lit from: where
/// its light is centered depends only on where it is.
InstanceHandle<RGB> add_ball(Scene<RGB>& scene, units::Meter radius = 1_m)
{
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    material.set_emissive_factor(RGB{1.f});
    return scene.root.new_instance(
        scene.add_primitive(scene.add_ellipsoid(radius, radius, radius), material));
}

Image<RGB> render(const Scene<RGB>& scene,
                  const CameraModelHandle<RGB>& camera_model,
                  const InstanceHandle<RGB>& camera,
                  const Interval& exposure,
                  ObservationMode mode,
                  std::size_t temporal_samples)
{
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(16);
    SceneView<RGB> scene_view(scene, exposure, camera, mode, temporal_samples);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

} // namespace

TEST_CASE("look_at follows a target moved by a velocity set by hand", "[scene][look_at]")
{
    // A ball 100 m in front of the camera, moving 20 m across the view (about 55 pixels) during
    // a 1 s exposure:
    const auto render_ball = [](bool track) {
        Scene<RGB> scene;
        auto ball = add_ball(scene);
        ball.set_position(0_m, 0_m, 100_m);
        ball.set_velocity(20_mps, 0_mps, 0_mps);
        auto camera_model = wide_camera(scene);
        auto camera = scene.root.new_instance(camera_model);
        if (track) {
            camera.look_at(ball, Vec3<double>{0.0, -1.0, 0.0});
        }
        return measure(render(scene,
                              camera_model,
                              camera,
                              Interval::from_centered(Time::from_et(0.0), 1_s),
                              ObservationMode::TRUE_STATE,
                              16));
    };

    // A camera that stares sees it streak...
    const Spot staring = render_ball(false);
    CHECK(staring.lit_columns > 40);

    // ...and one that tracks it keeps it at the center of the image, a disk a few pixels across.
    const Spot tracking = render_ball(true);
    CHECK(tracking.lit_columns < 10);
    CHECK(std::abs(tracking.dx) < 0.5);
    CHECK(std::abs(tracking.dy) < 0.5);
}

TEST_CASE("look_at aims from where a parent moved by hand puts the node", "[scene][look_at]")
{
    // The camera rides on a frame that moves 20 m sideways during the exposure, past a ball 100 m
    // away. Tracking it, the camera keeps the ball at the center of the image.
    Scene<RGB> scene;
    auto ball = add_ball(scene);
    ball.set_position(0_m, 0_m, 100_m);
    auto platform = scene.root.new_subframe();
    platform.set_velocity(20_mps, 0_mps, 0_mps);
    auto camera_model = wide_camera(scene);
    auto camera = platform.new_instance(camera_model);
    camera.look_at(ball, Vec3<double>{0.0, -1.0, 0.0});

    const Spot spot = measure(render(scene,
                                     camera_model,
                                     camera,
                                     Interval::from_centered(Time::from_et(0.0), 1_s),
                                     ObservationMode::TRUE_STATE,
                                     16));
    CHECK(spot.lit_columns < 10);
    CHECK(std::abs(spot.dx) < 0.5);
    CHECK(std::abs(spot.dy) < 0.5);
}

TEST_CASE("look_at aims where the scene view places the target, or in the mode it is given",
          "[scene][look_at]")
{
    // A ball 30 km in radius, a light second away (3e8 m), moving at 300 km/s across the view:
    // in the light second its light takes to arrive, it moves 1e-3 rad, which is 100 pixels in a
    // camera of 10 um pixels and a 1 m focal length. The camera itself moves at 30 km/s, which
    // aberrates what it sees by 1e-4 rad, 10 pixels.
    const auto render_ball = [](ObservationMode view_mode, std::optional<ObservationMode> aim) {
        Scene<RGB> scene;
        auto ball = add_ball(scene, 30_Km);
        ball.set_position(0_m, 0_m, units::Meter(3e8));
        ball.set_velocity(units::MetersPerSecond(3e5), 0_mps, 0_mps);

        auto camera_model = scene.new_camera_model();
        camera_model.configure_sensor_from_pitch({512, 512}, 10_um);
        camera_model.set_focal_length(1000_mm);
        camera_model.enable_diffraction(false);
        camera_model.enable_depth_of_field(false);
        auto camera = scene.root.new_instance(camera_model);
        camera.set_velocity(0_mps, units::MetersPerSecond(3e4), 0_mps);
        camera.look_at(ball, Vec3<double>{0.0, -1.0, 0.0}, aim);

        return measure(render(scene,
                              camera_model,
                              camera,
                              Interval::from_centered(Time::from_et(0.0), 1_ms),
                              view_mode,
                              1));
    };

    // By default, the target is aimed at where the scene view renders it, so it is centered in
    // every observation mode:
    for (ObservationMode mode : {ObservationMode::TRUE_STATE,
                                 ObservationMode::GEOMETRIC_STATE,
                                 ObservationMode::ABERRATED_STATE}) {
        INFO("observation mode " << static_cast<int>(mode));
        const Spot spot = render_ball(mode, std::nullopt);
        CHECK(std::abs(spot.dx) < 0.5);
        CHECK(std::abs(spot.dy) < 0.5);
    }

    // Aimed at where it truly is, it is seen 100 pixels behind, along its motion, where it was a
    // light second before:
    const Spot true_aim =
        render_ball(ObservationMode::GEOMETRIC_STATE, ObservationMode::TRUE_STATE);
    CHECK(std::abs(std::abs(true_aim.dx) - 100.0) < 1.0);
    CHECK(std::abs(true_aim.dy) < 0.5);

    // Aimed at its light-time position without the aberration, it is seen 10 pixels off, along
    // the camera's motion:
    const Spot geometric_aim =
        render_ball(ObservationMode::ABERRATED_STATE, ObservationMode::GEOMETRIC_STATE);
    CHECK(std::abs(geometric_aim.dx) < 0.5);
    CHECK(std::abs(std::abs(geometric_aim.dy) - 10.0) < 1.0);
}
