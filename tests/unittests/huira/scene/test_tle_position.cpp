#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "cspice/SpiceUsr.h"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;
using Catch::Matchers::ContainsSubstring;

namespace {

// The ISS, epoch 2008-09-20 12:25:40 UTC
constexpr std::string_view ISS_LINE1 =
    "1 25544U 98067A   08264.51782528 -.00002182  00000-0 -11606-4 0  2927";
constexpr std::string_view ISS_LINE2 =
    "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537";

// Another satellite: Vallado's case 29141, epoch 2006-06-19 06:25:41 UTC, which decays some seven
// hours later
constexpr std::string_view DECAYING_LINE1 =
    "1 29141U 85108AA  06170.26783845  .99999999  00000-0  13519-0 0   718";
constexpr std::string_view DECAYING_LINE2 =
    "2 29141  82.4288 273.4882 0015848 277.2124  83.9133 15.93343074  6828";

/// A TLE line with `text` written over it from `column` (counted from 1), and its checksum made to
/// match.
std::string edit(std::string_view line, std::size_t column, std::string_view text)
{
    std::string edited(line);
    edited.replace(column - 1, text.size(), text);
    int sum = 0;
    for (std::size_t i = 0; i < 68; ++i) {
        if (edited[i] >= '0' && edited[i] <= '9') {
            sum += edited[i] - '0';
        } else if (edited[i] == '-') {
            sum += 1;
        }
    }
    edited[68] = static_cast<char>('0' + sum % 10);
    return edited;
}

SatRec iss()
{
    return SatRec::from_tle(ISS_LINE1, ISS_LINE2);
}

/// The ISS's element set with its epoch a day later: a newer TLE, for the same satellite.
SatRec iss_a_day_later()
{
    return SatRec::from_tle(edit(ISS_LINE1, 19, "08265.51782528"), ISS_LINE2);
}

SatRec decaying()
{
    return SatRec::from_tle(DECAYING_LINE1, DECAYING_LINE2);
}

/// A node's pose relative to the solar system barycenter, with the J2000 axes, at a time.
template <typename THandle>
Transform<double> pose_at(const THandle& node, const Time& time)
{
    return node.get()->get_apparent_transform(
        ObservationMode::TRUE_STATE, time, time, Transform<double>{});
}

// The Earth's place in the test's ephemeris: moving in a straight line, through this position, at
// this velocity, at each segment's middle (relative to the solar system barycenter, J2000, km)
constexpr double EARTH_POSITION_KM[3] = {1.4e8, -5.0e7, 2.0e7};
constexpr double EARTH_VELOCITY_KMPS[3] = {10.0, 27.0, -3.0};

/**
 * Writes and loads an SPK giving the Earth's position for 20 days about each element set's epoch,
 * as huira places the TEME frame by SPICE, at the Earth. It is unloaded and deleted at exit.
 */
class TestEarthEphemeris {
  public:
    static void load() { static const TestEarthEphemeris ephemeris; }

    /// The Earth's state at a time, from the same straight lines, in meters and meters per second.
    static std::pair<Vec3<double>, Vec3<double>> earth_at(const Time& time)
    {
        const double middle = nearest_middle_(time.et());
        const double dt = time.et() - middle;
        Vec3<double> position{};
        Vec3<double> velocity{};
        for (int i = 0; i < 3; ++i) {
            position[i] = (EARTH_POSITION_KM[i] + EARTH_VELOCITY_KMPS[i] * dt) * 1000.0;
            velocity[i] = EARTH_VELOCITY_KMPS[i] * 1000.0;
        }
        return {position, velocity};
    }

  private:
    static constexpr double HALF_SPAN = 10.0 * 86400.0; // s

    static const std::array<double, 2>& middles_()
    {
        static const std::array<double, 2> middles = {iss().epoch().et(), decaying().epoch().et()};
        return middles;
    }

    static double nearest_middle_(double et)
    {
        const std::array<double, 2>& middles = middles_();
        return std::abs(et - middles[0]) < std::abs(et - middles[1]) ? middles[0] : middles[1];
    }

    TestEarthEphemeris()
    {
        std::random_device random;
        path_ = std::filesystem::temp_directory_path() /
                ("huira_test_tle_earth_" + std::to_string(random()) + ".bsp");

        SpiceInt handle = 0;
        spice::call_spice(spkopn_c, path_.string().c_str(), "huira test", 0, &handle);
        for (const double middle : middles_()) {
            // Type 8: Lagrange interpolation of equally spaced states, here two, so linearly
            SpiceDouble states[2][6];
            for (int end = 0; end < 2; ++end) {
                const double dt = (end == 0 ? -HALF_SPAN : HALF_SPAN);
                for (int i = 0; i < 3; ++i) {
                    states[end][i] = EARTH_POSITION_KM[i] + EARTH_VELOCITY_KMPS[i] * dt;
                    states[end][i + 3] = EARTH_VELOCITY_KMPS[i];
                }
            }
            spice::call_spice(spkw08_c,
                              handle,
                              399,
                              0,
                              "J2000",
                              middle - HALF_SPAN,
                              middle + HALF_SPAN,
                              "EARTH",
                              1,
                              2,
                              states,
                              middle - HALF_SPAN,
                              2.0 * HALF_SPAN);
        }
        spice::call_spice(spkcls_c, handle);
        spice::furnsh(path_);
    }

    ~TestEarthEphemeris()
    {
        unload_c(path_.string().c_str());
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    std::filesystem::path path_;
};

/// Where a TLE puts its node, worked out independently of the scene graph: SGP4's TEME state,
/// turned to J2000 by SPICE's own TEME-to-J2000 state transformation, plus the Earth's state.
std::pair<Vec3<double>, Vec3<double>> expected_state(const SatRec& satrec, const Time& time)
{
    const std::optional<PositionState> teme = satrec.evaluate(time);
    REQUIRE(teme.has_value());

    SpiceDouble transform[6][6];
    spice::call_spice(sxform_c, "HUIRA_EARTH_TEME", "J2000", time.et(), transform);
    const double state[6] = {teme->position.x,
                             teme->position.y,
                             teme->position.z,
                             teme->velocity.x,
                             teme->velocity.y,
                             teme->velocity.z};
    auto [position, velocity] = TestEarthEphemeris::earth_at(time);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 6; ++column) {
            position[row] += transform[row][column] * state[column];
            velocity[row] += transform[row + 3][column] * state[column];
        }
    }
    return {position, velocity};
}

} // namespace

TEST_CASE("A node placed by a TLE is where SGP4, the TEME frame and the Earth put it",
          "[scene][tle]")
{
    Scene<RGB> scene;
    FrameHandle<RGB> satellite = scene.teme_frame().new_subframe();
    TestEarthEphemeris::load();

    const SatRec satrec = iss();
    satellite.set_tle_position(satrec);
    CHECK(satellite.get()->get_position_mode() == TransformMode::POSITION_CALLBACK);

    for (const double hours : {0.0, 1.0, -1.0, 24.0, -72.0}) {
        INFO("Hours from epoch: " << hours);
        const Time time = satrec.epoch() + units::Second(hours * 3600.0);
        const Transform<double> pose = pose_at(satellite, time);
        const auto [position, velocity] = expected_state(satrec, time);
        CHECK(glm::length(pose.position - position) < 1e-3); // m
        CHECK(glm::length(pose.velocity - velocity) < 1e-6); // m/s
    }

    // What it holds moves with it: a meter along its x axis, which, its rotation left alone, is
    // the TEME frame's
    const FrameHandle<RGB> payload = satellite.new_subframe();
    payload.set_position(1_m, 0_m, 0_m);
    const Time time = satrec.epoch() + units::Second(600.0);
    SpiceDouble teme_to_j2000[3][3];
    spice::call_spice(pxform_c, "HUIRA_EARTH_TEME", "J2000", time.et(), teme_to_j2000);
    const Vec3<double> x_axis{teme_to_j2000[0][0], teme_to_j2000[1][0], teme_to_j2000[2][0]};
    CHECK(glm::length(pose_at(payload, time).position - pose_at(satellite, time).position -
                      x_axis) < 1e-4); // Each position is some 1.5e11 m from the barycenter
}

TEST_CASE("Only a child of the scene's TEME frame is placed by a TLE", "[scene][tle]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> teme = scene.teme_frame();

    CHECK_THROWS_WITH(scene.root.new_subframe().set_tle_position(iss()),
                      ContainsSubstring("child of the scene's TEME frame (Scene::teme_frame())"));
    CHECK_THROWS_WITH(teme.new_subframe().new_subframe().set_tle_position(iss()),
                      ContainsSubstring("child of the scene's TEME frame"));
    CHECK_THROWS_WITH(teme.set_tle_position(iss()), ContainsSubstring("its pose is fixed"));
    CHECK_NOTHROW(teme.new_subframe().set_tle_position(iss()));
}

TEST_CASE("A satellite places one node", "[scene][tle]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> teme = scene.teme_frame();
    const FrameHandle<RGB> first = teme.new_subframe();
    const FrameHandle<RGB> second = teme.new_subframe();

    first.set_tle_position(iss());
    CHECK_THROWS_WITH(second.set_tle_position(iss()),
                      ContainsSubstring("cannot be placed by satellite 25544's TLE: " +
                                        first.get()->get_info() + " is"));
    CHECK_THROWS(second.set_tle_position(iss_a_day_later())); // The same satellite

    // The failed claim left the second node as it was, and other satellites are free
    CHECK(second.get()->get_position_mode() == TransformMode::MANUAL_TRANSFORM);
    CHECK_NOTHROW(second.set_tle_position(decaying()));

    // Each scene has satellites of its own
    Scene<RGB> other;
    CHECK_NOTHROW(other.teme_frame().new_subframe().set_tle_position(iss()));
}

TEST_CASE("A node lets its satellite go when it is deleted or placed another way", "[scene][tle]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> teme = scene.teme_frame();
    const FrameHandle<RGB> holder = teme.new_subframe();
    const FrameHandle<RGB> next = teme.new_subframe();

    SECTION("Deleted")
    {
        holder.set_tle_position(iss());
        teme.delete_subframe(holder);
        CHECK_NOTHROW(next.set_tle_position(iss()));
    }

    SECTION("Deleted with the TEME frame")
    {
        holder.set_tle_position(iss());
        scene.root.delete_subframe(teme);
        CHECK_NOTHROW(scene.teme_frame().new_subframe().set_tle_position(iss()));
    }

    SECTION("Given a position by hand")
    {
        holder.set_tle_position(iss());
        holder.set_position(units::Meter(7e6), 0_m, 0_m);
        CHECK_NOTHROW(next.set_tle_position(iss()));
    }

    SECTION("Given another position source")
    {
        holder.set_tle_position(iss());
        holder.set_position_function([](const PoseContext&) { return Vec3<double>{7e6, 0, 0}; });
        CHECK_NOTHROW(next.set_tle_position(iss()));
    }

    SECTION("Placed by another satellite")
    {
        holder.set_tle_position(iss());
        holder.set_tle_position(decaying());
        CHECK_NOTHROW(next.set_tle_position(iss()));
        CHECK_THROWS(next.set_tle_position(decaying()));
    }
}

TEST_CASE("A newer element set for a node's satellite replaces the old one", "[scene][tle]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> satellite = scene.teme_frame().new_subframe();
    TestEarthEphemeris::load();

    satellite.set_tle_position(iss());
    const SatRec newer = iss_a_day_later();
    satellite.set_tle_position(newer);

    const Time time = newer.epoch() + units::Second(1800.0);
    CHECK(glm::length(pose_at(satellite, time).position - expected_state(newer, time).first) <
          1e-3);
    CHECK(glm::length(pose_at(satellite, time).position - expected_state(iss(), time).first) >
          1000.0); // The old element set puts it elsewhere
}

TEST_CASE("A node placed by a TLE can't be evaluated where SGP4 can't place it", "[scene][tle]")
{
    Scene<RGB> scene;
    const FrameHandle<RGB> satellite = scene.teme_frame().new_subframe();
    TestEarthEphemeris::load();

    const SatRec satrec = decaying();
    satellite.set_tle_position(satrec);
    CHECK_NOTHROW(pose_at(satellite, satrec.epoch()));
    CHECK_NOTHROW(pose_at(satellite, satrec.epoch() + units::Second(6.0 * 3600.0)));

    // Decayed, eight hours from its epoch
    const Time later = satrec.epoch() + units::Second(8.0 * 3600.0);
    CHECK_THROWS_WITH(pose_at(satellite, later),
                      ContainsSubstring("SGP4 can't place satellite 29141") &&
                          ContainsSubstring("0.3 days from its element set's epoch") &&
                          ContainsSubstring("SGP4 error 6"));
}
