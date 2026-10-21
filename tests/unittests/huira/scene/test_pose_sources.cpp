#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// A node's pose relative to the solar system barycenter at a time, where a scene view observing
/// without light time places it, with poses set by hand holding at `reference`.
template <typename THandle>
Transform<double> pose_at(const THandle& node, const Time& time, const Time& reference)
{
    return node.get()->get_apparent_transform(
        ObservationMode::TRUE_STATE, reference, time, Transform<double>{});
}

template <typename THandle>
Transform<double> pose_at(const THandle& node, const Time& time)
{
    return pose_at(node, time, time);
}

/// The largest difference between two rotations' matrices.
double rotation_difference(const Rotation<double>& a, const Rotation<double>& b)
{
    const Mat3<double> difference = a.local_to_parent_matrix() - b.local_to_parent_matrix();
    double largest = 0.0;
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            largest = std::max(largest, std::abs(difference[column][row]));
        }
    }
    return largest;
}

Time at(double et)
{
    return Time::from_et(et);
}

} // namespace

TEST_CASE("A position table is followed between its entries", "[scene][pose_sources]")
{
    Scene<RGB> scene;
    auto node = scene.root.new_subframe();

    // Entries unevenly spaced, from motion along a cubic: with its velocities, each piece
    // between two entries is that cubic, so it is followed exactly.
    const std::vector<double> ets{1000.0, 1000.7, 1002.0, 1002.4, 1004.0};
    const Vec3<double> a{1e3, -2e3, 5e2};
    const Vec3<double> b{30.0, 4.0, -7.0};
    const Vec3<double> c{-2.0, 0.5, 1.5};
    const Vec3<double> d{0.3, -0.2, 0.1};
    const auto cubic = [&](double et) {
        const double t = et - 1000.0;
        return a + b * t + c * (t * t) + d * (t * t * t);
    };
    const auto cubic_velocity = [&](double et) {
        const double t = et - 1000.0;
        return b + c * (2.0 * t) + d * (3.0 * t * t);
    };
    std::vector<Time> times;
    std::vector<Vec3<double>> positions;
    std::vector<Vec3<double>> velocities;
    for (double et : ets) {
        times.push_back(at(et));
        positions.push_back(cubic(et));
        velocities.push_back(cubic_velocity(et));
    }
    node.set_position_table(times, positions, velocities);
    for (double et : {1000.0, 1000.3, 1001.1, 1002.2, 1003.5, 1004.0}) {
        INFO("et " << et);
        const Transform<double> pose = pose_at(node, at(et));
        CHECK(glm::length(pose.position - cubic(et)) < 1e-9);
        CHECK(glm::length(pose.velocity - cubic_velocity(et)) < 1e-9);
    }

    // Without velocities, they are estimated at each entry from its neighbors, exactly for
    // motion along a quadratic:
    const auto quadratic = [&](double et) {
        const double t = et - 1000.0;
        return a + b * t + c * (t * t);
    };
    positions.clear();
    for (double et : ets) {
        positions.push_back(quadratic(et));
    }
    node.set_position_table(times, positions);
    for (double et : {1000.0, 1000.3, 1001.1, 1002.2, 1003.5, 1004.0}) {
        INFO("et " << et);
        const double t = et - 1000.0;
        const Transform<double> pose = pose_at(node, at(et));
        CHECK(glm::length(pose.position - quadratic(et)) < 1e-9);
        CHECK(glm::length(pose.velocity - (b + c * (2.0 * t))) < 1e-9);
    }

    // Two entries make a straight line:
    node.set_position_table({at(0.0), at(10.0)},
                            {Vec3<double>{0.0}, Vec3<double>{10.0, 20.0, 0.0}});
    CHECK(glm::length(pose_at(node, at(2.5)).position - Vec3<double>{2.5, 5.0, 0.0}) < 1e-12);
    CHECK(glm::length(pose_at(node, at(2.5)).velocity - Vec3<double>{1.0, 2.0, 0.0}) < 1e-12);

    // Nothing is extrapolated:
    CHECK_THROWS(pose_at(node, at(-0.001)));
    CHECK_THROWS(pose_at(node, at(10.001)));
}

TEST_CASE("A position table is checked when set", "[scene][pose_sources][validation]")
{
    Scene<RGB> scene;
    auto node = scene.root.new_subframe();
    const Vec3<double> p{1.0, 2.0, 3.0};
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS(node.set_position_table({at(0.0)}, {p}));                  // one entry
    CHECK_THROWS(node.set_position_table({at(1.0), at(0.0)}, {p, p}));      // backwards
    CHECK_THROWS(node.set_position_table({at(0.0), at(0.0)}, {p, p}));      // repeated
    CHECK_THROWS(node.set_position_table({at(0.0), at(1.0)}, {p}));         // sizes
    CHECK_THROWS(node.set_position_table({at(0.0), at(1.0)}, {p, p}, {p})); // sizes
    CHECK_THROWS(node.set_position_table({at(0.0), at(1.0)}, {p, Vec3<double>{NaN}}));
    CHECK_THROWS(node.set_position_table({at(0.0), at(1.0)}, {p, p}, {p, Vec3<double>{NaN}}));
    CHECK_THROWS(node.set_rotation_table({at(0.0)}, {Rotation<double>{}}));
    CHECK_THROWS(node.set_rotation_table({at(0.0), at(1.0)}, {Rotation<double>{}}));
    CHECK_THROWS(node.set_rotation_table(
        {at(0.0), at(1.0)}, {Rotation<double>{}, Rotation<double>{}}, {Vec3<double>{NaN}, p}));
    CHECK_NOTHROW(node.set_position_table({at(0.0), at(1.0)}, {p, p}));
}

TEST_CASE("A rotation table turns steadily between its entries", "[scene][pose_sources]")
{
    // A node spun by hand at a steady rate, and a table of its rotations one second apart: in
    // between, the table turns as the node does.
    Scene<RGB> scene;
    auto spun = scene.root.new_subframe();
    spun.set_rotation(Rotation<double>::extrinsic_euler_angles(0.3_rad, -0.2_rad, 1.1_rad, "XYZ"));
    const Vec3<double> omega = glm::normalize(Vec3<double>{1.0, 2.0, -3.0}) * 0.8; // rad/s
    spun.set_angular_velocity(units::RadiansPerSecond(omega.x),
                              units::RadiansPerSecond(omega.y),
                              units::RadiansPerSecond(omega.z));
    const Time reference = at(500.0);

    std::vector<Time> times;
    std::vector<Rotation<double>> rotations;
    for (int i = 0; i <= 5; ++i) {
        times.push_back(at(500.0 + i));
        rotations.push_back(pose_at(spun, times.back(), reference).rotation);
    }
    auto table = scene.root.new_subframe();
    table.set_rotation_table(times, rotations);
    for (double et : {500.0, 500.25, 501.5, 503.9, 505.0}) {
        INFO("et " << et);
        const Transform<double> expected = pose_at(spun, at(et), reference);
        const Transform<double> pose = pose_at(table, at(et));
        CHECK(rotation_difference(pose.rotation, expected.rotation) < 1e-12);
        CHECK(glm::length(pose.angular_velocity - omega) < 1e-12);
    }

    // Angular velocities, where given, are interpolated linearly:
    table.set_rotation_table({at(0.0), at(2.0)},
                             {Rotation<double>{}, Rotation<double>{}},
                             {Vec3<double>{0.0, 0.0, 1.0}, Vec3<double>{0.0, 0.0, 3.0}});
    CHECK(glm::length(pose_at(table, at(0.5)).angular_velocity - Vec3<double>{0.0, 0.0, 1.5}) <
          1e-12);

    CHECK_THROWS(pose_at(table, at(2.5)));
}

TEST_CASE("A function places a node, with rates estimated when not given", "[scene][pose_sources]")
{
    // A circular orbit, 7000 km in radius, at about 1.1e-3 rad/s, now (an ephemeris time of
    // about 8e8 s). The function works in time from an epoch, as an ephemeris would.
    Scene<RGB> scene;
    auto node = scene.root.new_subframe();
    const Time epoch = at(8.3e8);
    const double radius = 7.0e6;
    const double rate = 1.1e-3;
    const auto orbit = [=](const PoseContext& context) {
        const double t = (context.time - epoch).value();
        return Vec3<double>{radius * std::cos(rate * t), radius * std::sin(rate * t), 0.0};
    };
    const auto orbit_velocity = [=](const Time& time) {
        const double t = (time - epoch).value();
        return Vec3<double>{
            -radius * rate * std::sin(rate * t), radius * rate * std::cos(rate * t), 0.0};
    };
    node.set_position_function(orbit);
    const Time time = epoch + 1234.5_s;
    const Transform<double> estimated = pose_at(node, time);
    CHECK(estimated.position == orbit(PoseContext{time, time, ObservationMode::TRUE_STATE}));
    CHECK(glm::length(estimated.velocity - orbit_velocity(time)) < 1e-6 * radius * rate);

    // A velocity given is used as it is:
    node.set_position_function([&](const PoseContext& context) {
        return PositionState{orbit(context), Vec3<double>{1.0, 2.0, 3.0}};
    });
    CHECK(pose_at(node, time).velocity == Vec3<double>{1.0, 2.0, 3.0});

    // Rotations likewise: a steady spin about the parent's z axis, from a start.
    const Rotation<double> start =
        Rotation<double>::extrinsic_euler_angles(0.4_rad, 0.1_rad, -0.7_rad, "XYZ");
    const auto spin = [=](const PoseContext& context) {
        const double t = (context.time - epoch).value();
        return Rotation<double>::from_local_to_parent(Vec3<double>{0.0, 0.0, 1.0},
                                                      units::Radian(0.05 * t)) *
               start;
    };
    node.set_rotation_function(spin);
    CHECK(glm::length(pose_at(node, time).angular_velocity - Vec3<double>{0.0, 0.0, 0.05}) < 1e-9);
    node.set_rotation_function([&](const PoseContext& context) {
        return RotationState{spin(context), Vec3<double>{0.0, 0.0, 7.0}};
    });
    CHECK(pose_at(node, time).angular_velocity == Vec3<double>{0.0, 0.0, 7.0});

    // What the function gives is checked when it is called:
    node.set_position_function([](const PoseContext&) {
        return Vec3<double>{0.0, std::numeric_limits<double>::quiet_NaN(), 0.0};
    });
    CHECK_THROWS(pose_at(node, time));
}

TEST_CASE("A function is given the time to evaluate at, as the scene view places things",
          "[scene][pose_sources]")
{
    // Observed with light time, a body 3e8 m (a light second) away is evaluated a second early,
    // and a function placing it is told so, with the time poses set by hand hold at and the
    // observation mode.
    Scene<RGB> scene;
    auto node = scene.root.new_subframe();
    std::vector<PoseContext> seen;
    node.set_position_function([&seen](const PoseContext& context) {
        seen.push_back(context);
        return PositionState{Vec3<double>{0.0, 0.0, 3e8}, Vec3<double>{0.0}};
    });
    const Time time = at(100.0);
    const Time reference = at(99.5);
    node.get()->get_apparent_transform(
        ObservationMode::GEOMETRIC_STATE, reference, time, Transform<double>{});
    REQUIRE_FALSE(seen.empty());
    CHECK(std::abs((time - seen.back().time).value() - 3e8 / SPEED_OF_LIGHT<double>()) < 1e-9);
    for (const PoseContext& context : seen) {
        CHECK(context.reference == reference);
        CHECK(context.observation_mode == ObservationMode::GEOMETRIC_STATE);
    }
}

TEST_CASE("Each position or rotation source replaces the last", "[scene][pose_sources]")
{
    Scene<RGB> scene;
    auto node = scene.root.new_subframe();
    node.set_position_table({at(0.0), at(1.0)}, {Vec3<double>{0.0}, Vec3<double>{1.0}});
    CHECK(node.get()->get_position_mode() == TransformMode::POSITION_CALLBACK);
    CHECK_THROWS(node.set_velocity(1_mps, 0_mps, 0_mps)); // not set by hand
    node.set_position(1_m, 2_m, 3_m);
    CHECK(node.get()->get_position_mode() == TransformMode::MANUAL_TRANSFORM);
    CHECK_NOTHROW(node.set_velocity(1_mps, 0_mps, 0_mps));

    node.set_keplerian_orbit(7000_Km, 0.1, 0_rad, 0_rad, 0_rad, 0_rad, at(0.0), 398600.4418_Km3ps2);
    CHECK(std::abs(glm::length(pose_at(node, at(0.0)).position) - 6300e3) < 1e-3);

    node.set_rotation_function([](const PoseContext&) { return Rotation<double>{}; });
    CHECK(node.get()->get_rotation_mode() == TransformMode::ROTATION_CALLBACK);
    CHECK_THROWS(node.set_angular_velocity(
        units::RadiansPerSecond(1.0), units::RadiansPerSecond(0.0), units::RadiansPerSecond(0.0)));
    node.set_rotation(Rotation<double>{});
    CHECK(node.get()->get_rotation_mode() == TransformMode::MANUAL_TRANSFORM);

    // A node whose child is placed by SPICE is placed by SPICE too, as with set_position:
    auto earth = scene.root.new_subframe();
    earth.set_spice("EARTH", "IAU_EARTH");
    auto moon = earth.new_subframe();
    moon.set_spice("MOON", "IAU_MOON");
    const std::vector<Time> times{at(0.0), at(1.0)};
    CHECK_THROWS(earth.set_position_table(times, {Vec3<double>{0.0}, Vec3<double>{0.0}}));
    CHECK_THROWS(earth.set_position_function([](const PoseContext&) { return Vec3<double>{0.0}; }));
    CHECK_THROWS(earth.set_rotation_table(times, {Rotation<double>{}, Rotation<double>{}}));
    CHECK(earth.get()->get_position_mode() == TransformMode::SPICE_TRANSFORM);
    CHECK(earth.get()->get_rotation_mode() == TransformMode::SPICE_TRANSFORM);
}

TEST_CASE("A rotation set by hand is read back whatever places the node", "[scene][pose_sources]")
{
    // The getters of a rotation set by hand, and of a SPICE frame, check where the rotation comes
    // from, not where the position does.
    Scene<RGB> scene;
    auto node = scene.root.new_subframe();
    node.set_keplerian_orbit(7000_Km, 0.1, 0_rad, 0_rad, 0_rad, 0_rad, at(0.0), 398600.4418_Km3ps2);
    const Rotation<double> turned =
        Rotation<double>::from_local_to_parent(Vec3<double>{0.0, 0.0, 1.0}, units::Radian(0.5));
    node.set_rotation(turned);
    node.set_angular_velocity(
        units::RadiansPerSecond(0.1), units::RadiansPerSecond(0.0), units::RadiansPerSecond(0.0));
    CHECK(rotation_difference(node.get_static_rotation(), turned) == 0.0);
    CHECK(node.get_static_angular_velocity() == Vec3<double>{0.1, 0.0, 0.0});
    CHECK_THROWS(node.get_static_position());

    // And not when the rotation comes from elsewhere, wherever the position does:
    node.set_position(1_m, 2_m, 3_m);
    node.set_rotation_function([](const PoseContext&) { return Rotation<double>{}; });
    CHECK(node.get_static_position() == Vec3<double>{1.0, 2.0, 3.0});
    CHECK_THROWS(node.get_static_rotation());
    CHECK_THROWS(node.get_static_angular_velocity());

    auto framed = scene.root.new_subframe();
    framed.set_spice_frame("IAU_EARTH");
    CHECK(framed.get_spice_frame() == "IAU_EARTH");
    auto placed = scene.root.new_subframe();
    placed.set_spice_origin("EARTH");
    CHECK(placed.get_spice_origin() == "EARTH");
    CHECK_THROWS(placed.get_spice_frame());
}

TEST_CASE("A body placed by a table or a function is rendered where it is", "[scene][pose_sources]")
{
    // A ball 100 m in front of a camera about 40 degrees across, crossing 20 m (about 55 pixels)
    // in a 1 s exposure: moved by hand, or by a table of the same motion.
    const auto render = [](auto&& place) {
        Scene<RGB> scene;
        auto camera_model = scene.new_camera_model();
        camera_model.configure_sensor_from_size({200, 200}, 36_mm);
        camera_model.set_focal_length(50_mm);
        camera_model.enable_diffraction(false);
        camera_model.enable_depth_of_field(false);
        auto camera = scene.root.new_instance(camera_model);
        auto material = scene.new_material(scene.new_bsdf_lambertian());
        material.set_emissive(RGB{1.f});
        auto ball = scene.root.new_instance(
            scene.add_primitive(scene.add_ellipsoid(1_m, 1_m, 1_m), material));
        place(ball);
        SceneView<RGB> view(
            scene, Interval::from_start(at(10.0), 1_s), camera, ObservationMode::TRUE_STATE);
        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        renderer.set_samples_per_pixel(16);
        renderer.render(view, frame_buffer);
        return std::pair{frame_buffer.received_power(), view.max_temporal_samples()};
    };
    const auto centroid_x = [](const Image<RGB>& image) {
        double sum = 0.0;
        double moment = 0.0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const double power = static_cast<double>(image(x, y).total());
                sum += power;
                moment += power * static_cast<double>(x);
            }
        }
        return moment / sum;
    };

    const auto [by_hand, hand_samples] = render([](auto& ball) {
        ball.set_position(-10_m, 0_m, 100_m);
        ball.set_velocity(20_mps, 0_mps, 0_mps);
    });
    const auto [by_table, table_samples] = render([](auto& ball) {
        std::vector<Time> times;
        std::vector<Vec3<double>> positions;
        for (int i = 0; i <= 4; ++i) {
            times.push_back(at(9.0 + 0.75 * i));
            positions.push_back(Vec3<double>{-10.0 + 20.0 * (0.75 * i - 1.0), 0.0, 100.0});
        }
        ball.set_position_table(times, positions);
    });
    CHECK(hand_samples == 2);
    CHECK(table_samples == 2); // a straight line
    CHECK(std::abs(centroid_x(by_table) - centroid_x(by_hand)) < 0.01);
    CHECK(std::abs(centroid_x(by_hand) - 99.5) < 0.5);

    // A function moving it along a curve is followed with more samples:
    const auto [by_function, function_samples] = render([](auto& ball) {
        ball.set_position_function([](const PoseContext& context) {
            const double angle = 0.4 * (context.time.et() - 10.5);
            return Vec3<double>{25.0 * std::sin(angle), 25.0 * (1.0 - std::cos(angle)), 100.0};
        });
    });
    CHECK(function_samples > 2);
    CHECK(std::abs(centroid_x(by_function) - 99.5) < 0.5);
}
