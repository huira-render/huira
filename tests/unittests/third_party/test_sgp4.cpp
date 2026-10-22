// Vallado's reference SGP4, vendored in include/huira_impl/third_party/sgp4/ and included through
// huira_impl/third_party/sgp4.ipp, checked against Vallado's own verification cases
// (SGP4-VER.TLE) and his test program's output for them (tcppver.out), from the same zip as the
// code: see third_party/sgp4/NOTICE.
//
// The test is two source files, this one and sgp4_other.cpp, that both include the wrapper: it
// doesn't link if the wrapper leaves anything with external linkage.

#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <ios>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "huira_impl/third_party/sgp4.ipp"
#include "sgp4_other.hpp"

namespace {

using namespace huira::detail::vallado;

// SGP4.cpp defines a macro `pi`, which the wrapper undefines. If it didn't, this wouldn't compile.
constexpr double pi = std::numbers::pi;

/// One of Vallado's verification cases: an element set, and the states his program printed for it.
struct VerificationCase {
    struct State {
        double tsince = 0.0;          // Minutes from the element set's epoch
        std::array<double, 3> r = {}; // TEME position, km
        std::array<double, 3> v = {}; // TEME velocity, km/s
    };

    std::string line1;
    std::string line2;
    std::string satnum; // As tcppver.out names the case
    std::vector<State> states;
};

/// A file's lines, without line endings. Text after the last line ending is dropped: tcppver.out
/// ends with a truncated line, part of an earlier state, with no line ending.
std::vector<std::string> read_lines(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (file.eof()) {
            break; // getline() reached the end of the file before a line ending
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

/// Vallado's verification cases. In SGP4-VER.TLE, each case's second line is followed by the start,
/// stop and step of its run, in minutes. tcppver.out has a line "<satnum> xx" for each case, in the
/// same order, followed by one line per state: minutes from epoch, position and velocity, then
/// (except at epoch) orbital elements and the date.
std::vector<VerificationCase> load_cases()
{
    const std::string directory = HUIRA_SGP4_TEST_DATA_DIR;

    std::vector<VerificationCase> cases;
    std::string line1;
    for (const std::string& line : read_lines(directory + "/SGP4-VER.TLE")) {
        if (line.starts_with("1 ")) {
            line1 = line;
        } else if (line.starts_with("2 ")) {
            cases.push_back({line1, line, {}, {}});
        }
    }

    std::size_t headers = 0;
    for (const std::string& line : read_lines(directory + "/tcppver.out")) {
        std::istringstream fields(line);
        if (line.find("xx") != std::string::npos) {
            REQUIRE(headers < cases.size());
            fields >> cases[headers].satnum;
            ++headers;
            continue;
        }
        VerificationCase::State state;
        if (headers > 0 && fields >> state.tsince >> state.r[0] >> state.r[1] >> state.r[2] >>
                               state.v[0] >> state.v[1] >> state.v[2]) {
            cases[headers - 1].states.push_back(state);
        }
    }
    REQUIRE(headers == cases.size());
    return cases;
}

/// The case's element set, read in verification mode.
elsetrec read_element_set(const VerificationCase& verification_case, char opsmode)
{
    char line1[130] = {};
    char line2[130] = {};
    verification_case.line1.copy(line1, sizeof(line1) - 1);
    verification_case.line2.copy(line2, sizeof(line2) - 1);

    double startmfe = 0.0;
    double stopmfe = 0.0;
    double deltamin = 0.0;
    elsetrec satrec{};
    SGP4Funcs::twoline2rv(
        line1, line2, 'v', 'e', opsmode, wgs72, startmfe, stopmfe, deltamin, satrec);
    return satrec;
}

/// The largest difference between two vectors' components.
double largest_difference(const double (&a)[3], const std::array<double, 3>& b)
{
    double largest = 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
        largest = std::fmax(largest, std::fabs(a[i] - b[i]));
    }
    return largest;
}

// tcppver.out prints positions to 1e-8 km and velocities to 1e-9 km/s. The vendored code matches
// it to about 1e-7 km and 5e-10 km/s (rounding, and compilers' floating point); these leave room
// for other compilers, and are still far below the kilometre that an element set is good to.
constexpr double position_tolerance = 1e-5; // km
constexpr double velocity_tolerance = 1e-8; // km/s

// 33334 is Vallado's case of an element set that fails at epoch: its eccentricity, once
// perturbed, is out of range (error 3). tcppver.out still prints a state for it.
bool fails_at_epoch(const VerificationCase& verification_case)
{
    return verification_case.satnum == "33334";
}

} // namespace

TEST_CASE("Vendored SGP4: AFSPC mode reproduces Vallado's verification output", "[sgp4]")
{
    // Vallado made tcppver.out in AFSPC mode ('a'), with the WGS-72 constants
    const std::vector<VerificationCase> cases = load_cases();
    REQUIRE_FALSE(cases.empty());

    std::size_t states = 0;
    for (const VerificationCase& verification_case : cases) {
        INFO("Satellite " << verification_case.satnum);
        CHECK(std::stol(verification_case.satnum) ==
              std::stol(verification_case.line1.substr(2, 5)));
        CHECK_FALSE(verification_case.states.empty());

        elsetrec satrec = read_element_set(verification_case, 'a');
        for (const VerificationCase::State& state : verification_case.states) {
            INFO("Minutes from epoch: " << state.tsince);
            double r[3] = {};
            double v[3] = {};
            const bool ok = SGP4Funcs::sgp4(satrec, state.tsince, r, v);
            if (fails_at_epoch(verification_case)) {
                CHECK_FALSE(ok);
                CHECK(satrec.error == 3);
                continue;
            }
            REQUIRE(ok);
            CHECK(largest_difference(r, state.r) < position_tolerance);
            CHECK(largest_difference(v, state.v) < velocity_tolerance);
            ++states;
        }
    }
    CHECK(states > 600);
}

TEST_CASE("Vendored SGP4: Improved mode departs from Vallado's output only for 23599", "[sgp4]")
{
    // Improved mode ('i') differs from AFSPC mode by design for deep-space orbits inclined less
    // than 11.46 degrees, such as 23599's: by about a kilometre after a few hours. Elsewhere it
    // matches tcppver.out, which was made in AFSPC mode.
    const std::vector<VerificationCase> cases = load_cases();

    double largest_23599_difference = 0.0;
    for (const VerificationCase& verification_case : cases) {
        if (fails_at_epoch(verification_case)) {
            continue;
        }
        INFO("Satellite " << verification_case.satnum);

        elsetrec satrec = read_element_set(verification_case, 'i');
        for (const VerificationCase::State& state : verification_case.states) {
            INFO("Minutes from epoch: " << state.tsince);
            double r[3] = {};
            double v[3] = {};
            REQUIRE(SGP4Funcs::sgp4(satrec, state.tsince, r, v));
            if (verification_case.satnum == "23599") {
                largest_23599_difference =
                    std::fmax(largest_23599_difference, largest_difference(r, state.r));
                continue;
            }
            CHECK(largest_difference(r, state.r) < position_tolerance);
            CHECK(largest_difference(v, state.v) < velocity_tolerance);
        }
    }
    CHECK(largest_23599_difference > 0.1); // km
}

TEST_CASE("Vendored SGP4 can be included from more than one source file", "[sgp4][build]")
{
    // Linking is the test. The other source file propagates the same element set:
    char line1[130] = "1 25544U 98067A   08264.51782528 -.00002182  00000-0 -11606-4 0  2927";
    char line2[130] = "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537";
    double startmfe = 0.0;
    double stopmfe = 0.0;
    double deltamin = 0.0;
    elsetrec satrec{};
    SGP4Funcs::twoline2rv(line1, line2, 'c', 'e', 'a', wgs72, startmfe, stopmfe, deltamin, satrec);
    double r[3] = {};
    double v[3] = {};
    REQUIRE(SGP4Funcs::sgp4(satrec, 0.0, r, v));

    const std::array<double, 3> other = huira::test::iss_position_from_another_source_file();
    CHECK(other[0] == r[0]);
    CHECK(other[1] == r[1]);
    CHECK(other[2] == r[2]);
}

TEST_CASE("Vendored SGP4 doesn't leave its pi macro defined", "[sgp4][build]")
{
    // Compiling is the test (see `pi` at the top of this file)
    CHECK(pi == std::numbers::pi);
}
