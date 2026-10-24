#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;

namespace {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
constexpr double INF = std::numeric_limits<double>::infinity();
constexpr float NaN_F = std::numeric_limits<float>::quiet_NaN();
constexpr float INF_F = std::numeric_limits<float>::infinity();

/// Relative comparison: every value compared here is far from 0.
bool close(double a, double b, double tolerance = 1e-5)
{
    return std::abs(a - b) <= tolerance * std::abs(b);
}

/// Everything a failed setter might have changed, as seen from outside.
struct Snapshot {
    double focal_length;
    double fstop;
    int width;
    int height;
    Pixel a;
    Pixel b;
    double focus;

    bool operator==(const Snapshot& other) const
    {
        return focal_length == other.focal_length && fstop == other.fstop && width == other.width &&
               height == other.height && a.x == other.a.x && a.y == other.a.y && b.x == other.b.x &&
               b.y == other.b.y && focus == other.focus;
    }
};

Snapshot snapshot(const CameraModel<RGB>& camera)
{
    return {camera.focal_length().to_si(),
            camera.fstop(),
            camera.resolution().x,
            camera.resolution().y,
            camera.project_point(Vec3<float>{0.01f, 0.02f, 1.f}),
            camera.project_point(Vec3<float>{-0.03f, 0.01f, 1.f}),
            camera.focus_distance().to_si()};
}

/// Collects the warnings logged while it exists.
struct WarningLog {
    std::vector<std::string> messages;

    WarningLog()
    {
        Logger::set_custom_sink([this](const LogEntry& entry) {
            if (entry.level == LogLevel::Warning) {
                messages.push_back(entry.message);
            }
        });
    }
    ~WarningLog() { Logger::clear_custom_sink(); }

    WarningLog(const WarningLog&) = delete;
    WarningLog& operator=(const WarningLog&) = delete;
};

} // namespace

TEST_CASE("The intrinsic matrix is read as GLM lays it out", "[cameras][intrinsics]")
{
    const float fx = 1200.f;
    const float fy = 1100.f;
    const float cx = 30.5f;
    const float cy = 25.25f;
    const float s = 4.f;
    const Resolution resolution{64, 48};
    const units::Millimeter anchor(12.0);

    CameraModel<RGB> from_values;
    from_values.set_intrinsics(fx, fy, cx, cy, resolution, anchor, s);

    // GLM is column-major: its constructor takes a column at a time, and K[column][row].
    const Mat3<float> k{fx, 0.f, 0.f, s, fy, 0.f, cx, cy, 1.f};
    REQUIRE(k[2][0] == cx);
    REQUIRE(k[2][1] == cy);
    REQUIRE(k[1][0] == s);

    CameraModel<RGB> from_matrix;
    from_matrix.set_intrinsic_matrix(k, resolution, anchor);
    const Snapshot expected = snapshot(from_values);
    CHECK(snapshot(from_matrix) == expected);

    // A camera matrix is defined up to scale:
    CameraModel<RGB> from_scaled;
    from_scaled.set_intrinsic_matrix(k * 2.f, resolution, anchor);
    CHECK(snapshot(from_scaled) == expected);
    from_scaled.set_intrinsic_matrix(k * -0.5f, resolution, anchor);
    CHECK(snapshot(from_scaled) == expected);

    // A matrix that is not upper triangular is not a camera matrix. Most likely it was filled
    // row by row (so transposed), and it is rejected rather than misread, leaving the camera
    // as it was. So is one that cannot be scaled to a bottom-right entry of 1:
    CHECK_THROWS(from_matrix.set_intrinsic_matrix(glm::transpose(k), resolution, anchor));
    Mat3<float> lower = k;
    lower[0][1] = 3.f;
    CHECK_THROWS(from_matrix.set_intrinsic_matrix(lower, resolution, anchor));
    Mat3<float> projective = k;
    projective[1][2] = 1e-3f;
    CHECK_THROWS(from_matrix.set_intrinsic_matrix(projective, resolution, anchor));
    Mat3<float> unscalable = k;
    unscalable[2][2] = 0.f;
    CHECK_THROWS(from_matrix.set_intrinsic_matrix(unscalable, resolution, anchor));
    CHECK(snapshot(from_matrix) == expected);

    // Calibrations rarely give exact zeros:
    Mat3<float> nearly = k;
    nearly[0][1] = 1e-4f;
    nearly[0][2] = -1e-7f;
    CHECK_NOTHROW(from_matrix.set_intrinsic_matrix(nearly, resolution, anchor));
}

TEST_CASE("Skew shears the projection, in any pixel convention", "[cameras][intrinsics]")
{
    const float fx = 1200.f;
    const float fy = 1100.f;
    const float cx = 30.5f;
    const float cy = 25.25f;
    const float s = 40.f;
    const Resolution resolution{64, 48};
    const Vec3<float> point{0.004f, -0.006f, 1.f};

    for (const PixelConvention& convention :
         {PixelConvention::opencv(), PixelConvention::matlab(), PixelConvention::fits()}) {
        INFO(convention.to_string());
        CameraModel<RGB> camera;
        camera.set_pixel_convention(convention);
        camera.set_intrinsics(fx, fy, cx, cy, resolution, units::Millimeter(12.0), s);

        // The convention's own y runs up from the bottom row in FITS, so there the projection,
        // skew term included, sees y the other way up:
        const float y = convention.origin == PixelOrigin::TopLeft ? point.y : -point.y;
        const Pixel p = camera.project_point(point);
        CHECK(close(p.x, fx * point.x + s * y + cx, 1e-6));
        CHECK(close(p.y, fy * y + cy, 1e-6));

        // ...and cast_ray() inverts it:
        const Vec3<float> d = camera.cast_ray(p).direction();
        CHECK(close(d.x / d.z, point.x, 1e-4));
        CHECK(close(d.y / d.z, point.y, 1e-4));

        // The skew belongs to the pixel axes, so it scales with fx when the focal length
        // changes...
        camera.set_focal_length(units::Millimeter(24.0));
        const Pixel q = camera.project_point(point);
        CHECK(close(q.x, 2.f * (fx * point.x + s * y) + cx, 1e-5));

        // ...and configuring the sensor (rectangular pixels) removes it:
        camera.configure_sensor_from_pitch(resolution, units::Micrometer(10.0));
        const Pixel r = camera.project_point(Vec3<float>{0.f, 0.01f, 1.f});
        CHECK(close(r.x, camera.project_point(Vec3<float>{0.f, 0.f, 1.f}).x, 1e-6));
    }

    CameraModel<RGB> camera;
    CHECK_THROWS(camera.set_intrinsics(fx,
                                       fy,
                                       cx,
                                       cy,
                                       resolution,
                                       units::Millimeter(12.0),
                                       std::numeric_limits<float>::infinity()));
}

TEST_CASE("Rays can be cast through positions outside the image", "[cameras][intrinsics]")
{
    // With distortion, directions on the sensor come from a table, which ends at the sensor's
    // edge. Beyond it they are computed directly, rather than clamped to the edge:
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch(Resolution{32, 24}, units::Micrometer(10.0));
    camera.set_focal_length(units::Millimeter(5.0));
    camera.set_brown_conrady_distortion(BrownCoefficients{-0.05, 0, 0, 0, 0});

    for (const Pixel& p :
         {Pixel{-6.f, 10.f}, Pixel{40.f, -3.f}, Pixel{12.f, 30.f}, Pixel{5.f, 5.f}}) {
        INFO("(" << p.x << ", " << p.y << ")");
        const Pixel back = camera.project_point(camera.cast_ray(p).direction());
        CHECK(std::abs(back.x - p.x) < 1e-3);
        CHECK(std::abs(back.y - p.y) < 1e-3);
    }
}

TEST_CASE("Invalid settings throw and leave the camera as it was", "[cameras][validation]")
{
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch(Resolution{32, 24}, units::Micrometer(10.0));
    camera.set_focal_length(units::Millimeter(50.0));
    camera.set_fstop(4.f);
    camera.set_focus_distance(units::Meter(20.0));
    const Snapshot before = snapshot(camera);

    // Focal length:
    for (double f : {0.0, -50.0, NaN, INF}) {
        CHECK_THROWS(camera.set_focal_length(units::Millimeter(f)));
    }
    // ...including one that would put a sensor-offset focus at the lens:
    CameraModel<RGB> offset_camera;
    offset_camera.configure_sensor_from_pitch(Resolution{8, 8}, units::Micrometer(10.0));
    // Exactly minus the 10 mm focal length as stored, which puts the sensor at the lens:
    offset_camera.set_focus_sensor_offset(units::Meter(-static_cast<double>(0.01f)));
    const Snapshot offset_before = snapshot(offset_camera);
    CHECK_THROWS(offset_camera.set_focal_length(units::Millimeter(10.0)));
    CHECK_THROWS(offset_camera.set_intrinsics(
        1000.f, 1000.f, 3.5f, 3.5f, Resolution{8, 8}, units::Millimeter(10.0)));
    CHECK(snapshot(offset_camera) == offset_before);
    // ...or one whose f/2.8 aperture, the default, is too large or too small to hold:
    CameraModel<RGB> default_aperture;
    const Snapshot default_before = snapshot(default_aperture);
    for (double f : {1e23, 1e-30}) {
        CHECK_THROWS(default_aperture.set_focal_length(units::Millimeter(f)));
        CHECK_THROWS(default_aperture.set_intrinsics(static_cast<float>(f * 1e3),
                                                     static_cast<float>(f * 1e3),
                                                     3.5f,
                                                     3.5f,
                                                     Resolution{8, 8},
                                                     units::Millimeter(f)));
    }
    CHECK(snapshot(default_aperture) == default_before);

    // Aperture:
    for (float fstop : {0.f, -2.f, NaN_F, INF_F}) {
        CHECK_THROWS(camera.set_fstop(fstop));
    }
    for (double d : {0.0, -1.0, NaN, INF}) {
        CHECK_THROWS(camera.set_aperture_diameter(units::Millimeter(d)));
        CHECK_THROWS(camera.set_aperture<CircularAperture<RGB>>(units::Millimeter(d)));
    }

    // Sensor, with an otherwise valid change that must not be half made:
    CHECK_THROWS(camera.configure_sensor_from_pitch(Resolution{0, 24}, units::Micrometer(10.0)));
    CHECK_THROWS(camera.configure_sensor_from_pitch(Resolution{32, -1}, units::Micrometer(10.0)));
    CHECK_THROWS(camera.configure_sensor_from_pitch(Resolution{40, 30}, units::Micrometer(0.0)));
    CHECK_THROWS(camera.configure_sensor_from_pitch(Resolution{40, 30}, units::Micrometer(NaN)));
    CHECK_THROWS(camera.configure_sensor_from_pitch(
        Resolution{40, 30}, units::Micrometer(10.0), units::Micrometer(-5.0)));
    CHECK_THROWS(camera.configure_sensor_from_pitch(
        Resolution{40, 30}, units::Micrometer(5.0), std::nullopt, NaN_F, 2.f));
    CHECK_THROWS(camera.configure_sensor_from_size(Resolution{40, 30}, units::Millimeter(0.0)));
    CHECK_THROWS(camera.configure_sensor_from_size(Resolution{0, 30}, units::Millimeter(6.0)));
    // ...or with more pixels than an image holds, INT_MAX:
    CHECK_THROWS(
        camera.configure_sensor_from_pitch(Resolution{46341, 46341}, units::Micrometer(10.0)));
    CHECK_THROWS(camera.configure_sensor_from_size(
        Resolution{40, 30}, units::Millimeter(6.0), units::Millimeter(INF)));

    // Intrinsics:
    CHECK_THROWS(camera.set_intrinsics(
        0.f, 1000.f, 20.f, 15.f, Resolution{40, 30}, units::Millimeter(10.0)));
    CHECK_THROWS(camera.set_intrinsics(
        1000.f, NaN_F, 20.f, 15.f, Resolution{40, 30}, units::Millimeter(10.0)));
    CHECK_THROWS(camera.set_intrinsics(
        1000.f, 1000.f, INF_F, 15.f, Resolution{40, 30}, units::Millimeter(10.0)));
    CHECK_THROWS(camera.set_intrinsics(
        1000.f, 1000.f, 20.f, 15.f, Resolution{40, 0}, units::Millimeter(10.0)));
    CHECK_THROWS(camera.set_intrinsics(
        1000.f, 1000.f, 20.f, 15.f, Resolution{40, 30}, units::Millimeter(0.0)));

    // Distortion:
    CHECK_THROWS(camera.set_brown_conrady_distortion(BrownCoefficients{NaN, 0, 0, 0, 0}));
    CHECK_THROWS(camera.set_owen_distortion(OwenCoefficients{0, 0, 0, 0, 0, INF}));

    CHECK(snapshot(camera) == before);
}

TEST_CASE("Sensors and distortion models check their parameters", "[cameras][validation]")
{
    SimpleSensor<RGB> sensor;
    CHECK_THROWS(sensor.set_resolution(Resolution{0, 10}));
    CHECK_THROWS(sensor.set_pixel_pitch(units::Micrometer(INF), units::Micrometer(5.0)));
    CHECK_THROWS(sensor.set_bit_depth(0));
    CHECK_THROWS(sensor.set_bit_depth(25)); // beyond what the float response holds exactly
    CHECK_NOTHROW(sensor.set_bit_depth(24));
    CHECK_THROWS(sensor.set_roll(units::Radian(NaN)));
    CHECK_THROWS(sensor.set_gain_db(NaN_F));
    CHECK_THROWS(sensor.set_gain_db(1e6f)); // a conversion gain of 0
    CHECK(sensor.resolution().x == 1024);

    // A configuration is checked as the setters check it:
    SimpleSensorConfig<RGB> config;
    config.read_noise = -1.f;
    CHECK_THROWS(SimpleSensor<RGB>(config));
    config = SimpleSensorConfig<RGB>{};
    config.resolution = Resolution{0, 0};
    CHECK_THROWS(SimpleSensor<RGB>(config));
    CHECK_NOTHROW(SimpleSensor<RGB>(SimpleSensorConfig<RGB>{}));

    BrownDistortion<RGB> distortion(BrownCoefficients{0.1, 0, 0, 0, 0});
    CHECK_THROWS(distortion.set_tolerance(0.f));
    CHECK_THROWS(distortion.set_max_iterations(0));
    CHECK_THROWS(OpenCVDistortion<RGB>(OpenCVCoefficients{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NaN}));
}

TEST_CASE("Until the aperture is set the camera is f/2.8, and a set f-stop keeps its diameter",
          "[cameras][aperture]")
{
    WarningLog warnings;

    // The default follows the focal length:
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch(Resolution{8, 8}, units::Micrometer(10.0));
    CHECK(close(camera.fstop(), 2.8));
    camera.set_focal_length(units::Millimeter(100.0));
    CHECK(close(camera.fstop(), 2.8));
    CHECK(close(camera.aperture_diameter().to_si(), 0.1 / 2.8));

    // set_fstop() fixes the diameter, and the first focal length change after it warns:
    camera.set_fstop(8.f);
    CHECK(close(camera.aperture_diameter().to_si(), 0.0125));
    camera.set_focal_length(units::Millimeter(100.0)); // unchanged: nothing to warn about
    CHECK(warnings.messages.empty());
    camera.set_focal_length(units::Millimeter(50.0));
    CHECK(close(camera.fstop(), 4.0));
    CHECK(close(camera.aperture_diameter().to_si(), 0.0125));
    REQUIRE(warnings.messages.size() == 1);
    INFO(warnings.messages[0]);
    CHECK(warnings.messages[0].find("f/4") != std::string::npos);
    CHECK(warnings.messages[0].find("set_fstop(8)") != std::string::npos);

    camera.set_focal_length(units::Millimeter(25.0));
    CHECK(close(camera.fstop(), 2.0));
    CHECK(warnings.messages.size() == 1);

    // Setting it again re-arms the warning, which set_intrinsics() triggers too:
    camera.set_fstop(8.f);
    camera.set_intrinsics(5000.f, 5000.f, 3.5f, 3.5f, Resolution{8, 8}, units::Millimeter(50.0));
    CHECK(close(camera.fstop(), 16.0));
    CHECK(warnings.messages.size() == 2);

    // Focal length first, then f-stop, gives what was asked for:
    CameraModel<RGB> in_order;
    in_order.configure_sensor_from_pitch(Resolution{8, 8}, units::Micrometer(10.0));
    in_order.set_focal_length(units::Millimeter(100.0));
    in_order.set_fstop(8.f);
    CHECK(close(in_order.fstop(), 8.0));

    // A diameter, set either way, is kept without a warning:
    CameraModel<RGB> by_diameter;
    by_diameter.configure_sensor_from_pitch(Resolution{8, 8}, units::Micrometer(10.0));
    by_diameter.set_aperture_diameter(units::Millimeter(10.0));
    by_diameter.set_focal_length(units::Millimeter(100.0));
    CHECK(close(by_diameter.fstop(), 10.0));

    CameraModel<RGB> by_aperture;
    by_aperture.configure_sensor_from_pitch(Resolution{8, 8}, units::Micrometer(10.0));
    by_aperture.set_aperture<CircularAperture<RGB>>(units::Millimeter(5.0));
    by_aperture.set_focal_length(units::Millimeter(100.0));
    CHECK(close(by_aperture.fstop(), 20.0));

    CHECK(warnings.messages.size() == 2);
}

TEST_CASE("A pixel's radiance-to-power factor uses the direction to its center",
          "[cameras][radiometry]")
{
    // A wide field, so that the direction to a corner pixel's corner and to its center differ
    // by about 0.4 degrees:
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch(Resolution{64, 64}, units::Micrometer(100.0));
    camera.set_focal_length(units::Millimeter(5.0));

    // The solid angle of the spherical triangle between three unit vectors (Van Oosterom and
    // Strackee):
    auto triangle = [](const Vec3<double>& a, const Vec3<double>& b, const Vec3<double>& c) {
        const double numerator = std::abs(glm::dot(a, glm::cross(b, c)));
        const double denominator = 1.0 + glm::dot(a, b) + glm::dot(b, c) + glm::dot(c, a);
        return 2.0 * std::atan2(numerator, denominator);
    };
    auto direction = [&](float px, float py) {
        return glm::normalize(Vec3<double>(camera.cast_ray(Pixel{px, py}).direction()));
    };

    for (bool built : {false, true}) {
        if (built) {
            camera.precompute(); // the table, which gives the same values
        }
        for (const auto& [x, y] : {std::pair{0, 0}, {63, 0}, {17, 40}, {63, 63}}) {
            // Pixel (x, y) is centered at (x, y) in the default convention, its corners half a
            // pixel away:
            const float fx = static_cast<float>(x);
            const float fy = static_cast<float>(y);
            const Vec3<double> c00 = direction(fx - 0.5f, fy - 0.5f);
            const Vec3<double> c10 = direction(fx + 0.5f, fy - 0.5f);
            const Vec3<double> c11 = direction(fx + 0.5f, fy + 0.5f);
            const Vec3<double> c01 = direction(fx - 0.5f, fy + 0.5f);
            const double solid_angle = triangle(c00, c10, c11) + triangle(c00, c11, c01);
            const double expected =
                solid_angle * static_cast<double>(camera.projected_aperture_area(
                                  camera.cast_ray(x, y).direction()));
            INFO("pixel (" << x << ", " << y << "), table built: " << built);
            CHECK(close(camera.pixel_radiance_to_power(x, y), expected, 1e-4));
        }
    }
}

TEST_CASE("OpenCV distortion follows OpenCV's model and coefficient order", "[cameras][distortion]")
{
    // OpenCV's model, written out independently:
    auto opencv = [](const std::vector<double>& d, double x, double y) {
        const double k1 = d[0], k2 = d[1], p1 = d[2], p2 = d[3], k3 = d[4], k4 = d[5], k5 = d[6],
                     k6 = d[7], s1 = d[8], s2 = d[9], s3 = d[10], s4 = d[11];
        const double r2 = x * x + y * y;
        const double r4 = r2 * r2;
        const double r6 = r4 * r2;
        const double radial = (1 + k1 * r2 + k2 * r4 + k3 * r6) / (1 + k4 * r2 + k5 * r4 + k6 * r6);
        return Pixel_d{x * radial + 2 * p1 * x * y + p2 * (r2 + 2 * x * x) + s1 * r2 + s2 * r4,
                       y * radial + p1 * (r2 + 2 * y * y) + 2 * p2 * x * y + s3 * r2 + s4 * r4};
    };

    // No coefficients, no distortion:
    const OpenCVDistortion<RGB> none(OpenCVCoefficients{});
    CHECK(none.distort(Pixel{0.1f, 0.2f}).x == 0.1f);
    CHECK(none.distort(Pixel{0.1f, 0.2f}).y == 0.2f);
    CHECK(none.undistort(Pixel{0.1f, 0.2f}).x == 0.1f);

    // A full vector, in OpenCV's order:
    const std::vector<double> vector{
        -0.21, 0.05, 1e-3, -2e-3, -0.01, 0.02, -0.003, 0.001, 1e-3, -5e-4, 2e-3, 1e-4};
    const OpenCVCoefficients coefficients = OpenCVCoefficients::from_opencv(vector);
    CHECK(coefficients.p1 == vector[2]);
    CHECK(coefficients.k3 == vector[4]);
    CHECK(coefficients.s4 == vector[11]);

    const OpenCVDistortion<RGB> distortion(coefficients);
    for (const Pixel& p : {Pixel{0.1f, -0.2f}, Pixel{-0.3f, 0.15f}, Pixel{0.02f, 0.05f}}) {
        const Pixel_d expected = opencv(vector, p.x, p.y);
        const Pixel distorted = distortion.distort(p);
        CHECK(std::abs(distorted.x - expected.x) < 1e-6);
        CHECK(std::abs(distorted.y - expected.y) < 1e-6);
        const Pixel back = distortion.undistort(distorted);
        CHECK(std::abs(back.x - p.x) < 1e-5);
        CHECK(std::abs(back.y - p.y) < 1e-5);
    }

    // Shorter vectors leave the rest at 0, and other lengths and tilt are rejected:
    const OpenCVCoefficients five =
        OpenCVCoefficients::from_opencv(std::vector<double>{0.1, 0.2, 0.3, 0.4, 0.5});
    CHECK(five.k1 == 0.1);
    CHECK(five.p2 == 0.4);
    CHECK(five.k3 == 0.5);
    CHECK(five.k4 == 0.0);
    CHECK_THROWS(OpenCVCoefficients::from_opencv(std::vector<double>(6, 0.0)));
    std::vector<double> tilted(14, 0.0);
    CHECK_NOTHROW(OpenCVCoefficients::from_opencv(tilted));
    tilted[12] = 0.01;
    CHECK_THROWS(OpenCVCoefficients::from_opencv(tilted));
}

TEST_CASE("A new sensor keeps the intrinsics only if it has the same geometry",
          "[cameras][sensor][intrinsics]")
{
    // Calibrated intrinsics, with an off-center principal point and skew:
    auto calibrated = [] {
        auto camera = std::make_unique<CameraModel<RGB>>();
        camera->set_intrinsics(
            5000.f, 5100.f, 10.5f, 7.25f, Resolution{32, 24}, units::Millimeter(50.0), 3.f);
        return camera;
    };
    const Vec3<float> point{0.001f, 0.002f, 1.f};

    SECTION("A new noise model for the same sensor keeps them")
    {
        WarningLog warnings;
        auto camera = calibrated();
        const Pixel before = camera->project_point(point);

        SimpleSensorConfig<RGB> config;
        config.resolution = camera->resolution();
        config.pitch_x = units::Millimeter(50.0 / 5000.0);
        config.pitch_y = units::Millimeter(50.0 / 5100.0);
        config.read_noise = 3.f;
        camera->set_sensor<SimpleSensor<RGB>>(config);

        const Pixel after = camera->project_point(point);
        CHECK(close(after.x, before.x, 1e-6));
        CHECK(close(after.y, before.y, 1e-6));
        CHECK(warnings.messages.empty());
    }

    SECTION("A sensor of another size resets them, and says so")
    {
        // They used to be kept: the principal point and skew of the old sensor applied to the
        // new one, and fx and fy no longer matched the focal length and the new pitch.
        WarningLog warnings;
        auto camera = calibrated();

        SimpleSensorConfig<RGB> config;
        config.resolution = Resolution{64, 48};
        config.pitch_x = units::Micrometer(5.0);
        config.pitch_y = units::Micrometer(5.0);
        camera->set_sensor<SimpleSensor<RGB>>(config);

        // The optical axis lands at the center of the new sensor (pixel centers at integers):
        const Pixel axis = camera->project_point(Vec3<float>{0.f, 0.f, 1.f});
        CHECK(close(axis.x, 31.5, 1e-6));
        CHECK(close(axis.y, 23.5, 1e-6));

        // fx = fy = 50 mm / 5 um = 10000 px, and no skew:
        const Pixel p = camera->project_point(point);
        CHECK(close(p.x - axis.x, 10.0, 1e-5));
        CHECK(close(p.y - axis.y, 20.0, 1e-5));
        CHECK(close(camera->focal_length().to_si(), 0.05, 1e-6));

        REQUIRE(warnings.messages.size() == 1);
        INFO(warnings.messages[0]);
        CHECK(warnings.messages[0].find("principal point") != std::string::npos);
        CHECK(warnings.messages[0].find("64x48") != std::string::npos);
    }

    SECTION("Without intrinsics set, there is nothing to reset or warn about")
    {
        WarningLog warnings;
        CameraModel<RGB> camera;
        SimpleSensorConfig<RGB> config;
        config.resolution = Resolution{16, 16};
        camera.set_sensor<SimpleSensor<RGB>>(config);
        CHECK(warnings.messages.empty());
        CHECK(camera.resolution().x == 16);
    }
}

TEST_CASE("Harvey-Shack scatter rejects infinite parameters", "[cameras][validation][scatter]")
{
    // An infinite r0 made the profile flat, and an infinite exponent made it a single point;
    // both were accepted, despite the messages asking for finite values.
    CHECK_THROWS(HarveyShackScatter<RGB>(INF_F, 0.5f));
    CHECK_THROWS(HarveyShackScatter<RGB>(2.5f, INF_F));
    CHECK_NOTHROW(HarveyShackScatter<RGB>(2.5f, 0.5f));

    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch(Resolution{8, 8}, units::Micrometer(10.0));
    CHECK_THROWS(camera.set_scatter(0.1f, INF_F, units::Arcsecond(20.0)));
    CHECK_THROWS(camera.set_scatter(0.1f, 2.5f, units::Radian(INF_F)));
    CHECK_NOTHROW(camera.set_scatter(0.1f, 2.5f, units::Arcsecond(20.0)));
}

namespace {
/// An aperture of the user's own, which does not check its area.
template <IsSpectral TSpectral>
class UncheckedAperture : public Aperture<TSpectral> {
  public:
    explicit UncheckedAperture(double area) : area_{area} {}
    units::SquareMeter get_area() const override { return units::SquareMeter(area_); }
    void set_area(units::SquareMeter area) override { area_ = area.to_si(); }
    Vec2<float> sample(Sampler<float>&) const override { return {0.f, 0.f}; }
    units::Meter get_bounding_radius() const override
    {
        return units::Meter(std::sqrt(std::abs(area_) / 3.14159265358979323846));
    }
    void rasterize_shape(Image<float>&, float, float, float) const override {}

  private:
    double area_;
};
} // namespace

TEST_CASE("The optics' other inputs are checked when they are given", "[cameras][validation][psf]")
{
    // Before, each was taken, and spoiled the PSF or the readout later.
    CHECK_THROWS(AiryDisk<RGB>(units::Millimeter(25.0),
                               units::Micrometer(10.0),
                               units::Micrometer(10.0),
                               units::Millimeter(0.0),
                               8));
    CHECK_THROWS(AiryDisk<RGB>(units::Millimeter(-25.0),
                               units::Micrometer(10.0),
                               units::Micrometer(10.0),
                               units::Millimeter(5.0),
                               8));
    CHECK_THROWS(AiryDisk<RGB>(units::Millimeter(25.0),
                               units::Micrometer(INF),
                               units::Micrometer(10.0),
                               units::Millimeter(5.0),
                               8));
    CHECK_NOTHROW(AiryDisk<RGB>(units::Millimeter(25.0),
                                units::Micrometer(10.0),
                                units::Micrometer(10.0),
                                units::Millimeter(5.0),
                                8));
    // So small its square is 0 in single precision:
    CHECK_THROWS(HarveyShackScatter<RGB>(2.5f, 1e-30f));
    // A PSF can reach so far:
    HarveyShackScatter<RGB> scatter(2.5f, 0.5f);
    CHECK_THROWS(scatter.set_radius(PSF<RGB>::MAX_RADIUS + 1));
    CHECK_NOTHROW(scatter.set_radius(PSF<RGB>::MAX_RADIUS));

    // An aperture whose diameter is not finite, or of no area:
    CircularAperture<RGB> aperture(units::Millimeter(5.0));
    CHECK_THROWS(aperture.set_area(units::SquareMeter(3e38)));
    CameraModel<RGB> camera;
    camera.set_fstop(4.f);
    CHECK_THROWS(camera.set_aperture<UncheckedAperture<RGB>>(0.0));
    CHECK_THROWS(camera.set_aperture<UncheckedAperture<RGB>>(INF));
    CHECK(camera.fstop() == 4.f);

    // A coefficient that is infinite in single precision, which distortion is computed in:
    CHECK_THROWS(BrownDistortion<RGB>(BrownCoefficients{1e300, 0, 0, 0, 0}));

    // A bias at or above the largest value the bit depth holds reads out every pixel saturated:
    SimpleSensor<RGB> sensor;
    sensor.set_bit_depth(8);
    CHECK_THROWS(sensor.set_bias_level_dn(255.f));
    CHECK_NOTHROW(sensor.set_bias_level_dn(200.f));
    CHECK_THROWS(sensor.set_bit_depth(7));
    CHECK(sensor.bit_depth() == 8);
    // A configuration is checked whatever order its fields would be set in (a guard):
    SimpleSensorConfig<RGB> config;
    config.bit_depth = 16;
    config.bias_level_dn = 5000.f;
    CHECK_NOTHROW(SimpleSensor<RGB>(config));

    // An exposure that is negative or not finite:
    camera.configure_sensor_from_pitch(Resolution{8, 8}, units::Micrometer(10.0));
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_received_power();
    frame_buffer.enable_sensor_response();
    CHECK_THROWS(camera.readout(frame_buffer, units::Second(-1.0)));
    CHECK_THROWS(camera.readout(frame_buffer, units::Second(INF)));
    CHECK_NOTHROW(camera.readout(frame_buffer, units::Second(1.0)));
}

TEST_CASE("Turning diffraction off and on with a PSF set leaves the camera precomputed",
          "[cameras][psf]")
{
    // The set PSF is drawn either way. Before, it was rebuilt.
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch(Resolution{64, 64}, units::Micrometer(10.0));
    Image<RGB> data(9, 9, RGB{0.f});
    data(4, 4) = RGB{1.f};
    camera.set_measured_psf(data, 1.f, PSFSampling::PixelIntegrated);
    camera.precompute();
    camera.enable_diffraction(false);
    CHECK(camera.is_precomputed());
    camera.enable_diffraction(true);
    CHECK(camera.is_precomputed());
    // Cleared, diffraction is drawn again, which is built anew:
    camera.clear_psf();
    CHECK_FALSE(camera.is_precomputed());
}

TEST_CASE("Diffraction can be turned off and on, and a set PSF takes its place until cleared",
          "[cameras][psf]")
{
    Scene<RGB> scene;
    auto camera = scene.new_camera_model();
    camera.configure_sensor_from_pitch({64, 64}, units::Micrometer(10.0));
    const auto center = [&] {
        const auto pixel = camera.psf_image(4, 0.f, 0.f)(4, 4);
        return pixel[1];
    };
    const auto all_in_center = [&] { return std::abs(center() - 1.f) < 1e-6f; };
    const float diffraction = center();
    CHECK(diffraction < 1.f);

    // A set PSF, here all of the light in one pixel, takes diffraction's place whether it is on
    // or off:
    Image<RGB> data(9, 9, RGB{0.f});
    data(4, 4) = RGB{1.f};
    camera.set_measured_psf(data, 1.f, PSFSampling::PixelIntegrated);
    CHECK(camera.has_set_psf());
    CHECK(all_in_center());
    camera.enable_diffraction(false);
    CHECK(camera.has_set_psf());
    CHECK(all_in_center());

    // Cleared, it leaves no PSF while diffraction is off, and diffraction once it is on:
    camera.clear_psf();
    CHECK_FALSE(camera.has_set_psf());
    CHECK(all_in_center());
    camera.enable_diffraction(true);
    CHECK(center() == diffraction);
}
