#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

using Setting = std::function<void(CameraModel<RGB>&)>;

/// Builds a camera from settings applied in the given order.
std::unique_ptr<CameraModel<RGB>> build(const std::vector<Setting>& settings)
{
    auto camera = std::make_unique<CameraModel<RGB>>();
    for (const Setting& apply : settings) {
        apply(*camera);
    }
    return camera;
}

bool close(double a, double b, double relative = 1e-6)
{
    return std::abs(a - b) <= relative * std::max(std::abs(a), std::abs(b));
}

// A small sensor (fast to set up) with strong distortion, and an off-center principal point.
const Setting SENSOR = [](CameraModel<RGB>& c) {
    c.configure_sensor_from_pitch(
        Resolution{64, 48}, units::Micrometer(200.0), std::nullopt, 30.f, 26.f);
};
const Setting FOCAL_LENGTH = [](CameraModel<RGB>& c) { c.set_focal_length(20_mm); };
const Setting BARREL = [](CameraModel<RGB>& c) {
    c.set_brown_conrady_distortion(BrownCoefficients(-0.2, 0.05, 0.0, 0.0, 0.0));
};
const Setting PINCUSHION = [](CameraModel<RGB>& c) {
    c.set_brown_conrady_distortion(BrownCoefficients(0.2, 0.0, 0.0, 0.0, 0.0));
};
const Setting BLENDER = [](CameraModel<RGB>& c) { c.use_blender_convention(); };

/// Sensor positions spread over the whole sensor, including the far edges of the last column
/// and row (the sensor spans [0, 64] x [0, 48]).
const std::array<Pixel, 6> POSITIONS{
    Pixel{0.f, 0.f},
    Pixel{20.25f, 10.75f},
    Pixel{63.5f, 24.f},
    Pixel{32.f, 47.5f},
    Pixel{63.9f, 47.9f},
    Pixel{64.f, 48.f},
};

/// Everything a render reads from the camera's derived geometry must match.
void check_same_geometry(const CameraModel<RGB>& a, const CameraModel<RGB>& b)
{
    for (const Pixel& p : POSITIONS) {
        INFO("sensor position (" << p.x << ", " << p.y << ")");
        const Vec3<float> da = a.cast_ray(p).direction();
        const Vec3<float> db = b.cast_ray(p).direction();
        CHECK(glm::dot(da, db) > 1.f - 1e-6f);
        CHECK(a.in_fov(da * 10.f) == b.in_fov(db * 10.f));
    }
    for (int y : {0, 23, 47}) {
        for (int x : {0, 31, 63}) {
            INFO("pixel (" << x << ", " << y << ")");
            CHECK(close(a.pixel_radiance_to_power(x, y), b.pixel_radiance_to_power(x, y)));
        }
    }
}

} // namespace

TEST_CASE("Camera geometry does not depend on the order it is set up in", "[cameras][geometry]")
{
    // Distortion and the Blender convention used to be applied only to what had already been
    // set up: set first, a later sensor change left the distortion table stale (rays several
    // pixels off), and set last, the pixel solid angles ignored the distortion (radiometry
    // about 10% off in the corners) or the frustum faced the wrong way (every star culled).
    SECTION("Distortion")
    {
        auto first = build({BARREL, SENSOR, FOCAL_LENGTH});
        auto last = build({SENSOR, FOCAL_LENGTH, BARREL});
        check_same_geometry(*first, *last);
    }

    SECTION("Blender convention")
    {
        auto first = build({BLENDER, SENSOR, FOCAL_LENGTH, BARREL});
        auto last = build({SENSOR, FOCAL_LENGTH, BARREL, BLENDER});
        check_same_geometry(*first, *last);
        CHECK(last->in_fov(Vec3<float>{0.f, 0.f, -1.f})); // Blender cameras look along -Z
    }

    SECTION("Deleting distortion")
    {
        auto deleted = build({SENSOR, FOCAL_LENGTH, BARREL});
        deleted->delete_distortion();
        auto never = build({SENSOR, FOCAL_LENGTH});
        check_same_geometry(*deleted, *never);
    }
}

TEST_CASE("Distorted rays cover the whole sensor", "[cameras][geometry]")
{
    // The distortion table holds a direction for every pixel corner, out to the far edges of
    // the last column and row. With one per pixel, positions in the last column and row were
    // clamped to its left/top edge.
    for (const Setting& distortion : {BARREL, PINCUSHION}) {
        auto camera = build({SENSOR, FOCAL_LENGTH, distortion});
        for (const Pixel& p : POSITIONS) {
            INFO("sensor position (" << p.x << ", " << p.y << ")");
            const Pixel round_trip = camera->project_point(camera->cast_ray(p).direction());
            CHECK(std::abs(round_trip.x - p.x) < 0.01f);
            CHECK(std::abs(round_trip.y - p.y) < 0.01f);
        }
    }
}

TEST_CASE("The view frustum holds the whole sensor and nothing far outside it",
          "[cameras][geometry]")
{
    // The frustum used to be built through the last column's and row's left/top edges, which
    // culled stars in them, and from the most oblique point of each edge, which with pincushion
    // distortion is not the one that reaches furthest out.
    for (const Setting& distortion : {BARREL, PINCUSHION}) {
        auto camera = build({SENSOR, FOCAL_LENGTH, distortion});
        for (int y = 0; y < 48; ++y) {
            for (float x : {0.05f, 0.5f, 63.5f, 63.95f}) {
                INFO("sensor position (" << x << ", " << y << ".5)");
                const Pixel p{x, static_cast<float>(y) + 0.5f};
                CHECK(camera->in_fov(camera->cast_ray(p).direction() * 10.f));
            }
        }
        for (int x = 0; x < 64; ++x) {
            for (float y : {0.05f, 0.5f, 47.5f, 47.95f}) {
                INFO("sensor position (" << x << ".5, " << y << ")");
                const Pixel p{static_cast<float>(x) + 0.5f, y};
                CHECK(camera->in_fov(camera->cast_ray(p).direction() * 10.f));
            }
        }

        // The field of view is about 35 by 27 degrees; 60 degrees off axis is well outside.
        CHECK_FALSE(camera->in_fov(Vec3<float>{std::tan(1.047f), 0.f, 1.f}));
        CHECK_FALSE(camera->in_fov(Vec3<float>{0.f, -std::tan(1.047f), 1.f}));
    }
}

TEST_CASE("Unresolved sources in the last pixel column and row are rendered",
          "[cameras][geometry][render]")
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({32, 32}, 10_um);
    camera_model.set_focal_length(50_mm);
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));

    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};

    auto render_at = [&](double x, double y) {
        // Sensor positions: 5000 px focal length, principal point at the sensor center.
        const double z = 1000.0;
        source.set_position(units::Meter((x - 16.0) / 5000.0 * z),
                            units::Meter((y - 16.0) / 5000.0 * z),
                            units::Meter(z));
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);
        double total = 0.0;
        for (std::size_t i = 0; i < frame_buffer.received_power().size(); ++i) {
            total += static_cast<double>(frame_buffer.received_power()[i][0]);
        }
        return total;
    };

    // Off-axis sources receive slightly less (cosine and distance, ~1e-5 here); culled ones
    // received nothing at all.
    const double interior = render_at(16.5, 16.5);
    REQUIRE(interior > 0.0);
    CHECK(close(render_at(31.5, 16.5), interior, 1e-3)); // last column
    CHECK(close(render_at(16.5, 31.5), interior, 1e-3)); // last row
    CHECK(close(render_at(31.9, 31.9), interior, 1e-3)); // last pixel
}
