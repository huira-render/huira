#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

enum class Stamp { None, Airy, Defocus };

/// Where unresolved sources are placed: a 32x32 sensor of 10 um pixels behind a 50 mm lens, so
/// one pixel is 1/5000 rad and the optical axis meets the sensor at its center.
constexpr int SIZE = 32;
constexpr double FOCAL_PIXELS = 5000.0;
constexpr double RANGE = 1000.0;

/// Sub-pixel positions in sensor coordinates (x right and y down from the sensor's top-left
/// corner, so the center is (16, 16)), spread over a pixel in both axes so that every stamp
/// phase is exercised.
const std::array<Vec2<double>, 7> SENSOR_POSITIONS{
    Vec2<double>{15.03, 16.97},
    Vec2<double>{15.17, 16.62},
    Vec2<double>{15.31, 16.44},
    Vec2<double>{15.50, 16.50},
    Vec2<double>{15.62, 16.21},
    Vec2<double>{15.78, 16.09},
    Vec2<double>{15.94, 16.31},
};

/// Coordinate of the first pixel's center in a convention, written out independently of
/// PixelConvention so that the test checks it rather than repeats it.
double first_center(const PixelConvention& convention)
{
    switch (convention.center) {
    case PixelCenter::Integer:
        return 0.0;
    case PixelCenter::HalfInteger:
        return 0.5;
    case PixelCenter::OneBased:
        return 1.0;
    default:
        return 0.0;
    }
}

/// Center of the pixel at column i and row j (row 0 at the top) in a convention.
Vec2<double> pixel_center(int i, int j, const PixelConvention& convention)
{
    const double c = first_center(convention);
    const int row_from_origin = convention.origin == PixelOrigin::TopLeft ? j : SIZE - 1 - j;
    return {i + c, row_from_origin + c};
}

double total(const Image<RGB>& image)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        sum += static_cast<double>(image[i][0]);
    }
    return sum;
}

/// Centroid in a convention.
Vec2<double> centroid(const Image<RGB>& image, const PixelConvention& convention)
{
    double sum = 0.0;
    Vec2<double> weighted{0.0, 0.0};
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double w = static_cast<double>(image(x, y)[0]);
            sum += w;
            weighted += w * pixel_center(x, y, convention);
        }
    }
    return weighted / sum;
}

std::string name(const PixelConvention& convention)
{
    if (convention == PixelConvention::opencv()) {
        return "opencv";
    }
    if (convention == PixelConvention::colmap()) {
        return "colmap";
    }
    if (convention == PixelConvention::matlab()) {
        return "matlab";
    }
    return "fits";
}

/// Renders a single unresolved source aimed at each of SENSOR_POSITIONS, and checks where it
/// lands against the camera's own projection of the source, in the given convention.
template <typename Check>
void render_positions(Stamp stamp, const PixelConvention& convention, Check check)
{
    Scene<RGB> scene;
    // Sensor first: each setter recomputes per-pixel camera data at the current resolution,
    // and the default sensor is 1024x1024, which is slow in unoptimized builds.
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, 10_um);
    camera_model.set_focal_length(50_mm);
    camera_model.set_pixel_convention(convention);
    if (stamp == Stamp::Airy) {
        // f/32 puts the first dark ring at about 2.1 px. A much narrower PSF would be
        // undersampled, and its pixel-integrated centroid genuinely sticks to pixel positions.
        camera_model.set_fstop(32.0f);
        camera_model.use_aperture_psf(12, 16);
    } else {
        camera_model.set_fstop(4.0f);
        camera_model.delete_psf(); // the aperture's, by default
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

    for (const Vec2<double>& s : SENSOR_POSITIONS) {
        const double center = SIZE / 2.0;
        const Vec3<double> position{
            (s.x - center) / FOCAL_PIXELS * RANGE, (s.y - center) / FOCAL_PIXELS * RANGE, RANGE};
        source.set_position(
            units::Meter(position.x), units::Meter(position.y), units::Meter(RANGE));
        const Pixel projected = camera_model.project_point(Vec3<float>(position));

        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);

        INFO(name(convention) << ": projected to (" << projected.x << ", " << projected.y << ")");
        check(frame_buffer.received_power(), Vec2<double>{projected.x, projected.y});
    }
}

/// A camera of REFERENCE_SIZE pixels square, with the optical axis at its center, that the
/// windows below are placed on.
constexpr int REFERENCE_SIZE = 192;

/// A camera of the given resolution whose pixel (0, 0) is the reference camera's pixel
/// (offset_x, offset_y). All share pitch, focal length and optical axis, so a source falls at
/// the same subpixel position in each, a whole number of pixels apart.
struct Window {
    const char* where;
    Resolution resolution{0, 0};
    int offset_x;
    int offset_y;
};

/// Renders one unresolved source with the camera of each window in turn. The source is given by
/// where it falls on the reference camera, in sensor coordinates.
std::vector<Image<RGB>>
render_windows(Stamp stamp, Vec2<double> source, const std::vector<Window>& windows)
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    // In this convention pixel edges are at integers, the same as sensor coordinates:
    camera_model.set_pixel_convention(PixelConvention::colmap());
    camera_model.configure_sensor_from_pitch(windows.front().resolution, 10_um);
    camera_model.set_focal_length(50_mm);
    if (stamp == Stamp::Airy) {
        camera_model.set_fstop(32.0f);
        camera_model.use_aperture_psf(12, 16);
    } else {
        // About a 6 px blur radius.
        camera_model.set_fstop(4.0f);
        camera_model.delete_psf(); // the aperture's, by default
        camera_model.set_focus_sensor_offset(units::Micrometer(480.0));
    }
    auto camera = scene.root.new_instance(camera_model);

    const double axis = REFERENCE_SIZE / 2.0;
    auto emitter = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
    emitter.set_position(units::Meter((source.x - axis) / FOCAL_PIXELS * RANGE),
                         units::Meter((source.y - axis) / FOCAL_PIXELS * RANGE),
                         units::Meter(RANGE));

    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    std::vector<Image<RGB>> images;
    for (const Window& window : windows) {
        // Only the sensor changes, so the PSF is built once:
        camera_model.configure_sensor_from_pitch(window.resolution,
                                                 10_um,
                                                 std::nullopt,
                                                 static_cast<float>(axis - window.offset_x),
                                                 static_cast<float>(axis - window.offset_y));
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);
        images.push_back(frame_buffer.received_power());
    }
    return images;
}

} // namespace

TEST_CASE("Unresolved sources land where the camera projects them", "[render][unresolved]")
{
    const std::array<PixelConvention, 4> all_conventions{PixelConvention::opencv(),
                                                         PixelConvention::colmap(),
                                                         PixelConvention::matlab(),
                                                         PixelConvention::fits()};

    SECTION("Without a PSF, all the light goes to the pixel the source falls in")
    {
        for (const PixelConvention& convention : all_conventions) {
            render_positions(Stamp::None, convention, [&](const Image<RGB>& image, Vec2<double> p) {
                REQUIRE(total(image) > 0.0);

                // A single pixel holds all the light...
                double brightest = 0.0;
                for (std::size_t i = 0; i < image.size(); ++i) {
                    brightest = std::max(brightest, static_cast<double>(image[i][0]));
                }
                CHECK(brightest == total(image));

                // ...and it is the one the projection falls in, in the camera's convention:
                // its center (the centroid) is within half a pixel.
                const Vec2<double> c = centroid(image, convention);
                CHECK(std::abs(c.x - p.x) <= 0.5);
                CHECK(std::abs(c.y - p.y) <= 0.5);
            });
        }
    }

    // The aperture's PSF is drawn at the source's exact position, so its centroid lands on the
    // projection, but for the pull of the stamp's truncated tail towards the pixel's center (up
    // to 0.012 px for this wide pattern). Defocus stamps hold the kernel pre-shifted in 1/16
    // pixel banks, and each source uses the nearest bank, so the centroid lands within 1/32 px
    // of the projection, and much closer to the projection rounded to the nearest bank; what is
    // left there is the pixelated edge of the defocus disk. Before this was fixed, stamps were
    // half a pixel off in both axes, and taking the bank below rather than the nearest one
    // added up to a further 1/16 px towards -x and -y.
    for (Stamp stamp : {Stamp::Airy, Stamp::Defocus}) {
        for (const PixelConvention& convention :
             {PixelConvention::opencv(), PixelConvention::fits()}) {
            DYNAMIC_SECTION((stamp == Stamp::Airy ? "Airy PSF stamp, " : "Defocus disk stamp, ")
                            << name(convention))
            {
                render_positions(stamp, convention, [&](const Image<RGB>& image, Vec2<double> p) {
                    // Banks are spaced from pixel centers:
                    const double c0 = first_center(convention);
                    auto nearest_bank = [c0](double v) {
                        return std::round((v - c0) * 16.0) / 16.0 + c0;
                    };
                    const Vec2<double> c = centroid(image, convention);
                    INFO("centroid (" << c.x << ", " << c.y << ")");
                    if (stamp == Stamp::Airy) {
                        CHECK(std::abs(c.x - p.x) < 0.015);
                        CHECK(std::abs(c.y - p.y) < 0.015);
                    } else {
                        CHECK(std::abs(c.x - p.x) < 0.05);
                        CHECK(std::abs(c.y - p.y) < 0.05);
                        CHECK(std::abs(c.x - nearest_bank(p.x)) < 0.025);
                        CHECK(std::abs(c.y - nearest_bank(p.y)) < 0.025);
                    }
                });
            }
        }
    }
}

TEST_CASE("A source's light is the same wherever it falls relative to tiles and the image's edges",
          "[render][unresolved]")
{
    // Unresolved sources are rendered in 64 px tiles. A source straddling tiles has to be
    // stamped by each of them, and one just outside the image still lights its edge: before,
    // its light was dropped, because only sources projecting inside the image were rendered.
    const Vec2<double> source{96.3, 96.7};

    // The reference camera holds the whole stamp, in the middle of a tile. The others are 128 px
    // square, 2 x 2 tiles, and placed so that the source falls...
    const std::vector<Window> windows{
        Window{"reference", {REFERENCE_SIZE, REFERENCE_SIZE}, 0, 0},
        Window{"on the corner of four tiles", {128, 128}, 32, 32},            // at (64.3, 64.7)
        Window{"just beyond the left edge", {128, 128}, 99, 32},              // at (-2.7, 64.7)
        Window{"just beyond the top-left corner", {128, 128}, 99, 99},        // at (-2.7, -2.3)
        Window{"just beyond the bottom-right corner", {128, 128}, -34, -34}}; // (130.3, 130.7)

    for (Stamp stamp : {Stamp::Airy, Stamp::Defocus}) {
        const std::vector<Image<RGB>> images = render_windows(stamp, source, windows);
        const Image<RGB>& reference = images.front();
        double peak = 0.0;
        for (std::size_t i = 0; i < reference.size(); ++i) {
            peak = std::max(peak, static_cast<double>(reference[i][0]));
        }
        REQUIRE(peak > 0.0);

        for (std::size_t w = 1; w < windows.size(); ++w) {
            const Window& window = windows[w];
            const Image<RGB>& image = images[w];
            INFO((stamp == Stamp::Airy ? "Airy PSF stamp, " : "Defocus disk stamp, ")
                 << "source " << window.where);
            CHECK(total(image) > 0.0);

            // Every pixel matches the reference's, and is dark beyond it:
            double worst = 0.0;
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const int rx = x + window.offset_x;
                    const int ry = y + window.offset_y;
                    const bool in_reference =
                        rx >= 0 && rx < REFERENCE_SIZE && ry >= 0 && ry < REFERENCE_SIZE;
                    const double expected =
                        in_reference ? static_cast<double>(reference(rx, ry)[0]) : 0.0;
                    worst =
                        std::max(worst, std::abs(static_cast<double>(image(x, y)[0]) - expected));
                }
            }
            CHECK(worst <= 1e-5 * peak);
        }
    }
}
