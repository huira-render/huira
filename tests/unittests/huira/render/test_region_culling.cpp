#include <cmath>
#include <cstddef>

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

} // namespace

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
