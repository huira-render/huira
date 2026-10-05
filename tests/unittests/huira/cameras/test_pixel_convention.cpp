#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

constexpr int W = 64;
constexpr int H = 48;

const std::array<PixelConvention, 4> PRESETS{PixelConvention::opencv(),
                                             PixelConvention::colmap(),
                                             PixelConvention::matlab(),
                                             PixelConvention::fits()};

std::string name(const PixelConvention& c)
{
    return c.to_string();
}

/// Sensor coordinates (x right and y down from the top-left corner, pixel edges at integers) of
/// a position given in a convention. Written out independently of PixelConvention, so that the
/// tests check its conversions rather than repeat them.
Pixel_d to_sensor(const PixelConvention& c, Pixel_d p)
{
    const double first = c.center == PixelCenter::Integer       ? 0.0
                         : c.center == PixelCenter::HalfInteger ? 0.5
                                                                : 1.0;
    const double x = p.x - first + 0.5;
    const double y_from_origin = p.y - first + 0.5;
    return {x, c.origin == PixelOrigin::TopLeft ? y_from_origin : H - y_from_origin};
}

/// The inverse of to_sensor().
Pixel_d from_sensor(const PixelConvention& c, Pixel_d s)
{
    const double first = c.center == PixelCenter::Integer       ? 0.0
                         : c.center == PixelCenter::HalfInteger ? 0.5
                                                                : 1.0;
    const double y_from_origin = c.origin == PixelOrigin::TopLeft ? s.y : H - s.y;
    return {s.x + first - 0.5, y_from_origin + first - 0.5};
}

std::unique_ptr<CameraModel<RGB>> make_camera(const PixelConvention& convention)
{
    auto camera = std::make_unique<CameraModel<RGB>>();
    camera->configure_sensor_from_pitch(Resolution{W, H}, units::Micrometer(200.0));
    camera->set_focal_length(20_mm);
    camera->set_pixel_convention(convention);
    return camera;
}

/// Points in front of the camera, spread across the field of view.
const std::array<Vec3<float>, 5> POINTS{
    Vec3<float>{0.f, 0.f, 10.f},
    Vec3<float>{1.f, 0.5f, 10.f},
    Vec3<float>{-2.f, 1.5f, 10.f},
    Vec3<float>{2.5f, -1.8f, 10.f},
    Vec3<float>{-0.3f, -0.7f, 2.f},
};

/// Two cameras have the same geometry when every point lands on the same place on the sensor.
void check_same_sensor_positions(const CameraModel<RGB>& a, const CameraModel<RGB>& b)
{
    for (const Vec3<float>& point : POINTS) {
        const Pixel pa = a.project_point(point);
        const Pixel pb = b.project_point(point);
        const Pixel_d sa = to_sensor(a.pixel_convention(), {pa.x, pa.y});
        const Pixel_d sb = to_sensor(b.pixel_convention(), {pb.x, pb.y});
        INFO("point (" << point.x << ", " << point.y << ", " << point.z << "): sensor (" << sa.x
                       << ", " << sa.y << ") vs (" << sb.x << ", " << sb.y << ")");
        CHECK(std::abs(sa.x - sb.x) < 1e-4);
        CHECK(std::abs(sa.y - sb.y) < 1e-4);
    }
}

} // namespace

TEST_CASE("The default pixel convention is OpenCV's", "[cameras][pixels]")
{
    CHECK(PixelConvention{} == PixelConvention::opencv());
    CameraModel<RGB> camera;
    CHECK(camera.pixel_convention() == PixelConvention::opencv());
}

TEST_CASE("Pixel conventions convert to and from sensor coordinates", "[cameras][pixels]")
{
    const Resolution resolution{W, H};

    // The center of the top-left pixel, (0.5, 0.5) on the sensor, in each convention:
    CHECK(PixelConvention::opencv().from_sensor({0.5f, 0.5f}, resolution).x == 0.f);
    CHECK(PixelConvention::opencv().from_sensor({0.5f, 0.5f}, resolution).y == 0.f);
    CHECK(PixelConvention::colmap().from_sensor({0.5f, 0.5f}, resolution).x == 0.5f);
    CHECK(PixelConvention::colmap().from_sensor({0.5f, 0.5f}, resolution).y == 0.5f);
    CHECK(PixelConvention::matlab().from_sensor({0.5f, 0.5f}, resolution).x == 1.f);
    CHECK(PixelConvention::matlab().from_sensor({0.5f, 0.5f}, resolution).y == 1.f);
    CHECK(PixelConvention::fits().from_sensor({0.5f, 0.5f}, resolution).x == 1.f);
    CHECK(PixelConvention::fits().from_sensor({0.5f, 0.5f}, resolution).y == H);

    // FITS counts rows up from the bottom: its first pixel is the bottom-left one.
    CHECK(PixelConvention::fits().to_sensor({1.f, 1.f}, resolution).y == H - 0.5f);

    for (const PixelConvention& c : PRESETS) {
        for (Pixel_d s : {Pixel_d{0.0, 0.0}, Pixel_d{12.3, 45.6}, Pixel_d{W, H}}) {
            INFO(name(c) << ", sensor (" << s.x << ", " << s.y << ")");
            const Pixel p =
                c.from_sensor({static_cast<float>(s.x), static_cast<float>(s.y)}, resolution);
            const Pixel_d expected = from_sensor(c, s);
            CHECK(std::abs(p.x - expected.x) < 1e-5);
            CHECK(std::abs(p.y - expected.y) < 1e-5);
            const Pixel back = c.to_sensor(p, resolution);
            CHECK(std::abs(back.x - s.x) < 1e-5);
            CHECK(std::abs(back.y - s.y) < 1e-5);
        }
    }
}

TEST_CASE("The default principal point is the sensor's center in every convention",
          "[cameras][pixels]")
{
    // The optical axis projects to the center of the sensor, numbered per convention.
    const std::array<Pixel_d, 4> centers{Pixel_d{(W - 1) / 2.0, (H - 1) / 2.0},
                                         Pixel_d{W / 2.0, H / 2.0},
                                         Pixel_d{(W + 1) / 2.0, (H + 1) / 2.0},
                                         Pixel_d{(W + 1) / 2.0, (H + 1) / 2.0}};
    auto reference = make_camera(PixelConvention::opencv());
    for (std::size_t i = 0; i < PRESETS.size(); ++i) {
        INFO(name(PRESETS[i]));
        auto camera = make_camera(PRESETS[i]);
        const Pixel axis = camera->project_point(Vec3<float>{0.f, 0.f, 1.f});
        CHECK(std::abs(axis.x - centers[i].x) < 1e-4);
        CHECK(std::abs(axis.y - centers[i].y) < 1e-4);

        // ...and so the convention changes no geometry at all:
        check_same_sensor_positions(*camera, *reference);
    }
}

TEST_CASE("y runs down in top-left conventions and up in FITS", "[cameras][pixels]")
{
    // A point above the optical axis (camera -y is up in the image).
    const Vec3<float> above{0.f, -1.f, 10.f};
    CHECK(make_camera(PixelConvention::opencv())->project_point(above).y < (H - 1) / 2.f);
    CHECK(make_camera(PixelConvention::fits())->project_point(above).y > (H + 1) / 2.f);

    // The ray through the center of the top-left pixel, named per convention, is one ray.
    const std::array<Pixel, 4> top_left{
        Pixel{0.f, 0.f}, Pixel{0.5f, 0.5f}, Pixel{1.f, 1.f}, Pixel{1.f, static_cast<float>(H)}};
    const Vec3<float> reference =
        make_camera(PixelConvention::opencv())->cast_ray(Pixel{0.f, 0.f}).direction();
    for (std::size_t i = 0; i < PRESETS.size(); ++i) {
        INFO(name(PRESETS[i]));
        const Vec3<float> d = make_camera(PRESETS[i])->cast_ray(top_left[i]).direction();
        CHECK(glm::dot(d, reference) > 1.f - 1e-7f);
    }
}

TEST_CASE("A principal point is read in the camera's convention", "[cameras][pixels]")
{
    // One place on the sensor - the center of the pixel in column 20, row 10 - named per
    // convention.
    const std::array<Pixel, 4> principal{
        Pixel{20.f, 10.f}, Pixel{20.5f, 10.5f}, Pixel{21.f, 11.f}, Pixel{21.f, H - 10.f}};

    auto reference = make_camera(PixelConvention::opencv());
    reference->configure_sensor_from_pitch(
        Resolution{W, H}, units::Micrometer(200.0), std::nullopt, 20.f, 10.f);

    for (std::size_t i = 0; i < PRESETS.size(); ++i) {
        INFO(name(PRESETS[i]));

        // configure_sensor_from_pitch(), with the convention set first:
        auto first = make_camera(PRESETS[i]);
        first->configure_sensor_from_pitch(Resolution{W, H},
                                           units::Micrometer(200.0),
                                           std::nullopt,
                                           principal[i].x,
                                           principal[i].y);
        check_same_sensor_positions(*first, *reference);

        // The optical axis lands exactly on the principal point as given:
        const Pixel axis = first->project_point(Vec3<float>{0.f, 0.f, 1.f});
        CHECK(std::abs(axis.x - principal[i].x) < 1e-4f);
        CHECK(std::abs(axis.y - principal[i].y) < 1e-4f);

        // set_intrinsics() (fx = 20 mm / 200 um), with the convention set last:
        CameraModel<RGB> last;
        last.set_intrinsics(100.f, 100.f, principal[i].x, principal[i].y, {W, H}, 20_mm);
        last.set_pixel_convention(PRESETS[i]);
        check_same_sensor_positions(last, *reference);
    }
}

TEST_CASE("Projection and ray casting are inverse in every convention", "[cameras][pixels]")
{
    for (const PixelConvention& convention : PRESETS) {
        for (bool blender : {false, true}) {
            INFO(name(convention) << ", Blender " << blender);
            auto camera = make_camera(convention);
            camera->set_brown_conrady_distortion(BrownCoefficients(-0.2, 0.05, 0.0, 0.0, 0.0));
            camera->use_blender_convention(blender);

            // Positions from just inside one corner of the sensor to just inside the opposite
            // one, in the camera's convention.
            for (Pixel_d s : {Pixel_d{0.1, 0.1}, Pixel_d{20.25, 30.75}, Pixel_d{63.9, 47.9}}) {
                const Pixel_d p = from_sensor(convention, s);
                const Pixel pixel{static_cast<float>(p.x), static_cast<float>(p.y)};
                const Pixel round_trip = camera->project_point(camera->cast_ray(pixel).direction());
                CHECK(std::abs(round_trip.x - pixel.x) < 0.01f);
                CHECK(std::abs(round_trip.y - pixel.y) < 0.01f);
            }
        }
    }
}

TEST_CASE("The pixel convention does not change what is rendered", "[cameras][pixels][render]")
{
    // Renders an unresolved source with the camera's principal point at the same place on the
    // sensor, named in each convention, and with the default one.
    auto render = [](const PixelConvention& convention, std::optional<Pixel> principal) {
        Scene<RGB> scene;
        auto camera_model = scene.new_camera_model();
        camera_model.set_pixel_convention(convention);
        camera_model.configure_sensor_from_pitch(
            {32, 32},
            10_um,
            std::nullopt,
            principal ? std::optional<float>(principal->x) : std::nullopt,
            principal ? std::optional<float>(principal->y) : std::nullopt);
        camera_model.set_focal_length(50_mm);
        camera_model.set_fstop(4.0f);
        camera_model.set_focus_sensor_offset(units::Micrometer(100.0)); // a small disk stamp
        auto camera = scene.root.new_instance(camera_model);
        auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
        source.set_position(0.83_m, -0.41_m, 1000_m);

        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        SceneView<RGB> scene_view(scene,
                                  Interval{Time::from_et(0.0), Time::from_et(0.001)},
                                  camera,
                                  ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);
        return frame_buffer.received_power();
    };

    auto same = [](const Image<RGB>& a, const Image<RGB>& b) {
        REQUIRE(a.size() == b.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a[i][0] != b[i][0]) {
                return false;
            }
        }
        return true;
    };

    const Image<RGB> reference = render(PixelConvention::opencv(), std::nullopt);
    const Image<RGB> shifted = render(PixelConvention::opencv(), Pixel{12.f, 18.f});
    REQUIRE_FALSE(same(reference, shifted)); // guard: the principal point does matter

    // Sensor position (12.5, 18.5), the center of column 12, row 18:
    const std::array<Pixel, 4> principal{
        Pixel{12.f, 18.f}, Pixel{12.5f, 18.5f}, Pixel{13.f, 19.f}, Pixel{13.f, 32.f - 18.f}};
    for (std::size_t i = 0; i < PRESETS.size(); ++i) {
        INFO(name(PRESETS[i]));
        CHECK(same(render(PRESETS[i], std::nullopt), reference));
        CHECK(same(render(PRESETS[i], principal[i]), shifted));
    }
}
