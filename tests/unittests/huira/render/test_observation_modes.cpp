#include <cmath>
#include <optional>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// What is in view: a star, or a still body far away in the same direction.
enum class Source { Star, Body };

/// Where a source straight ahead of the camera is drawn, in pixels (its light's centroid), as
/// seen by a camera moving across the line of sight at 300 km/s (1e-3 of the speed of light),
/// which aberrates it by 1e-3 rad: 100 pixels of 10 um, behind a 1 m focal length. With no
/// observation mode, the scene view is given none.
Vec2<double> drawn_at(Source source, std::optional<ObservationMode> mode)
{
    Scene<RGB> scene;
    if (source == Source::Star) {
        scene.set_stars({Star<RGB>(Vec3<double>{0.0, 0.0, 1.0}, RGB{1e-10f})});
    } else {
        // A light hour away, still: light time does not move it.
        auto body = scene.root.new_instance(
            scene.new_unresolved_object(units::WattsPerMeterSquared(1e-10)));
        body.set_position(0_m, 0_m, units::Meter(1.08e12));
    }

    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({512, 512}, 10_um);
    camera_model.set_focal_length(1000_mm);
    camera_model.enable_diffraction(false);
    camera_model.enable_depth_of_field(false);
    auto camera = scene.root.new_instance(camera_model);
    camera.set_velocity(0_mps, units::MetersPerSecond(3e5), 0_mps);

    const Interval exposure = Interval::from_centered(Time::from_et(0.0), 1_ms);
    SceneView<RGB> scene_view = mode ? SceneView<RGB>(scene, exposure, camera, *mode)
                                     : SceneView<RGB>(scene, exposure, camera);
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.render(scene_view, frame_buffer);
    const Image<RGB>& image = frame_buffer.received_power();

    double sum = 0.0;
    Vec2<double> moment{0.0};
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double power = image(x, y).total();
            sum += power;
            moment += power * Vec2<double>{x, y};
        }
    }
    REQUIRE(sum > 0.0);
    return moment / sum;
}

} // namespace

TEST_CASE("Aberration is relativistic, and keeps a direction a unit vector", "[core][aberration]")
{
    const double c = SPEED_OF_LIGHT<double>();

    // Seen across the observer's motion, a source is shifted toward it by the angle whose sine
    // is the speed over that of light: at 0.6 c, from (1, 0, 0) to (0.8, 0.6, 0).
    const Vec3<double> across =
        compute_aberrated_direction(Vec3<double>{1.0, 0.0, 0.0}, Vec3<double>{0.0, 0.6 * c, 0.0});
    CHECK(std::abs(across.x - 0.8) < 1e-12);
    CHECK(std::abs(across.y - 0.6) < 1e-12);
    CHECK(std::abs(across.z) < 1e-12);

    // A source theta from the motion is seen theta' from it, with
    // cos(theta') = (cos(theta) + beta) / (1 + beta cos(theta)), whatever the directions:
    const std::vector<Vec3<double>> directions{
        glm::normalize(Vec3<double>{1.0, 2.0, -0.5}),
        glm::normalize(Vec3<double>{-0.3, 0.1, 1.0}),
        glm::normalize(Vec3<double>{0.0, -1.0, -1.0}),
    };
    const std::vector<Vec3<double>> velocities{
        Vec3<double>{0.5 * c, 0.0, 0.0},
        Vec3<double>{0.0, -1e-4 * c, 2e-4 * c},
        Vec3<double>{0.2 * c, 0.3 * c, -0.1 * c},
    };
    for (const Vec3<double>& u : directions) {
        for (const Vec3<double>& v : velocities) {
            const Vec3<double> seen = compute_aberrated_direction(u, v);
            const double beta = glm::length(v) / c;
            const Vec3<double> motion = glm::normalize(v);
            const double cos_theta = glm::dot(u, motion);
            CHECK(std::abs(glm::length(seen) - 1.0) < 1e-12);
            CHECK(std::abs(glm::dot(seen, motion) - (cos_theta + beta) / (1.0 + beta * cos_theta)) <
                  1e-12);
            // It stays in the plane of the direction and the motion:
            CHECK(std::abs(glm::dot(seen, glm::normalize(glm::cross(u, motion)))) < 1e-12);
        }
    }

    // An observer at rest sees a direction as it is:
    const Vec3<double> u = directions[0];
    CHECK(compute_aberrated_direction(u, Vec3<double>{0.0}) == u);
}

TEST_CASE("Stars are placed by the observation mode, as bodies are", "[render][observation_mode]")
{
    // A star and a still, distant body in the same direction are seen in the same place in every
    // observation mode: straight ahead in TRUE_STATE and GEOMETRIC_STATE, and aberrated toward
    // the camera's motion (+y) by 100 pixels in ABERRATED_STATE.
    const Vec2<double> ahead = drawn_at(Source::Body, ObservationMode::TRUE_STATE);
    for (ObservationMode mode : {ObservationMode::TRUE_STATE,
                                 ObservationMode::GEOMETRIC_STATE,
                                 ObservationMode::ABERRATED_STATE}) {
        INFO("observation mode " << static_cast<int>(mode));
        const Vec2<double> expected =
            ahead + Vec2<double>{0.0, mode == ObservationMode::ABERRATED_STATE ? 100.0 : 0.0};
        const Vec2<double> star = drawn_at(Source::Star, mode);
        const Vec2<double> body = drawn_at(Source::Body, mode);
        CHECK(std::abs(star.x - expected.x) < 0.05);
        CHECK(std::abs(star.y - expected.y) < 0.05);
        CHECK(std::abs(body.x - expected.x) < 0.05);
        CHECK(std::abs(body.y - expected.y) < 0.05);
    }
}

TEST_CASE("A scene view observes as a camera sees, unless told otherwise",
          "[render][observation_mode]")
{
    // Given no observation mode, a scene view draws a star and a distant body where
    // ABERRATED_STATE does:
    for (Source source : {Source::Star, Source::Body}) {
        const Vec2<double> by_default = drawn_at(source, std::nullopt);
        const Vec2<double> aberrated = drawn_at(source, ObservationMode::ABERRATED_STATE);
        CHECK(glm::length(by_default - aberrated) < 1e-9);
    }

    // A pose context made without one, as for trying out a position or rotation function, has
    // it too:
    const Time time = Time::from_et(0.0);
    CHECK(PoseContext{time, time}.observation_mode == ObservationMode::ABERRATED_STATE);
}
