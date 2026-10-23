#pragma once

// Vallado's SGP4 verification cases (SGP4-VER.TLE) and his test program's output for them
// (tcppver.out), from the same zip as the vendored code: see third_party/sgp4/NOTICE. A test that
// includes this defines HUIRA_SGP4_TEST_DATA_DIR as the directory they are in.

#include <array>
#include <cstddef>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace huira::test {

/// One of Vallado's verification cases: an element set, and the states his program printed for it.
struct VerificationCase {
    struct State {
        double tsince = 0.0;          // Minutes from the element set's epoch
        std::array<double, 3> r = {}; // TEME position, km
        std::array<double, 3> v = {}; // TEME velocity, km/s
    };

    std::string line1;
    std::string line2;  // With the start, stop and step of the case's run after its 69 columns
    std::string satnum; // As tcppver.out names the case
    std::vector<State> states;
};

/// A file's lines, without line endings. Text after the last line ending is dropped: tcppver.out
/// ends with a truncated line, part of an earlier state, with no line ending.
inline std::vector<std::string> read_lines(const std::string& path)
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
inline std::vector<VerificationCase> load_verification_cases()
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

} // namespace huira::test
