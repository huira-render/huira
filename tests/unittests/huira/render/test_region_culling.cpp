#include <cmath>
#include <cstddef>
#include <optional>
#include <random>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// Renders a small sunlit ball that lies entirely outside the pinhole field of view, with
/// region culling on or off, and returns the received power.
///
/// The camera is focused very close with a large aperture, so the depth-of-field rays from
/// the frame edge are tilted well outside the pinhole field. Beyond the focus distance they
/// diverge, and some of them reach the ball: the defocused ball bleeds into the frame edge.
/// Region culling must not discard that light.
Image<RGB> render_out_of_field_ball(float focus_distance_m, bool region_culling)
{
    Scene<RGB> scene;

    // Sensor first: each setter recomputes per-pixel camera data at the current resolution,
    // and the default sensor is 1024x1024, which is slow in unoptimized builds.
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({64, 48}, 200_um);
    camera_model.set_focal_length(20_mm);
    camera_model.set_fstop(1.0f);
    camera_model.enable_depth_of_field();
    camera_model.set_focus_distance(units::Meter(focus_distance_m));
    auto camera = scene.root.new_instance(camera_model);

    // The pinhole field reaches 17.7 degrees off axis horizontally, and the edge tiles' cones
    // about 20 degrees. The ball's centre is 24 degrees off axis at 1 m, and its 5 cm radius
    // keeps all of it beyond 21 degrees, so only tilted depth-of-field rays can reach it.
    auto ball_geometry = scene.add_ellipsoid(5_cm, 5_cm, 5_cm);
    auto ball_primitive = scene.add_primitive(ball_geometry);
    auto ball = scene.root.new_instance(ball_primitive);
    ball.set_position(units::Meter(std::tan(24.0 * PI<double>() / 180.0)), 0_m, 1_m);

    // Behind the camera, lighting the face of the ball that the camera sees.
    auto sun_light = scene.new_sun_light();
    auto sun = scene.root.new_instance(sun_light);
    sun.set_position(0_m, 0_m, -1_au);

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();

    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(16);
    renderer.set_max_bounces(1);
    renderer.set_region_culling(region_culling);

    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);

    return frame_buffer.received_power();
}

double total_power(const Image<RGB>& image)
{
    double total = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            total += static_cast<double>(image[i][c]);
        }
    }
    return total;
}

/// A camera, and how the scene is laid out around it, for comparing frames rendered with
/// region culling and without.
struct CullingCase {
    std::string name;
    double pitch_um = 50.0;
    double focal_mm = 50.0;
    bool blender = false;
    double roll_deg = 0.0;
    std::optional<float> cx = std::nullopt;
    std::optional<float> cy = std::nullopt;
    double k1 = 0.0;
    bool depth_of_field = false;
    double near_m = 50.0;
    double far_m = 500.0;
    double motion_px = 0.0;
    std::size_t temporal_samples = 1;
};

/// Renders balls 2 to 20 pixels in radius, placed at random across the frame and a little
/// beyond it, with region culling on or off.
Image<RGB> render_balls(const CullingCase& c, bool region_culling)
{
    constexpr int WIDTH = 128;
    constexpr int HEIGHT = 96;
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    if (c.blender) {
        camera_model.use_blender_convention();
    }
    camera_model.configure_sensor_from_pitch(
        {WIDTH, HEIGHT}, units::Micrometer(c.pitch_um), std::nullopt, c.cx, c.cy);
    camera_model.set_focal_length(units::Millimeter(c.focal_mm));
    camera_model.enable_diffraction(false);
    camera_model.enable_depth_of_field(c.depth_of_field);
    if (c.depth_of_field) {
        camera_model.set_fstop(1.4f);
        camera_model.set_focus_distance(2_m);
    }
    if (c.roll_deg != 0.0) {
        camera_model.set_sensor_roll(units::Degree(c.roll_deg));
    }
    if (c.k1 != 0.0) {
        camera_model.set_brown_conrady_distortion(BrownCoefficients{c.k1, 0.0, 0.0, 0.0, 0.0});
    }
    auto camera = scene.root.new_instance(camera_model);

    // Out to 65% of the half-diagonal of the field, which the corners of a 4:3 frame reach at
    // 80% and its edges at 48% and 64%:
    const double pixel_angle = c.pitch_um * 1e-3 / c.focal_mm;
    const double half_diagonal =
        std::atan(0.5 * std::hypot(WIDTH, HEIGHT) * c.pitch_um * 1e-3 / c.focal_mm);
    const double forward = c.blender ? -1.0 : 1.0;
    auto material = scene.new_material(scene.new_bsdf_lambertian());
    for (int i = 0; i < 8; ++i) {
        const double theta = 0.65 * half_diagonal * std::sqrt(uniform(rng));
        const double phi = 2.0 * PI<double>() * uniform(rng);
        const double distance = c.near_m + (c.far_m - c.near_m) * uniform(rng);
        const double radius = (2.0 + 18.0 * uniform(rng)) * pixel_angle * distance;
        auto ball = scene.root.new_instance(scene.add_primitive(
            scene.add_ellipsoid(units::Meter(radius), units::Meter(radius), units::Meter(radius)),
            material));
        ball.set_position(units::Meter(distance * std::sin(theta) * std::cos(phi)),
                          units::Meter(distance * std::sin(theta) * std::sin(phi)),
                          units::Meter(distance * forward * std::cos(theta)));
        const double speed = c.motion_px * pixel_angle * distance;
        const double heading = 2.0 * PI<double>() * uniform(rng);
        ball.set_velocity(units::MetersPerSecond(speed * std::cos(heading)),
                          units::MetersPerSecond(speed * std::sin(heading)),
                          0_mps);
    }
    auto sun = scene.root.new_instance(scene.new_sun_light());
    sun.set_position(0_m, 0_m, units::Meter(-forward * 1.496e11));

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(4);
    renderer.set_region_culling(region_culling);
    SceneView<RGB> scene_view(scene,
                              Interval{Time::from_et(0.0), Time::from_et(1.0)},
                              camera,
                              ObservationMode::TRUE_STATE,
                              c.temporal_samples);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

std::size_t mismatched_pixels(const Image<RGB>& a, const Image<RGB>& b)
{
    std::size_t mismatched = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            if (a[i][c] != b[i][c]) {
                ++mismatched;
                break;
            }
        }
    }
    return mismatched;
}

} // namespace

TEST_CASE("Region culling changes nothing, whatever the camera", "[render][culling]")
{
    // Culling skips the tiles that nothing can reach, and each tile's samples are seeded by the
    // tile, so a frame must come out the same with culling as without it.
    std::vector<CullingCase> cases;
    cases.push_back({.name = "a 50 mm lens"});
    // A narrow field of view, where the angles culling compares are a few microradians: the
    // arc cosine of a single-precision dot product cannot resolve them.
    cases.push_back({.name = "10 microradian pixels", .pitch_um = 10.0, .focal_mm = 1000.0});
    cases.push_back({.name = "1 microradian pixels", .pitch_um = 5.0, .focal_mm = 5000.0});
    cases.push_back({.name = "10 microradian pixels, moving",
                     .pitch_um = 10.0,
                     .focal_mm = 1000.0,
                     .motion_px = 20.0,
                     .temporal_samples = 2});
    cases.push_back({.name = "a 120 degree field",
                     .pitch_um = 270.0,
                     .focal_mm = 10.0,
                     .near_m = 5.0,
                     .far_m = 50.0});
    cases.push_back({.name = "Blender's convention, a rolled sensor and an off-center principal "
                             "point",
                     .blender = true,
                     .roll_deg = 30.0,
                     .cx = 20.f,
                     .cy = 80.f});
    cases.push_back({.name = "barrel distortion", .pitch_um = 100.0, .k1 = -0.3});
    cases.push_back({.name = "pincushion distortion", .pitch_um = 100.0, .k1 = 0.3});
    cases.push_back({.name = "depth of field, focused at 2 m",
                     .depth_of_field = true,
                     .near_m = 1.0,
                     .far_m = 30.0});

    for (const CullingCase& c : cases) {
        INFO(c.name);
        const Image<RGB> culled = render_balls(c, true);
        const Image<RGB> reference = render_balls(c, false);
        CHECK(mismatched_pixels(culled, reference) == 0);
    }
}

TEST_CASE("Region culling keeps depth-of-field light from outside the pinhole field",
          "[render][culling][focus]")
{
    // Per-tile sampling is seeded by tile index, so culling must change nothing at all: any
    // difference means a tile was skipped whose rays would have hit the ball.
    for (float focus : {0.05f, -0.05f}) {
        DYNAMIC_SECTION("focus distance " << focus << " m")
        {
            const Image<RGB> culled = render_out_of_field_ball(focus, true);
            const Image<RGB> reference = render_out_of_field_ball(focus, false);

            // Guard against a vacuous pass: the defocused ball must reach the frame.
            REQUIRE(total_power(reference) > 0.0);

            REQUIRE(culled.size() == reference.size());
            std::size_t mismatched = 0;
            for (std::size_t i = 0; i < reference.size(); ++i) {
                for (std::size_t c = 0; c < 3; ++c) {
                    if (culled[i][c] != reference[i][c]) {
                        ++mismatched;
                        break;
                    }
                }
            }
            INFO("culled total " << total_power(culled) << ", reference total "
                                 << total_power(reference));
            REQUIRE(mismatched == 0);
        }
    }
}
