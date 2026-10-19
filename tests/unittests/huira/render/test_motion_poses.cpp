#include <cmath>
#include <cstddef>
#include <optional>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A camera 200 pixels square, about 40 degrees across, lit from behind by the Sun, and a way to
/// add balls 2 m across and render it over a 1 s exposure.
struct BallScene {
    Scene<RGB> scene;
    CameraModelHandle<RGB> camera_model;
    InstanceHandle<RGB> camera;
    PrimitiveHandle<RGB> ball;

    BallScene()
        : camera_model(scene.new_camera_model()), camera(scene.root.new_instance(camera_model)),
          ball(scene.add_primitive(scene.add_ellipsoid(2_m, 2_m, 2_m),
                                   scene.new_material(scene.new_bsdf_lambertian())))
    {
        camera_model.configure_sensor_from_size({200, 200}, 36_mm);
        camera_model.set_focal_length(50_mm);
        camera_model.enable_diffraction(false);
        camera_model.enable_depth_of_field(false);
        scene.root.new_instance(scene.new_sun_light()).set_position(0_m, 0_m, -1_au);
    }

    BallScene(const BallScene&) = delete;
    BallScene& operator=(const BallScene&) = delete;
    BallScene(BallScene&&) = delete;
    BallScene& operator=(BallScene&&) = delete;

    InstanceHandle<RGB> add_ball(units::Meter x, units::Meter y, units::Meter z)
    {
        auto instance = scene.root.new_instance(ball);
        instance.set_position(x, y, z);
        return instance;
    }

    Image<RGB> render(std::optional<std::size_t> samples = std::nullopt) const
    {
        SceneView<RGB> view(scene,
                            Interval::from_start(Time::from_et(0.0), 1_s),
                            camera,
                            ObservationMode::TRUE_STATE,
                            samples);
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        renderer.set_samples_per_pixel(64);
        renderer.render(view, frame_buffer);
        return frame_buffer.received_power();
    }
};

/// The light in the columns [first, last) of an image, and where it is centered.
struct RegionLight {
    double total = 0.0;
    double x = 0.0;
    double y = 0.0;
};

RegionLight light_in(const Image<RGB>& image, int first, int last)
{
    RegionLight light;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = first; x < last; ++x) {
            const double power = static_cast<double>(image(x, y).total());
            light.total += power;
            light.x += power * static_cast<double>(x);
            light.y += power * static_cast<double>(y);
        }
    }
    light.x /= light.total;
    light.y /= light.total;
    return light;
}

} // namespace

TEST_CASE("Something moving without turning is drawn as the same thing turning",
          "[render][motion_blur]")
{
    // A ball crossing 20 m (about 55 pixels) during the exposure. Turning as well, it is given to
    // Embree as poses that it turns between, and not turning, as matrices that it interpolates:
    // the two must draw the same streak, since a ball looks the same whichever way it faces.
    const auto streak = [](bool turning) {
        BallScene scene;
        auto ball = scene.add_ball(-10_m, 0_m, 100_m);
        ball.set_velocity(20_mps, 0_mps, 0_mps);
        if (turning) {
            ball.set_angular_velocity(units::RadiansPerSecond(1.0),
                                      units::RadiansPerSecond(2.0),
                                      units::RadiansPerSecond(0.5));
        }
        return light_in(scene.render(5), 0, 200);
    };
    const RegionLight still_facing = streak(false);
    const RegionLight turning = streak(true);
    INFO("total " << still_facing.total << " and " << turning.total << ", centered at "
                  << still_facing.x << ", " << still_facing.y << " and " << turning.x << ", "
                  << turning.y);
    CHECK(std::abs(turning.total / still_facing.total - 1.0) < 1e-4);
    CHECK(std::abs(turning.x - still_facing.x) < 1e-3);
    CHECK(std::abs(turning.y - still_facing.y) < 1e-3);
}

TEST_CASE("A still thing is drawn the same with few or many still things", "[render][motion_blur]")
{
    // A still ball on the left of the view and one moving on the right. With motion in the scene,
    // still things are given to Embree with moving ones, unless they far outnumber them (here,
    // with 70 more still balls behind the camera), when they are kept apart. Either way, the
    // still ball is drawn the same. The balls glow, so each pixel's light is the ball's coverage.
    const auto left_ball = [](int others) {
        Scene<RGB> scene;
        auto camera_model = scene.new_camera_model();
        camera_model.configure_sensor_from_size({200, 200}, 36_mm);
        camera_model.set_focal_length(50_mm);
        camera_model.enable_diffraction(false);
        camera_model.enable_depth_of_field(false);
        auto camera = scene.root.new_instance(camera_model);
        auto material = scene.new_material(scene.new_bsdf_lambertian());
        material.set_emissive(RGB{1.f});
        auto ball = scene.add_primitive(scene.add_ellipsoid(2_m, 2_m, 2_m), material);
        scene.root.new_instance(ball).set_position(-15_m, 0_m, 100_m);
        auto moving = scene.root.new_instance(ball);
        moving.set_position(15_m, -10_m, 100_m);
        moving.set_velocity(0_mps, 20_mps, 0_mps);
        for (int i = 0; i < others; ++i) {
            scene.root.new_instance(ball).set_position(units::Meter(3.0 * i), 0_m, -50_m);
        }
        SceneView<RGB> view(scene,
                            Interval::from_start(Time::from_et(0.0), 1_s),
                            camera,
                            ObservationMode::TRUE_STATE);
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        renderer.set_samples_per_pixel(16);
        renderer.set_max_bounces(1); // No light from the other balls, only the ball's own
        renderer.render(view, frame_buffer);
        return frame_buffer.received_power();
    };
    // The same up to rounding: given twice, the still ball's pose is interpolated, which can move
    // its edge by a rounding error.
    const RegionLight few = light_in(left_ball(0), 0, 100);
    const RegionLight many = light_in(left_ball(70), 0, 100);
    INFO("totals " << few.total << " and " << many.total << ", centered at " << few.x << ", "
                   << few.y << " and " << many.x << ", " << many.y);
    CHECK(few.total > 0.0);
    CHECK(std::abs(many.total / few.total - 1.0) < 1e-4);
    CHECK(std::abs(many.x - few.x) < 1e-3);
    CHECK(std::abs(many.y - few.y) < 1e-3);
}
