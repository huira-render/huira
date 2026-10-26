#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
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

/// The ISS's element set above, as CelesTrak writes its OMMs in JSON (each value as Python writes
/// the number), and as SatRec::from_omm() is given them.
OmmFields iss_omm()
{
    return {
        {"OBJECT_NAME", "ISS (ZARYA)"},
        {"OBJECT_ID", "1998-067A"},
        {"EPOCH", "2008-09-20T12:25:40.104192"},
        {"MEAN_MOTION", "15.72125391"},
        {"ECCENTRICITY", "0.0006703"},
        {"INCLINATION", "51.6416"},
        {"RA_OF_ASC_NODE", "247.4627"},
        {"ARG_OF_PERICENTER", "130.536"},
        {"MEAN_ANOMALY", "325.0288"},
        {"EPHEMERIS_TYPE", "0"},
        {"CLASSIFICATION_TYPE", "U"},
        {"NORAD_CAT_ID", "25544"},
        {"ELEMENT_SET_NO", "292"},
        {"REV_AT_EPOCH", "56353"},
        {"BSTAR", "-1.1606e-05"},
        {"MEAN_MOTION_DOT", "-2.182e-05"},
        {"MEAN_MOTION_DDOT", "0"},
    };
}

OmmFields with(OmmFields fields, const std::string& keyword, const std::string& value)
{
    fields[keyword] = value;
    return fields;
}

OmmFields without(OmmFields fields, const std::string& keyword)
{
    fields.erase(keyword);
    return fields;
}

/// A TLE line, cut to its 69 columns, with its checksum made to match the rest.
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

/// Columns of a TLE line, counted from 1 as the format counts them, without spaces around them.
std::string columns(std::string_view line, std::size_t first, std::size_t last)
{
    std::string text(line.substr(first - 1, last - first + 1));
    const std::size_t begin = text.find_first_not_of(' ');
    if (begin == std::string::npos) {
        return {};
    }
    return text.substr(begin, text.find_last_not_of(' ') - begin + 1);
}

/// A TLE's mantissa-and-exponent field, from its sign's column (counted from 1), such as
/// " 12345-5" for 0.12345e-5, as an OMM writes it.
std::string exponential(std::string_view line, std::size_t first)
{
    const std::size_t sign = first - 1; // Counted from 0
    std::string mantissa(line.substr(sign + 1, 5));
    for (char& c : mantissa) {
        c = c == ' ' ? '0' : c;
    }
    const char exponent_sign = line[sign + 6] == '-' ? '-' : '+';
    const char exponent_digit = line[sign + 7] == ' ' ? '0' : line[sign + 7];
    return std::string(line[sign] == '-' ? "-" : "") + "0." + mantissa + "E" + exponent_sign +
           exponent_digit;
}

/// The OMM that a TLE is the same element set as, as CelesTrak and Space-Track make one from the
/// other: its fields, written out as numbers, and its epoch as a calendar date and time.
OmmFields omm_from_tle(std::string_view line1, std::string_view line2)
{
    const int two_digit_year = std::stoi(columns(line1, 19, 20));
    const int year = two_digit_year + (two_digit_year < 57 ? 2000 : 1900);
    int day_of_year = std::stoi(columns(line1, 21, 23));

    // Eight decimals of a day, each 864 microseconds
    const std::int64_t microseconds = std::stoll(columns(line1, 25, 32)) * 864;
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    const std::array<int, 12> lengths = {
        31, leap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int month = 1;
    while (day_of_year > lengths[static_cast<std::size_t>(month - 1)]) {
        day_of_year -= lengths[static_cast<std::size_t>(month - 1)];
        ++month;
    }
    std::ostringstream epoch;
    epoch << std::setfill('0') << std::setw(4) << year << '-' << std::setw(2) << month << '-'
          << std::setw(2) << day_of_year << 'T' << std::setw(2) << microseconds / 3600000000 << ':'
          << std::setw(2) << microseconds / 60000000 % 60 << ':' << std::setw(2)
          << microseconds / 1000000 % 60 << '.' << std::setw(6) << microseconds % 1000000;

    // The designator, such as 98067A, as 1998-067A
    const std::string designator = columns(line1, 10, 17);
    std::string object_id;
    if (!designator.empty()) {
        const int launch_year = std::stoi(designator.substr(0, 2));
        object_id = std::to_string(launch_year + (launch_year < 57 ? 2000 : 1900)) + "-" +
                    designator.substr(2);
    }

    const std::string rate = columns(line1, 34, 43);
    return {
        {"OBJECT_ID", object_id},
        {"EPOCH", epoch.str()},
        {"MEAN_ELEMENT_THEORY", "SGP4"},
        {"REF_FRAME", "TEME"},
        {"CENTER_NAME", "EARTH"},
        {"TIME_SYSTEM", "UTC"},
        {"MEAN_MOTION", columns(line2, 53, 63)},
        {"ECCENTRICITY", "0." + columns(line2, 27, 33)},
        {"INCLINATION", columns(line2, 9, 16)},
        {"RA_OF_ASC_NODE", columns(line2, 18, 25)},
        {"ARG_OF_PERICENTER", columns(line2, 35, 42)},
        {"MEAN_ANOMALY", columns(line2, 44, 51)},
        {"EPHEMERIS_TYPE", "0"},
        {"CLASSIFICATION_TYPE", "U"},
        {"NORAD_CAT_ID", std::to_string(std::stoi(columns(line1, 3, 7)))},
        {"ELEMENT_SET_NO", columns(line1, 65, 68)},
        {"REV_AT_EPOCH", columns(line2, 64, 68)},
        {"BSTAR", exponential(line1, 54)},
        {"MEAN_MOTION_DOT", (rate[0] == '-' ? "-0" : "0") + rate.substr(rate.find('.'))},
        {"MEAN_MOTION_DDOT", exponential(line1, 45)},
    };
}

/// The largest difference between two states' positions and velocities.
std::pair<double, double> difference(const PositionState& a, const PositionState& b)
{
    return {glm::length(a.position - b.position), glm::length(a.velocity - b.velocity)};
}

} // namespace

TEST_CASE("SatRec: An OMM made from a TLE propagates as the TLE does", "[satrec][omm]")
{
    // Over every one of Vallado's cases and the times of his output for them. They could differ in
    // the last bit of a number or two, as B* and the second derivative are one number in an OMM
    // and a mantissa and an exponent in a TLE, and the epoch is a date and time, not a day of the
    // year. For these cases they don't: the states are the same to the bit.
    const std::vector<test::VerificationCase> cases = test::load_verification_cases();
    std::size_t states = 0;
    double largest_position = 0.0;
    double largest_velocity = 0.0;
    for (const test::VerificationCase& verification_case : cases) {
        INFO("Satellite " << verification_case.satnum);
        const std::string line1 = with_checksum(verification_case.line1);
        const std::string line2 = with_checksum(verification_case.line2);
        const OmmFields omm = omm_from_tle(line1, line2);

        // 33334 can't be propagated even at its epoch, from either
        if (verification_case.satnum == "33334") {
            CHECK_THROWS_WITH(SatRec::from_omm(omm), ContainsSubstring("SGP4 error 3"));
            continue;
        }

        for (const Sgp4Mode mode : {Sgp4Mode::AFSPC, Sgp4Mode::IMPROVED}) {
            const SatRec from_tle = SatRec::from_tle(line1, line2, mode);
            const SatRec from_omm = SatRec::from_omm(omm, mode);
            CHECK(from_omm.catalog_number() == from_tle.catalog_number());
            CHECK(from_omm.international_designator() == from_tle.international_designator());
            CHECK(from_omm.mode() == mode);
            CHECK_THAT(from_omm.epoch().et(), WithinAbs(from_tle.epoch().et(), 1e-6));

            for (const test::VerificationCase::State& state : verification_case.states) {
                INFO("Minutes from epoch: " << state.tsince);
                const Time time = from_tle.epoch() + units::Second(state.tsince * 60.0);
                const std::optional<PositionState> a = from_tle.evaluate(time);
                const std::optional<PositionState> b = from_omm.evaluate(time);
                REQUIRE(a.has_value());
                REQUIRE(b.has_value());
                const auto [position, velocity] = difference(*a, *b);
                largest_position = std::max(largest_position, position);
                largest_velocity = std::max(largest_velocity, velocity);
                ++states;
            }
        }
    }
    CHECK(states > 1200);
    CHECK(largest_position < 1e-3); // m
    CHECK(largest_velocity < 1e-6); // m/s
}

TEST_CASE("SatRec: Reads an OMM as CelesTrak and Space-Track write it", "[satrec][omm]")
{
    const SatRec tle = SatRec::from_tle(ISS_LINE1, ISS_LINE2);
    const auto same_as_tle = [&tle](const SatRec& omm) {
        CHECK_THAT(omm.epoch().et(), WithinAbs(tle.epoch().et(), 1e-6));
        for (const double minutes : {0.0, 90.0, 1440.0}) {
            const Time time = tle.epoch() + units::Second(minutes * 60.0);
            const auto [position, velocity] = difference(*omm.evaluate(time), *tle.evaluate(time));
            CHECK(position < 1e-3);
            CHECK(velocity < 1e-6);
        }
    };

    SECTION("CelesTrak, its numbers as Python writes them")
    {
        const SatRec omm = SatRec::from_omm(iss_omm());
        same_as_tle(omm);
        CHECK(omm.catalog_number() == 25544);
        CHECK(omm.name() == "ISS (ZARYA)");
        CHECK(omm.international_designator() == "98067A");
    }

    SECTION("In KVN, with exponents as a TLE writes them")
    {
        OmmFields fields = iss_omm();
        fields["BSTAR"] = "-0.11606E-04";
        fields["MEAN_MOTION_DOT"] = "-0.00002182";
        fields["MEAN_MOTION_DDOT"] = "0.0E+0";
        fields["MEAN_ELEMENT_THEORY"] = "SGP4";
        fields["REF_FRAME"] = "TEME";
        fields["CENTER_NAME"] = "EARTH";
        fields["TIME_SYSTEM"] = "UTC";
        same_as_tle(SatRec::from_omm(fields));
    }

    SECTION("With the fields Space-Track adds, which are ignored")
    {
        OmmFields fields = iss_omm();
        fields["SEMIMAJOR_AXIS"] = "6726.433";
        fields["PERIOD"] = "91.596";
        fields["OBJECT_TYPE"] = "PAYLOAD";
        fields["DECAY_DATE"] = "";
        fields["TLE_LINE1"] = std::string(ISS_LINE1);
        same_as_tle(SatRec::from_omm(fields));
    }

    SECTION("With whitespace around values, and metadata in lower case")
    {
        OmmFields fields = iss_omm();
        fields["MEAN_MOTION"] = "  15.72125391 ";
        fields["OBJECT_NAME"] = " ISS (ZARYA)  ";
        fields["REF_FRAME"] = "teme";
        fields["MEAN_ELEMENT_THEORY"] = "SGP/SGP4";
        const SatRec omm = SatRec::from_omm(fields);
        same_as_tle(omm);
        CHECK(omm.name() == "ISS (ZARYA)");
    }

    SECTION("With the epoch as a day of the year, with a Z, or to the second")
    {
        same_as_tle(SatRec::from_omm(with(iss_omm(), "EPOCH", "2008-264T12:25:40.104192")));
        same_as_tle(SatRec::from_omm(with(iss_omm(), "EPOCH", "2008-09-20T12:25:40.104192Z")));
        const SatRec whole = SatRec::from_omm(with(iss_omm(), "EPOCH", "2008-09-20T12:25:40"));
        CHECK_THAT(whole.epoch().et(), WithinAbs(tle.epoch().et() - 0.104192, 1e-6));

        // The last day of a leap year
        const SatRec leap = SatRec::from_omm(with(iss_omm(), "EPOCH", "2016-366T12:00:00"));
        CHECK_THAT(leap.epoch().et(), WithinAbs(Time("2016-12-31 12:00:00 UTC").et(), 1e-6));
    }
}

TEST_CASE("SatRec: Reads catalog numbers and designators from an OMM", "[satrec][omm]")
{
    const Time time = SatRec::from_omm(iss_omm()).epoch() + units::Second(3600.0);
    const Vec3<double> position = SatRec::from_omm(iss_omm()).evaluate(time)->position;

    // Past the five digits of a TLE: the Alpha-5 range, and the nine digits an OMM allows
    for (const int number : {100544, 339999, 340000, 270000001}) {
        INFO("NORAD_CAT_ID " << number);
        const SatRec satrec =
            SatRec::from_omm(with(iss_omm(), "NORAD_CAT_ID", std::to_string(number)));
        CHECK(satrec.catalog_number() == number);
        CHECK(satrec.evaluate(time)->position == position); // It has no part in propagating
    }

    // An analyst object has no designator: CelesTrak leaves it out, and the standard allows
    // UNKNOWN
    CHECK(SatRec::from_omm(with(iss_omm(), "OBJECT_ID", "UNKNOWN")).international_designator() ==
          "");
    CHECK(SatRec::from_omm(without(iss_omm(), "OBJECT_ID")).international_designator().empty());
    CHECK(
        SatRec::from_omm(with(iss_omm(), "OBJECT_ID", "2023-001ABC")).international_designator() ==
        "23001ABC");
    CHECK(SatRec::from_omm(without(iss_omm(), "OBJECT_NAME")).name().empty());
}

TEST_CASE("SatRec: Rejects an OMM SGP4 can't use", "[satrec][omm]")
{
    SECTION("A field SGP4 needs is missing, or empty")
    {
        for (const std::string keyword : {"NORAD_CAT_ID",
                                          "EPOCH",
                                          "MEAN_MOTION",
                                          "ECCENTRICITY",
                                          "INCLINATION",
                                          "RA_OF_ASC_NODE",
                                          "ARG_OF_PERICENTER",
                                          "MEAN_ANOMALY",
                                          "BSTAR",
                                          "MEAN_MOTION_DOT",
                                          "MEAN_MOTION_DDOT"}) {
            INFO(keyword);
            CHECK_THROWS_WITH(SatRec::from_omm(without(iss_omm(), keyword)),
                              ContainsSubstring("The OMM has no " + keyword));
            CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), keyword, " ")),
                              ContainsSubstring("The OMM has no " + keyword));
        }
    }

    SECTION("A number that isn't one")
    {
        for (const std::string text : {"15.7x", "inf", "nan", "0x1p4", "1e999", "15..7", "--15"}) {
            INFO(text);
            CHECK_THROWS_WITH(
                SatRec::from_omm(with(iss_omm(), "MEAN_MOTION", text)),
                ContainsSubstring("MEAN_MOTION is \"" + text + "\", which isn't a finite number"));
        }
        for (const std::string text : {"0", "-5", "25544.0", "1234567890", "2554x"}) {
            INFO(text);
            CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "NORAD_CAT_ID", text)),
                              ContainsSubstring("NORAD_CAT_ID is \"" + text + "\""));
        }
    }

    SECTION("Elements out of range")
    {
        CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "MEAN_MOTION", "0")),
                          ContainsSubstring("where it should be positive"));
        CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "ECCENTRICITY", "1.0")),
                          ContainsSubstring("from 0 to below 1"));
        CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "ECCENTRICITY", "-0.001")),
                          ContainsSubstring("from 0 to below 1"));
        CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "INCLINATION", "180.5")),
                          ContainsSubstring("from 0 to 180"));
        CHECK_NOTHROW(SatRec::from_omm(with(iss_omm(), "INCLINATION", "180")));
    }

    SECTION("An epoch that isn't a UTC time")
    {
        for (const std::string text : {"2008-09-20 12:25:40",
                                       "2008-02-30T00:00:00",
                                       "2008-13-01T00:00:00",
                                       "2015-366T00:00:00",
                                       "2008-09-20T24:00:00",
                                       "2008-09-20T12:60:00",
                                       "2016-12-31T23:59:60",
                                       "1899-12-31T00:00:00",
                                       "2101-01-01T00:00:00",
                                       "2008-09-20T12:25:40,5",
                                       "2008-09-20T12:25:40.",
                                       "20080920T122540"}) {
            INFO(text);
            CHECK_THROWS_WITH(
                SatRec::from_omm(with(iss_omm(), "EPOCH", text)),
                ContainsSubstring("EPOCH is \"" + text + "\", which isn't a UTC time"));
        }
    }

    SECTION("Metadata for something other than SGP4 in TEME about the Earth, in UTC")
    {
        CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "MEAN_ELEMENT_THEORY", "SGP4-XP")),
                          ContainsSubstring("SGP4-XP"));
        CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "EPHEMERIS_TYPE", "4")),
                          ContainsSubstring("SGP4-XP (EPHEMERIS_TYPE 4)"));
        CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "MEAN_ELEMENT_THEORY", "DSST")),
                          ContainsSubstring("MEAN_ELEMENT_THEORY is \"DSST\""));
        CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "REF_FRAME", "GCRF")),
                          ContainsSubstring("REF_FRAME is \"GCRF\""));
        CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "CENTER_NAME", "MOON")),
                          ContainsSubstring("CENTER_NAME is \"MOON\""));
        CHECK_THROWS_WITH(SatRec::from_omm(with(iss_omm(), "TIME_SYSTEM", "TAI")),
                          ContainsSubstring("TIME_SYSTEM is \"TAI\""));
    }

    SECTION("Errors name the satellite as far as it is known")
    {
        CHECK_THROWS_WITH(SatRec::from_omm(without(iss_omm(), "NORAD_CAT_ID")),
                          ContainsSubstring("SatRec::from_omm - \"ISS (ZARYA)\": The OMM has no"));
        CHECK_THROWS_WITH(SatRec::from_omm(without(iss_omm(), "BSTAR")),
                          ContainsSubstring("Satellite 25544, \"ISS (ZARYA)\": The OMM has no"));
    }
}
