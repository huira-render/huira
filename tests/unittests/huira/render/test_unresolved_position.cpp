#include <array>
#include <cmath>
#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

enum class Stamp { None, Airy, Defocus };

/// Where unresolved sources are placed: a 32x32 sensor of 10 um pixels behind a 50 mm lens, so
/// one pixel is 1/5000 rad and the principal point is (16, 16).
constexpr double FOCAL_PIXELS = 5000.0;
constexpr double RANGE = 1000.0;

/// Sub-pixel positions, in the camera's pixel-edge coordinates (pixel i covers [i, i + 1)),
/// spread over a pixel in both axes so that every stamp phase is exercised.
const std::array<Vec2<double>, 7> POSITIONS{
    Vec2<double>{15.03, 16.97},
    Vec2<double>{15.17, 16.62},
    Vec2<double>{15.31, 16.44},
    Vec2<double>{15.50, 16.50},
    Vec2<double>{15.62, 16.21},
    Vec2<double>{15.78, 16.09},
    Vec2<double>{15.94, 16.31},
};

double total(const Image<RGB>& image)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        sum += static_cast<double>(image[i][0]);
    }
    return sum;
}

/// Centroid in pixel-edge coordinates: pixel x's light is centered at x + 0.5.
Vec2<double> centroid(const Image<RGB>& image)
{
    double sum = 0.0;
    double sx = 0.0;
    double sy = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double w = static_cast<double>(image(x, y)[0]);
            sum += w;
            sx += w * (x + 0.5);
            sy += w * (y + 0.5);
        }
    }
    return {sx / sum, sy / sum};
}

/// Renders a single unresolved source aimed at each of POSITIONS, and checks where it lands
/// against the camera's own projection of the source.
template <typename Check>
void render_positions(Stamp stamp, Check check)
{
    Scene<RGB> scene;
    // Sensor first: each setter recomputes per-pixel camera data at the current resolution,
    // and the default sensor is 1024x1024, which is slow in unoptimized builds.
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({32, 32}, 10_um);
    camera_model.set_focal_length(50_mm);
    if (stamp == Stamp::Airy) {
        // f/32 puts the first dark ring at about 2.1 px. A much narrower PSF would be
        // undersampled, and its pixel-integrated centroid genuinely sticks to pixel positions.
        camera_model.set_fstop(32.0f);
        camera_model.use_aperture_psf(12, 16);
    } else {
        camera_model.set_fstop(4.0f);
    }
    if (stamp == Stamp::Defocus) {
        // About a 2 px blur radius.
        camera_model.set_focus_sensor_offset(units::Micrometer(160.0));
        REQUIRE(camera_model.defocus_blur_radius() > 1.5f);
    }
    auto camera = scene.root.new_instance(camera_model);

    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));

    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};

    for (const Vec2<double>& p : POSITIONS) {
        const Vec3<double> position{
            (p.x - 16.0) / FOCAL_PIXELS * RANGE, (p.y - 16.0) / FOCAL_PIXELS * RANGE, RANGE};
        source.set_position(
            units::Meter(position.x), units::Meter(position.y), units::Meter(RANGE));
        const Pixel projected = camera_model.project_point(Vec3<float>(position));

        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);

        INFO("projected to (" << projected.x << ", " << projected.y << ")");
        check(frame_buffer.received_power(), Vec2<double>{projected.x, projected.y});
    }
}

} // namespace

TEST_CASE("Unresolved sources land where the camera projects them", "[render][unresolved]")
{
    SECTION("Without a PSF, all the light goes to the pixel the source falls in")
    {
        render_positions(Stamp::None, [](const Image<RGB>& image, Vec2<double> p) {
            const int x = static_cast<int>(std::floor(p.x));
            const int y = static_cast<int>(std::floor(p.y));
            REQUIRE(total(image) > 0.0);
            CHECK(static_cast<double>(image(x, y)[0]) == total(image));
        });
    }

    // Stamps hold the kernel pre-shifted in 1/16 pixel banks, and each source uses the nearest
    // bank. So the centroid lands within 1/32 px of the projection, and much closer to the
    // projection rounded to the nearest bank; what is left there is the pixelated edge of the
    // defocus disk and the truncated tail of the Airy pattern. Before this was fixed, stamps
    // were half a pixel off in both axes, and taking the bank below rather than the nearest
    // one added up to a further 1/16 px towards -x and -y.
    for (Stamp stamp : {Stamp::Airy, Stamp::Defocus}) {
        DYNAMIC_SECTION((stamp == Stamp::Airy ? "Airy PSF stamp" : "Defocus disk stamp"))
        {
            render_positions(stamp, [](const Image<RGB>& image, Vec2<double> p) {
                // Stamps work in pixel-center coordinates, half a pixel below the projection:
                auto nearest_bank = [](double v) {
                    return std::round((v - 0.5) * 16.0) / 16.0 + 0.5;
                };
                const Vec2<double> c = centroid(image);
                INFO("centroid (" << c.x << ", " << c.y << ")");
                CHECK(std::abs(c.x - p.x) < 0.05);
                CHECK(std::abs(c.y - p.y) < 0.05);
                CHECK(std::abs(c.x - nearest_bank(p.x)) < 0.025);
                CHECK(std::abs(c.y - nearest_bank(p.y)) < 0.025);
            });
        }
    }
}
