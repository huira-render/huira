#pragma once

// What tests of satellites placed by their TLEs share: element sets, an ephemeris of the Earth to
// place the TEME frame by, and where SGP4 and SPICE put a satellite.

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
#include "cspice/SpiceUsr.h"
#include "huira/huira.hpp"

namespace huira::test {

// The ISS, epoch 2008-09-20 12:25:40 UTC
inline constexpr std::string_view ISS_LINE1 =
    "1 25544U 98067A   08264.51782528 -.00002182  00000-0 -11606-4 0  2927";
inline constexpr std::string_view ISS_LINE2 =
    "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537";

// Another satellite: Vallado's case 29141, epoch 2006-06-19 06:25:41 UTC, which decays some seven
// hours later
inline constexpr std::string_view DECAYING_LINE1 =
    "1 29141U 85108AA  06170.26783845  .99999999  00000-0  13519-0 0   718";
inline constexpr std::string_view DECAYING_LINE2 =
    "2 29141  82.4288 273.4882 0015848 277.2124  83.9133 15.93343074  6828";

/// A TLE line with `text` written over it from `column` (counted from 1), and its checksum made to
/// match.
inline std::string edit(std::string_view line, std::size_t column, std::string_view text)
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

inline SatRec iss()
{
    return SatRec::from_tle(ISS_LINE1, ISS_LINE2);
}

/// The ISS's element set with its epoch a day later: a newer TLE, for the same satellite.
inline SatRec iss_a_day_later()
{
    return SatRec::from_tle(edit(ISS_LINE1, 19, "08265.51782528"), ISS_LINE2);
}

inline SatRec decaying()
{
    return SatRec::from_tle(DECAYING_LINE1, DECAYING_LINE2);
}

// The Earth's place in the test's ephemeris: moving in a straight line, through this position, at
// this velocity, at each segment's middle (relative to the solar system barycenter, J2000, km)
inline constexpr double EARTH_POSITION_KM[3] = {1.4e8, -5.0e7, 2.0e7};
inline constexpr double EARTH_VELOCITY_KMPS[3] = {10.0, 27.0, -3.0};

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

    TestEarthEphemeris(const TestEarthEphemeris&) = delete;
    TestEarthEphemeris& operator=(const TestEarthEphemeris&) = delete;

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

/// Where a TLE puts its satellite, worked out independently of the scene graph: SGP4's TEME state,
/// turned to J2000 by SPICE's own TEME-to-J2000 state transformation, plus the Earth's state.
inline std::pair<Vec3<double>, Vec3<double>> expected_state(const SatRec& satrec, const Time& time)
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

} // namespace huira::test
