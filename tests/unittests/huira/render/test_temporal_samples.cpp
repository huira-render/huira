#include <cmath>
#include <cstddef>
#include <optional>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A ball 100 m in front of a camera about 40 degrees across, 200 pixels square, lit from behind
/// the camera, over a 1 s exposure: the camera, the ball, and how to view them.
struct BallScene {
    Scene<RGB> scene;
    CameraModelHandle<RGB> camera_model;
    InstanceHandle<RGB> camera;
    InstanceHandle<RGB> ball;

    BallScene()
        : camera_model(scene.new_camera_model()), camera(scene.root.new_instance(camera_model)),
          ball(scene.root.new_instance(scene.add_primitive(
              scene.add_ellipsoid(2_m, 2_m, 2_m), scene.new_material(scene.new_bsdf_lambertian()))))
    {
        camera_model.configure_sensor_from_size({200, 200}, 36_mm);
        camera_model.set_focal_length(50_mm);
        camera_model.enable_diffraction(false);
        camera_model.enable_depth_of_field(false);
        ball.set_position(0_m, 0_m, 100_m);
        scene.root.new_instance(scene.new_sun_light()).set_position(0_m, 0_m, -1_au);
    }

    SceneView<RGB> view(std::optional<std::size_t> samples = std::nullopt,
                        double tolerance = SceneView<RGB>::DEFAULT_MOTION_TOLERANCE) const
    {
        return SceneView<RGB>(scene,
                              Interval::from_centered(Time::from_et(0.0), 1_s),
                              camera,
                              ObservationMode::TRUE_STATE,
                              samples,
                              tolerance);
    }

    Image<RGB> render(std::optional<std::size_t> samples = std::nullopt,
                      int samples_per_pixel = 64) const
    {
        SceneView<RGB> scene_view = view(samples);
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        renderer.set_samples_per_pixel(samples_per_pixel);
        renderer.render(scene_view, frame_buffer);
        return frame_buffer.received_power();
    }
};

/// Where an image's light is centered, in pixels.
Vec2<double> centroid(const Image<RGB>& image)
{
    double sum = 0.0;
    Vec2<double> moment{0.0};
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double power = image(x, y).total();
            sum += power;
            moment += power * Vec2<double>{x, y};
        }
    }
    return moment / sum;
}

std::size_t lit_pixels(const Image<RGB>& image)
{
    std::size_t lit = 0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        lit += image[i].total() > 0.f ? std::size_t{1} : std::size_t{0};
    }
    return lit;
}

} // namespace

TEST_CASE("What does not move in view is rendered as a still frame", "[render][motion_blur]")
{
    BallScene still;
    const SceneView<RGB> still_view = still.view();
    CHECK_FALSE(still_view.has_motion_blur());
    CHECK(still_view.max_temporal_samples() == 1);

    // Nor does motion too small to see: the ball moves a hundredth of a pixel (pixels span 36 cm
    // at 100 m)...
    BallScene slow;
    slow.ball.set_velocity(units::MetersPerSecond(0.0036), 0_mps, 0_mps);
    CHECK_FALSE(slow.view().has_motion_blur());

    // ...nor motion shared with the camera: the ball and camera both move 8 km/s, and the Sun,
    // 1 au away, shifts by far less than a pixel.
    BallScene together;
    together.ball.set_velocity(0_mps, 8_Kmps, 0_mps);
    together.camera.set_velocity(0_mps, 8_Kmps, 0_mps);
    CHECK_FALSE(together.view().has_motion_blur());
}

TEST_CASE("A body moving in a straight line takes two temporal samples", "[render][motion_blur]")
{
    // The ball crosses 20 m, about 55 pixels: a straight line is drawn exactly from its ends.
    BallScene scene;
    scene.ball.set_velocity(20_mps, 0_mps, 0_mps);
    const SceneView<RGB> view = scene.view();
    CHECK(view.has_motion_blur());
    CHECK(view.max_temporal_samples() == 2);
}

TEST_CASE("A number of temporal samples, if given, is used for everything", "[render][motion_blur]")
{
    BallScene scene;
    scene.ball.set_velocity(20_mps, 0_mps, 0_mps);

    // One renders a still frame, at the middle of the exposure:
    const SceneView<RGB> still = scene.view(1);
    CHECK_FALSE(still.has_motion_blur());
    CHECK(still.max_temporal_samples() == 1);

    const SceneView<RGB> five = scene.view(5);
    CHECK(five.has_motion_blur());
    CHECK(five.max_temporal_samples() == 5);

    CHECK_THROWS(scene.view(0));
    CHECK_THROWS(scene.view(130)); // Embree's most
    CHECK_THROWS(scene.view(std::nullopt, 0.0));
    CHECK_THROWS(scene.view(std::nullopt, -1.0));
}

TEST_CASE("A turning camera's view is followed within the tolerance", "[render][motion_blur]")
{
    // The camera turns 0.4 rad (23 degrees) during the exposure, so the ball, which holds still,
    // sweeps about 112 pixels along an arc. Drawn with the samples chosen, its streak lies where
    // it does drawn with many more.
    BallScene scene;
    scene.ball.set_position(20_m, 15_m, 100_m);
    scene.camera.set_angular_velocity(
        units::RadiansPerSecond(0.0), units::RadiansPerSecond(0.4), units::RadiansPerSecond(0.0));
    const std::size_t chosen = scene.view().max_temporal_samples();
    CHECK(chosen > 2);
    CHECK(chosen < 129);

    // A tighter tolerance takes more:
    CHECK(scene.view(std::nullopt, 0.01).max_temporal_samples() > chosen);

    // (At 256 samples per pixel, where along the streak each pixel's samples catch the ball
    // varies by a few hundredths of a pixel with the path's smallest changes.)
    const Vec2<double> automatic = centroid(scene.render(std::nullopt, 256));
    const Vec2<double> many = centroid(scene.render(129, 256));
    CHECK(glm::length(automatic - many) < 0.1);
}

TEST_CASE("A spinning body is not taken for a still one", "[render][motion_blur]")
{
    // A square 10 m across, turning a full turn about the line of sight during the exposure:
    // it ends as it starts, so its poses at the start and end alone say it never moved.
    BallScene scene;
    const Vec3<float> normal{0.f, 0.f, -1.f};
    auto vertex = [&](float x, float y) {
        Vertex<RGB> v;
        v.position = Vec3<float>{x, y, 0.f};
        v.normal = normal;
        return v;
    };
    const VertexBuffer<RGB> vertices{
        vertex(-5.f, -5.f), vertex(5.f, -5.f), vertex(5.f, 5.f), vertex(-5.f, 5.f)};
    auto square = scene.scene.root.new_instance(
        scene.scene.add_primitive(scene.scene.add_mesh(IndexBuffer{0, 2, 1, 0, 3, 2}, vertices),
                                  scene.scene.new_material(scene.scene.new_bsdf_lambertian())));
    square.set_position(0_m, 0_m, 100_m);
    square.set_angular_velocity(units::RadiansPerSecond(0.0),
                                units::RadiansPerSecond(0.0),
                                units::RadiansPerSecond(2.0 * PI<double>()));
    scene.ball.set_visible(false);

    const SceneView<RGB> view = scene.view();
    CHECK(view.has_motion_blur());
    CHECK(view.max_temporal_samples() >= 9); // At most 45 degrees between two

    // Swept round, it covers a disk:
    const std::size_t automatic = lit_pixels(scene.render());
    const std::size_t many = lit_pixels(scene.render(129));
    CHECK(std::abs(static_cast<double>(automatic) / static_cast<double>(many) - 1.0) < 0.02);
}

TEST_CASE("Stars are followed along their arcs as the camera rolls", "[render][motion_blur]")
{
    // The camera rolls 0.5 rad about its line of sight, and stars near the edges of the frame
    // sweep arcs about 40 pixels long.
    BallScene scene;
    scene.ball.set_visible(false);
    std::vector<Star<RGB>> stars;
    for (const Vec3<double>& direction : {Vec3<double>{0.25, 0.0, 1.0},
                                          Vec3<double>{-0.2, 0.2, 1.0},
                                          Vec3<double>{0.1, -0.3, 1.0}}) {
        stars.emplace_back(glm::normalize(direction), RGB{1e-10f});
    }
    scene.scene.set_stars(stars);
    scene.camera.set_angular_velocity(
        units::RadiansPerSecond(0.0), units::RadiansPerSecond(0.0), units::RadiansPerSecond(0.5));

    const std::size_t chosen = scene.view().max_temporal_samples();
    CHECK(chosen > 1);
    CHECK(chosen <= 17);

    const Vec2<double> automatic = centroid(scene.render());
    const Vec2<double> many = centroid(scene.render(65));
    CHECK(glm::length(automatic - many) < 0.1);
}
