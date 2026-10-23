#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "huira/ephemeris/satrec.hpp"
#include "third_party/sgp4_verification.hpp"

using namespace huira;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

constexpr std::string_view ISS_LINE1 =
    "1 25544U 98067A   08264.51782528 -.00002182  00000-0 -11606-4 0  2927";
constexpr std::string_view ISS_LINE2 =
    "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537";

/// A TLE line, cut to its 69 columns, with its checksum (column 69) made to match the rest.
std::string with_checksum(std::string line)
{
    line.resize(69);
    int sum = 0;
    for (std::size_t i = 0; i < 68; ++i) {
        if (line[i] >= '0' && line[i] <= '9') {
            sum += line[i] - '0';
        } else if (line[i] == '-') {
            sum += 1;
        }
    }
    line[68] = static_cast<char>('0' + sum % 10);
    return line;
}

/// A TLE line with `text` written over it from `column` (counted from 1, as the format counts
/// them), and its checksum made to match.
std::string edit(std::string_view line, std::size_t column, std::string_view text)
{
    std::string edited(line);
    edited.replace(column - 1, text.size(), text);
    return with_checksum(edited);
}

/// The state the vendored SGP4 gives, used directly, in meters and meters per second.
std::pair<Vec3<double>, Vec3<double>>
vendored_state(std::string_view line1, std::string_view line2, char opsmode, double minutes)
{
    namespace vallado = detail::vallado;
    char buffer1[130] = {};
    char buffer2[130] = {};
    line1.copy(buffer1, 69);
    line2.copy(buffer2, 69);
    double start = 0.0;
    double stop = 0.0;
    double step = 0.0;
    vallado::elsetrec record{};
    vallado::SGP4Funcs::twoline2rv(
        buffer1, buffer2, 'c', 'e', opsmode, vallado::wgs72, start, stop, step, record);
    double r[3] = {};
    double v[3] = {};
    REQUIRE(vallado::SGP4Funcs::sgp4(record, minutes, r, v));
    return {Vec3<double>{r[0], r[1], r[2]} * 1000.0, Vec3<double>{v[0], v[1], v[2]} * 1000.0};
}

/// The largest difference between a vector's components and a state's from tcppver.out, which are
/// in kilometers (or kilometers per second).
double largest_difference(const Vec3<double>& meters, const std::array<double, 3>& kilometers)
{
    double largest = 0.0;
    for (int i = 0; i < 3; ++i) {
        largest = std::fmax(
            largest, std::fabs(meters[i] - kilometers[static_cast<std::size_t>(i)] * 1000.0));
    }
    return largest;
}

/// The time a state in tcppver.out is at.
Time at_minutes(const SatRec& satrec, double minutes)
{
    return satrec.epoch() + units::Second(minutes * 60.0);
}

/// 33333, 33334 and 33335 are Vallado's cases of element sets SGP4 should stop on. He made them
/// by editing others, and left their checksums as they were.
bool has_stale_checksums(const test::VerificationCase& verification_case)
{
    return verification_case.satnum == "33333" || verification_case.satnum == "33334" ||
           verification_case.satnum == "33335";
}

/// A verification case's element set, with its checksums made to match it if they are stale.
SatRec read_case(const test::VerificationCase& verification_case, Sgp4Mode mode)
{
    std::string line1 = verification_case.line1.substr(0, 69);
    std::string line2 = verification_case.line2.substr(0, 69);
    if (has_stale_checksums(verification_case)) {
        line1 = with_checksum(line1);
        line2 = with_checksum(line2);
    }
    return SatRec::from_tle(line1, line2, mode);
}

const test::VerificationCase& find_case(const std::vector<test::VerificationCase>& cases,
                                        std::string_view satnum)
{
    const auto found =
        std::find_if(cases.begin(), cases.end(), [&](const test::VerificationCase& c) {
            return c.satnum == satnum;
        });
    REQUIRE(found != cases.end());
    return *found;
}

// tcppver.out prints positions to 1e-8 km and velocities to 1e-9 km/s. Through SatRec, the time
// since epoch also carries the rounding of ephemeris time, well under a millimeter of motion.
constexpr double position_tolerance = 0.01; // m
constexpr double velocity_tolerance = 1e-5; // m/s

} // namespace

TEST_CASE("SatRec: Reproduces Vallado's verification output", "[satrec]")
{
    // Vallado made tcppver.out in AFSPC mode, the default
    const std::vector<test::VerificationCase> cases = test::load_verification_cases();
    REQUIRE_FALSE(cases.empty());

    std::size_t states = 0;
    for (const test::VerificationCase& verification_case : cases) {
        INFO("Satellite " << verification_case.satnum);
        if (has_stale_checksums(verification_case)) {
            CHECK_THROWS_WITH(SatRec::from_tle(verification_case.line1.substr(0, 69),
                                               verification_case.line2.substr(0, 69)),
                              ContainsSubstring("checksum"));
        }

        // 33334 is Vallado's case of an element set SGP4 can't propagate even at its epoch: its
        // perturbed eccentricity is out of range. tcppver.out prints a state for it anyway.
        if (verification_case.satnum == "33334") {
            CHECK_THROWS_WITH(read_case(verification_case, Sgp4Mode::AFSPC),
                              ContainsSubstring("SGP4 error 3"));
            continue;
        }

        const SatRec satrec = read_case(verification_case, Sgp4Mode::AFSPC);
        CHECK(satrec.catalog_number() == std::stoi(verification_case.satnum));
        for (const test::VerificationCase::State& state : verification_case.states) {
            INFO("Minutes from epoch: " << state.tsince);
            const std::optional<PositionState> result =
                satrec.evaluate(at_minutes(satrec, state.tsince));
            REQUIRE(result.has_value());
            CHECK(largest_difference(result->position, state.r) < position_tolerance);
            CHECK(largest_difference(result->velocity, state.v) < velocity_tolerance);
            ++states;
        }
    }
    CHECK(states > 600);
}

TEST_CASE("SatRec: Gives nothing where Vallado's output stops, and says why", "[satrec]")
{
    // Vallado's program stops a run at SGP4's first error. Each of these runs stops before the
    // end it was given: at the next step after the last state printed for it, SGP4 has an error.
    struct Stop {
        std::string satnum;
        std::string error;
    };
    const std::vector<Stop> stops = {
        {"22312", "SGP4 error 1"}, // Its mean eccentricity grows out of range
        {"28350", "SGP4 error 1"},
        {"28872", "SGP4 error 6"}, // It decays
        {"29141", "SGP4 error 6"},
        {"33333", "SGP4 error 4"}, // Its semi-latus rectum turns negative
    };

    const std::vector<test::VerificationCase> cases = test::load_verification_cases();
    for (const Stop& stop : stops) {
        INFO("Satellite " << stop.satnum);
        const test::VerificationCase& verification_case = find_case(cases, stop.satnum);
        const SatRec satrec = read_case(verification_case, Sgp4Mode::AFSPC);

        // The run's start, stop and step, after the second line's 69 columns
        double run_start = 0.0;
        double run_stop = 0.0;
        double run_step = 0.0;
        std::istringstream run(verification_case.line2.substr(69));
        REQUIRE(run >> run_start >> run_stop >> run_step);

        REQUIRE_FALSE(verification_case.states.empty());
        const double last = verification_case.states.back().tsince;
        const double next = std::fmin(last + run_step, run_stop);
        REQUIRE(last < run_stop);

        CHECK(satrec.evaluate(at_minutes(satrec, last)).has_value());
        CHECK(satrec.error_message_at(at_minutes(satrec, last)).empty());

        CHECK_FALSE(satrec.evaluate(at_minutes(satrec, next)).has_value());
        CHECK_THAT(satrec.error_message_at(at_minutes(satrec, next)),
                   ContainsSubstring(stop.error));
    }
}

TEST_CASE("SatRec: Improved mode departs from AFSPC mode only for 23599", "[satrec]")
{
    // The modes differ only for deep-space orbits whose inclination, as the Sun and Moon perturb
    // it, is under 11.46 degrees. Of Vallado's cases, that is 23599, where they are about a
    // kilometer apart within hours. 20413's long run, 3.5 years from its epoch, dips under it too,
    // but there they differ only by rounding. Every other case is propagated identically.
    const std::vector<test::VerificationCase> cases = test::load_verification_cases();

    double largest_23599_difference = 0.0;
    double largest_other_difference = 0.0;
    for (const test::VerificationCase& verification_case : cases) {
        if (verification_case.satnum == "33334") {
            continue; // Can't be propagated
        }
        INFO("Satellite " << verification_case.satnum);
        const SatRec afspc = read_case(verification_case, Sgp4Mode::AFSPC);
        const SatRec improved = read_case(verification_case, Sgp4Mode::IMPROVED);
        CHECK(afspc.mode() == Sgp4Mode::AFSPC);
        CHECK(improved.mode() == Sgp4Mode::IMPROVED);
        CHECK(afspc.epoch() == improved.epoch());

        for (const test::VerificationCase::State& state : verification_case.states) {
            INFO("Minutes from epoch: " << state.tsince);
            const Time time = at_minutes(afspc, state.tsince);
            const std::optional<PositionState> a = afspc.evaluate(time);
            const std::optional<PositionState> i = improved.evaluate(time);
            REQUIRE(a.has_value());
            REQUIRE(i.has_value());
            const double difference = glm::length(a->position - i->position);
            if (verification_case.satnum == "23599") {
                largest_23599_difference = std::fmax(largest_23599_difference, difference);
            } else if (verification_case.satnum == "20413") {
                largest_other_difference = std::fmax(largest_other_difference, difference);
            } else {
                CHECK(a->position == i->position);
                CHECK(a->velocity == i->velocity);
            }
        }
    }
    CHECK(largest_23599_difference > 100.0); // m
    CHECK(largest_other_difference < 1e-5);  // m
}

TEST_CASE("SatRec: Reads element sets as the vendored SGP4's reader does, to the bit", "[satrec]")
{
    // SatRec reads a TLE itself, then initializes it with the vendored sgp4init(), as the vendored
    // twoline2rv() does once it has read it with sscanf(). They should give the same states.
    const std::vector<test::VerificationCase> cases = test::load_verification_cases();
    std::vector<std::pair<std::string, std::string>> element_sets;
    for (const test::VerificationCase& verification_case : cases) {
        if (verification_case.satnum != "33334") { // Can't be propagated
            std::string line1 = verification_case.line1.substr(0, 69);
            std::string line2 = verification_case.line2.substr(0, 69);
            if (has_stale_checksums(verification_case)) {
                line1 = with_checksum(line1);
                line2 = with_checksum(line2);
            }
            element_sets.emplace_back(line1, line2);
        }
    }

    // The fields with signs, and the ones twoline2rv() reads spaces in as zeros, written each
    // way they can be
    const std::string iss1(ISS_LINE1);
    const std::string iss2(ISS_LINE2);
    element_sets.emplace_back(edit(iss1, 34, "+"), iss2);        // The mean motion's rate
    element_sets.emplace_back(edit(iss1, 45, "+12345+3"), iss2); // Its second derivative
    element_sets.emplace_back(edit(iss1, 45, "-12345 3"), iss2);
    element_sets.emplace_back(edit(iss1, 45, "        "), iss2);
    element_sets.emplace_back(edit(iss1, 45, " 1 3 5- "), iss2);
    element_sets.emplace_back(edit(iss1, 54, "+11606+1"), iss2); // B*
    element_sets.emplace_back(edit(iss1, 54, " 11606 0"), iss2);
    element_sets.emplace_back(edit(iss1, 63, " "), iss2);                // The ephemeris type
    element_sets.emplace_back(iss1, edit(iss2, 9, "  1.0000 360.0000")); // Angles
    element_sets.emplace_back(iss1, edit(iss2, 53, " 1.00270000"));      // The mean motion

    for (const auto& [line1, line2] : element_sets) {
        INFO("Line 1: " << line1);
        INFO("Line 2: " << line2);
        for (const Sgp4Mode mode : {Sgp4Mode::AFSPC, Sgp4Mode::IMPROVED}) {
            const SatRec satrec = SatRec::from_tle(line1, line2, mode);
            const char opsmode = mode == Sgp4Mode::AFSPC ? 'a' : 'i';
            for (const double minutes : {0.0, 360.0, -360.0, 1440.0, 4320.0}) {
                INFO("Minutes from epoch: " << minutes);
                const std::optional<PositionState> state =
                    satrec.evaluate(at_minutes(satrec, minutes));
                if (!state.has_value()) {
                    continue; // Some of Vallado's cases decay first
                }
                // The time since epoch, exactly as SatRec gives it to SGP4
                const double since =
                    (at_minutes(satrec, minutes).et() - satrec.epoch().et()) / 60.0;
                const auto [position, velocity] = vendored_state(line1, line2, opsmode, since);
                CHECK(state->position == position);
                CHECK(state->velocity == velocity);
            }
        }
    }
}

TEST_CASE("SatRec: Reads the epoch", "[satrec]")
{
    SECTION("To well under a millisecond")
    {
        // Day 264.51782528 of 2008
        const SatRec iss = SatRec::from_tle(ISS_LINE1, ISS_LINE2);
        CHECK_THAT(iss.epoch().et(), WithinAbs(Time("2008-09-20 12:25:40.104192 UTC").et(), 1e-6));
    }

    SECTION("With two-digit years from 1957 to 2056")
    {
        const SatRec in_1957 =
            SatRec::from_tle(edit(ISS_LINE1, 19, "57275.00000000"), std::string(ISS_LINE2));
        CHECK_THAT(in_1957.epoch().et(), WithinAbs(Time("1957-10-02 00:00:00 UTC").et(), 1e-6));

        const SatRec in_2056 =
            SatRec::from_tle(edit(ISS_LINE1, 19, "56001.50000000"), std::string(ISS_LINE2));
        CHECK_THAT(in_2056.epoch().et(), WithinAbs(Time("2056-01-01 12:00:00 UTC").et(), 1e-6));
    }
}

TEST_CASE("SatRec: Counts leap seconds in the time since epoch", "[satrec]")
{
    // A day of UTC from noon on 2016-12-31 to noon on 2017-01-01 has 86401 seconds: a leap
    // second ends 2016. SGP4 is given the time that has elapsed.
    const std::string line1 = edit(ISS_LINE1, 19, "16366.50000000");
    const SatRec satrec = SatRec::from_tle(line1, ISS_LINE2);
    CHECK_THAT(satrec.epoch().et(), WithinAbs(Time("2016-12-31 12:00:00 UTC").et(), 1e-6));

    const std::optional<PositionState> state = satrec.evaluate(Time("2017-01-01 12:00:00 UTC"));
    REQUIRE(state.has_value());

    // Ephemeris time (TDB) also runs a little differently from the seconds of UTC, by about 30
    // microseconds a day at most: some 20 cm of motion
    const Vec3<double> elapsed = vendored_state(line1, ISS_LINE2, 'a', 86401.0 / 60.0).first;
    CHECK(glm::length(state->position - elapsed) < 1.0); // m

    // A second of motion away from the UTC day that python-sgp4 would propagate over
    const Vec3<double> utc_day = vendored_state(line1, ISS_LINE2, 'a', 1440.0).first;
    CHECK(glm::length(state->position - utc_day) > 5000.0); // m
}

TEST_CASE("SatRec: Rejects lines that aren't a TLE's", "[satrec]")
{
    const std::string line1(ISS_LINE1);
    const std::string line2(ISS_LINE2);

    SECTION("A line that isn't 69 characters long")
    {
        CHECK_THROWS_WITH(SatRec::from_tle(line1.substr(0, 68), line2),
                          ContainsSubstring("Line 1 has 68 characters"));
        CHECK_THROWS_WITH(SatRec::from_tle(line1, line2 + "0"),
                          ContainsSubstring("Line 2 has 70 characters"));
        CHECK_THROWS_WITH(SatRec::from_tle("", line2), ContainsSubstring("Line 1 has 0"));
    }

    SECTION("Lines in the wrong order")
    {
        CHECK_THROWS_WITH(SatRec::from_tle(line2, line1),
                          ContainsSubstring("Line 1, column 1, should be '1', not '2'"));
    }

    SECTION("A column out of place")
    {
        // The epoch's decimal point
        CHECK_THROWS_WITH(SatRec::from_tle(edit(line1, 24, "5"), line2),
                          ContainsSubstring("Line 1, column 24, should be '.', not '5'"));

        // A field shifted by a column
        CHECK_THROWS_WITH(
            SatRec::from_tle(line1, with_checksum(line2.substr(0, 8) + line2.substr(9) + "0")),
            ContainsSubstring("Line 2, column"));
    }

    SECTION("A character a number can't have")
    {
        CHECK_THROWS_WITH(SatRec::from_tle(line1, edit(line2, 10, "x")),
                          ContainsSubstring("Line 2, column 10, should be a digit"));

        // A space after a number's first digit: the mean motion, 15, as "1 "
        CHECK_THROWS_WITH(SatRec::from_tle(line1, edit(line2, 54, " ")),
                          ContainsSubstring("Line 2, column 54, should be a digit, or a space "
                                            "before the number's first digit"));
    }

    SECTION("A checksum that doesn't match")
    {
        std::string wrong = line1;
        wrong[68] = '8';
        CHECK_THROWS_WITH(
            SatRec::from_tle(wrong, line2),
            ContainsSubstring("Line 1's checksum is 8, but its other columns give 7"));
    }

    SECTION("Lines for different satellites")
    {
        CHECK_THROWS_WITH(SatRec::from_tle(line1, edit(line2, 3, "25545")),
                          ContainsSubstring("different satellites"));
    }

    SECTION("An SGP4-XP element set")
    {
        CHECK_THROWS_WITH(SatRec::from_tle(edit(line1, 63, "4"), line2),
                          ContainsSubstring("SGP4-XP"));
    }

    SECTION("Elements SGP4 would propagate garbage from")
    {
        CHECK_THROWS_WITH(SatRec::from_tle(edit(line1, 21, "000"), line2),
                          ContainsSubstring("The epoch is on day 0"));
        CHECK_THROWS_WITH(SatRec::from_tle(line1, edit(line2, 9, "180.0001")),
                          ContainsSubstring("The inclination, 180.0001 degrees, is more than 180"));
        CHECK_THROWS_WITH(SatRec::from_tle(line1, edit(line2, 53, " 0.00000000")),
                          ContainsSubstring("The mean motion is 0"));
    }

    SECTION("Naming the satellite as far as it is known")
    {
        CHECK_THROWS_WITH(SatRec::from_tle("ISS (ZARYA)", line1.substr(0, 68), line2),
                          ContainsSubstring("\"ISS (ZARYA)\": Line 1 has 68"));
        CHECK_THROWS_WITH(SatRec::from_tle("ISS (ZARYA)", edit(line1, 63, "4"), line2),
                          ContainsSubstring("Satellite 25544, \"ISS (ZARYA)\": The element set"));
    }

    // Inclinations of exactly 180 degrees, and blank fields SGP4 reads as zeros, are fine
    CHECK_NOTHROW(SatRec::from_tle(line1, edit(line2, 9, "180.0000")));
    CHECK_NOTHROW(SatRec::from_tle(edit(line1, 45, "        "), line2));
}

TEST_CASE("SatRec: Ignores whitespace around lines", "[satrec]")
{
    const SatRec plain = SatRec::from_tle(ISS_LINE1, ISS_LINE2);
    const SatRec padded = SatRec::from_tle("  " + std::string(ISS_LINE1) + "\r\n",
                                           "\t" + std::string(ISS_LINE2) + "   \n");
    CHECK(padded.epoch() == plain.epoch());

    const Time time = plain.epoch() + units::Second(5000.0);
    CHECK(padded.evaluate(time)->position == plain.evaluate(time)->position);
}

TEST_CASE("SatRec: Reads Alpha-5 catalog numbers and numbers after spaces", "[satrec]")
{
    const Time time = SatRec::from_tle(ISS_LINE1, ISS_LINE2).epoch() + units::Second(3600.0);
    const Vec3<double> position = SatRec::from_tle(ISS_LINE1, ISS_LINE2).evaluate(time)->position;

    // A letter for the first two digits, skipping I and O
    const std::vector<std::pair<std::string, int>> numbers = {
        {"A0544", 100544},
        {"H0001", 170001},
        {"J0001", 180001},
        {"N9999", 229999},
        {"P0000", 230000},
        {"Z9999", 339999},
        {"  544", 544},
        {"    5", 5},
    };
    for (const auto& [field, number] : numbers) {
        INFO("Catalog number \"" << field << "\"");
        const SatRec satrec =
            SatRec::from_tle(edit(ISS_LINE1, 3, field), edit(ISS_LINE2, 3, field));
        CHECK(satrec.catalog_number() == number);

        // The catalog number has no part in propagating
        CHECK(satrec.evaluate(time)->position == position);
    }

    for (const std::string field : {"I0001", "O0001", "a0001", " A001", "     ", "2554x"}) {
        INFO("Catalog number \"" << field << "\"");
        CHECK_THROWS_WITH(SatRec::from_tle(edit(ISS_LINE1, 3, field), edit(ISS_LINE2, 3, field)),
                          ContainsSubstring("neither digits nor in the Alpha-5 form"));
    }
}

TEST_CASE("SatRec: Names and designators", "[satrec]")
{
    const SatRec unnamed = SatRec::from_tle(ISS_LINE1, ISS_LINE2);
    CHECK(unnamed.name().empty());
    CHECK(unnamed.international_designator() == "98067A");

    // As CelesTrak writes name lines, padded, and as Space-Track does, after "0 "
    for (const std::string name :
         {"ISS (ZARYA)", "ISS (ZARYA)             \r\n", "0 ISS (ZARYA)"}) {
        INFO("Name line \"" << name << "\"");
        CHECK(SatRec::from_tle(name, ISS_LINE1, ISS_LINE2).name() == "ISS (ZARYA)");
    }

    CHECK(SatRec::from_tle(edit(ISS_LINE1, 10, "        "), ISS_LINE2)
              .international_designator()
              .empty());
}

TEST_CASE("SatRec: Keeps no state between evaluations", "[satrec]")
{
    // A resonant deep-space orbit: SGP4 integrates its resonance in steps from the epoch, and
    // keeps how far it has got in the record it propagates
    const std::vector<test::VerificationCase> cases = test::load_verification_cases();
    const SatRec molniya = read_case(find_case(cases, "9880"), Sgp4Mode::AFSPC);

    std::vector<Time> times;
    for (int i = 0; i < 40; ++i) {
        times.push_back(molniya.epoch() + units::Second(((i * 37) % 40 - 20) * 7200.0));
    }
    std::vector<Vec3<double>> serial;
    for (const Time& time : times) {
        serial.push_back(molniya.evaluate(time)->position);
    }

    // In another order, from a copy
    const SatRec copy = molniya;
    for (std::size_t i = times.size(); i-- > 0;) {
        CHECK(copy.evaluate(times[i])->position == serial[i]);
    }

    // From several threads at once
    std::vector<std::vector<Vec3<double>>> results(4);
    std::vector<std::thread> threads;
    for (std::vector<Vec3<double>>& result : results) {
        threads.emplace_back([&molniya, &times, &result] {
            for (int repeat = 0; repeat < 10; ++repeat) {
                result.clear();
                for (const Time& time : times) {
                    result.push_back(molniya.evaluate(time)->position);
                }
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    for (const std::vector<Vec3<double>>& result : results) {
        CHECK(result == serial);
    }
}
