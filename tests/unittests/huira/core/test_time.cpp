#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "huira/core/time.hpp"

using namespace huira;

TEST_CASE("Time: Factory method from_et", "[time]") {
    SECTION("Zero ephemeris time") {
        Time t = Time::from_et(0.0);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(0.0, 1e-9));
        REQUIRE_THAT(t.ephemeris_time(), Catch::Matchers::WithinAbs(0.0, 1e-9));
    }

    SECTION("Positive ephemeris time") {
        Time t = Time::from_et(1000.0);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(1000.0, 1e-9));
    }

    SECTION("Negative ephemeris time") {
        Time t = Time::from_et(-5000.0);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(-5000.0, 1e-9));
    }
}

TEST_CASE("Time: Constructor from string (UTC)", "[time]") {
    SECTION("ISO 8601 format with space delimiter") {
        Time t("2000-01-01 12:00:00.000 UTC");
        REQUIRE(t.et() > 0.0);
    }

    SECTION("J2000 epoch") {
        Time t("2000-01-01 11:58:55.816 UTC");  // J2000 epoch in UTC
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(0.0, 1.0));
    }

    SECTION("Various date formats") {
        Time t1("2024-06-15 14:30:00 UTC");
        Time t2("JAN 1, 2010");
        Time t3("1 JUL 2015 18:00:00");

        REQUIRE(t1.et() != 0.0);
        REQUIRE(t2.et() != 0.0);
        REQUIRE(t3.et() != 0.0);
    }
}

TEST_CASE("Time: Factory methods", "[time]") {
    SECTION("from_et") {
        Time t = Time::from_et(12345.678);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(12345.678, 1e-9));
    }

    SECTION("from_ephemeris_time") {
        Time t = Time::from_ephemeris_time(98765.432);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(98765.432, 1e-9));
    }

    SECTION("from_julian_date with TDB") {
        double jd = 2451545.0;  // J2000.0
        Time t = Time::from_julian_date(jd, TimeScale::TDB);
        // J2000 epoch is ET = 0 in TDB
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(0.0, 1e-6));
    }

    SECTION("from_julian_date with TT") {
        double jd = 2451545.0;  // J2000.0 is defined in TT
        Time t = Time::from_julian_date(jd, TimeScale::TT);
        // Should be very close to ET = 0 (TDB ≈ TT)
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(0.0, 1.0));
    }

    SECTION("from_modified_julian_date with TDB") {
        double mjd = 51544.5;  // J2000.0
        Time t = Time::from_modified_julian_date(mjd, TimeScale::TDB);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(0.0, 1e-6));
    }
}

TEST_CASE("Time: Julian date conversions", "[time]") {
    SECTION("Roundtrip JD conversion in TDB") {
        double original_jd = 2459000.5;
        Time t = Time::from_julian_date(original_jd, TimeScale::TDB);
        double converted_jd = t.to_julian_date(TimeScale::TDB);

        REQUIRE_THAT(converted_jd, Catch::Matchers::WithinAbs(original_jd, 1e-9));
    }

    SECTION("Roundtrip JD conversion in TT") {
        double original_jd = 2459000.5;
        Time t = Time::from_julian_date(original_jd, TimeScale::TT);
        double converted_jd = t.to_julian_date(TimeScale::TT);

        REQUIRE_THAT(converted_jd, Catch::Matchers::WithinAbs(original_jd, 1e-6));
    }

    SECTION("Roundtrip JD conversion in TAI") {
        double original_jd = 2459000.5;
        Time t = Time::from_julian_date(original_jd, TimeScale::TAI);
        double converted_jd = t.to_julian_date(TimeScale::TAI);

        REQUIRE_THAT(converted_jd, Catch::Matchers::WithinAbs(original_jd, 1e-6));
    }

    SECTION("Roundtrip JD conversion in UTC") {
        double original_jd = 2459000.5;
        Time t = Time::from_julian_date(original_jd, TimeScale::UTC);
        double converted_jd = t.to_julian_date(TimeScale::UTC);

        REQUIRE_THAT(converted_jd, Catch::Matchers::WithinAbs(original_jd, 1e-6));
    }

    SECTION("Roundtrip MJD conversion") {
        double original_mjd = 58849.0;
        Time t = Time::from_modified_julian_date(original_mjd, TimeScale::TDB);
        double converted_mjd = t.to_modified_julian_date(TimeScale::TDB);

        REQUIRE_THAT(converted_mjd, Catch::Matchers::WithinAbs(original_mjd, 1e-9));
    }

    SECTION("MJD and JD relationship") {
        constexpr double mjd_offset = 2400000.5;
        double mjd = 60000.0;

        Time t1 = Time::from_modified_julian_date(mjd, TimeScale::TDB);
        Time t2 = Time::from_julian_date(mjd + mjd_offset, TimeScale::TDB);

        REQUIRE_THAT(t1.et(), Catch::Matchers::WithinAbs(t2.et(), 1e-9));
    }
}

TEST_CASE("Time: Timescale conversions", "[time]") {
    SECTION("TT vs TDB difference is small") {
        // TDB and TT differ by at most ~1.7ms
        double jd = 2459000.5;
        Time t = Time::from_julian_date(jd, TimeScale::TDB);

        double jd_tdb = t.to_julian_date(TimeScale::TDB);
        double jd_tt = t.to_julian_date(TimeScale::TT);

        // Difference should be < 2ms = 2e-3 seconds = 2.3e-8 days
        REQUIRE_THAT(jd_tdb - jd_tt, Catch::Matchers::WithinAbs(0.0, 3e-8));
    }

    SECTION("TAI vs TT offset is 32.184 seconds") {
        Time t = Time::from_et(0.0);  // J2000.0 TDB

        double jd_tt = t.to_julian_date(TimeScale::TT);
        double jd_tai = t.to_julian_date(TimeScale::TAI);

        // TT = TAI + 32.184s, so JD_TT - JD_TAI = 32.184 / 86400
        double expected_offset = 32.184 / 86400.0;
        REQUIRE_THAT(jd_tt - jd_tai, Catch::Matchers::WithinAbs(expected_offset, 1e-9));
    }

    SECTION("UTC vs TAI includes leap seconds") {
        // At J2000.0 (Jan 1, 2000), there were 32 leap seconds
        Time t = Time::from_et(0.0);

        double jd_tai = t.to_julian_date(TimeScale::TAI);
        double jd_utc = t.to_julian_date(TimeScale::UTC);

        // TAI = UTC + leap_seconds, so JD_TAI - JD_UTC = leap_seconds / 86400
        // At J2000, leap seconds = 32
        double expected_offset = 32.0 / 86400.0;
        REQUIRE_THAT(jd_tai - jd_utc, Catch::Matchers::WithinAbs(expected_offset, 1e-6));
    }
}

TEST_CASE("Time: julian_years_since_j2000", "[time]") {
    SECTION("At J2000 epoch") {
        Time t = Time::from_julian_date(Time::J2000_JD, TimeScale::TT);
        double years = t.julian_years_since_j2000(TimeScale::TT);
        REQUIRE_THAT(years, Catch::Matchers::WithinAbs(0.0, 1e-6));
    }

    SECTION("One Julian year after J2000") {
        double jd_one_year = Time::J2000_JD + Time::DAYS_PER_JULIAN_YEAR;
        Time t = Time::from_julian_date(jd_one_year, TimeScale::TT);
        double years = t.julian_years_since_j2000(TimeScale::TT);
        REQUIRE_THAT(years, Catch::Matchers::WithinAbs(1.0, 1e-6));
    }

    SECTION("Ten Julian years before J2000") {
        double jd_ten_years_before = Time::J2000_JD - (10.0 * Time::DAYS_PER_JULIAN_YEAR);
        Time t = Time::from_julian_date(jd_ten_years_before, TimeScale::TT);
        double years = t.julian_years_since_j2000(TimeScale::TT);
        REQUIRE_THAT(years, Catch::Matchers::WithinAbs(-10.0, 1e-6));
    }

    SECTION("Default timescale is TT") {
        Time t = Time::from_julian_date(Time::J2000_JD, TimeScale::TT);
        double years_default = t.julian_years_since_j2000();
        double years_tt = t.julian_years_since_j2000(TimeScale::TT);
        REQUIRE_THAT(years_default, Catch::Matchers::WithinAbs(years_tt, 1e-9));
    }
}

TEST_CASE("Time: String conversions", "[time]") {
    SECTION("to_iso_8601") {
        Time t = Time::from_et(0.0);  // J2000 epoch
        std::string iso = t.to_iso_8601();

        REQUIRE_FALSE(iso.empty());
        REQUIRE(iso.find('T') != std::string::npos);  // Contains 'T' separator
        REQUIRE(iso.back() == 'Z');  // Ends with 'Z'
    }

    SECTION("to_utc_string with default format") {
        Time t = Time::from_et(1000.0);
        std::string utc = t.to_utc_string();

        REQUIRE_FALSE(utc.empty());
        REQUIRE(utc.find("UTC") != std::string::npos);
    }

    SECTION("to_utc_string with custom format") {
        Time t = Time::from_et(0.0);
        std::string utc = t.to_utc_string("YYYY-MM-DD");

        REQUIRE_FALSE(utc.empty());
    }

    SECTION("Roundtrip string conversion") {
        std::string original = "2020-07-15 09:30:00.000 UTC";
        Time t1(original);
        std::string converted = t1.to_utc_string("YYYY-MM-DD HR:MN:SC.### UTC");
        Time t2(converted);

        REQUIRE_THAT(t1.et(), Catch::Matchers::WithinAbs(t2.et(), 1e-3));
    }
}

TEST_CASE("Time: Comparison operators", "[time]") {
    Time t1 = Time::from_et(1000.0);
    Time t2 = Time::from_et(2000.0);
    Time t3 = Time::from_et(1000.0);

    SECTION("Equality") {
        REQUIRE(t1 == t3);
        REQUIRE_FALSE(t1 == t2);
    }

    SECTION("Inequality") {
        REQUIRE(t1 != t2);
        REQUIRE_FALSE(t1 != t3);
    }

    SECTION("Less than") {
        REQUIRE(t1 < t2);
        REQUIRE_FALSE(t2 < t1);
        REQUIRE_FALSE(t1 < t3);
    }

    SECTION("Other comparison operators") {
        REQUIRE(t2 > t1);
        REQUIRE(t1 <= t2);
        REQUIRE(t1 <= t3);
        REQUIRE(t2 >= t1);
        REQUIRE(t1 >= t3);
    }
}

TEST_CASE("Time: Edge cases", "[time]") {
    SECTION("Very large ephemeris time") {
        double large_et = 1e15;
        Time t = Time::from_et(large_et);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(large_et, 1e6));
    }

    SECTION("Very small ephemeris time") {
        double small_et = -1e15;
        Time t = Time::from_et(small_et);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(small_et, 1e6));
    }

    SECTION("Fractional seconds precision") {
        double et_with_fraction = 1000.123456789;
        Time t = Time::from_et(et_with_fraction);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(et_with_fraction, 1e-9));
    }
}

TEST_CASE("Time: Real-world scenarios", "[time]") {
    SECTION("Historical date") {
        Time apollo11("1969-07-20 20:17:40 UTC");  // Apollo 11 moon landing
        REQUIRE(apollo11.et() < 0.0);  // Before J2000 epoch
    }

    SECTION("Far future date") {
        Time future("2100-01-01 00:00:00 UTC");
        REQUIRE(future.et() > 0.0);  // After J2000 epoch
    }

    SECTION("Proper motion calculation use case") {
        // Typical use: compute dt for Tycho-2 catalog
        Time obs_time("2024-06-15 00:00:00 UTC");
        double dt = obs_time.julian_years_since_j2000(TimeScale::TT);

        // Should be approximately 24.45 years
        REQUIRE_THAT(dt, Catch::Matchers::WithinAbs(24.45, 0.1));
    }
}

TEST_CASE("Time: Constants", "[time]") {
    SECTION("J2000_JD value") {
        REQUIRE_THAT(Time::J2000_JD, Catch::Matchers::WithinAbs(2451545.0, 1e-9));
    }

    SECTION("DAYS_PER_JULIAN_YEAR value") {
        REQUIRE_THAT(Time::DAYS_PER_JULIAN_YEAR, Catch::Matchers::WithinAbs(365.25, 1e-9));
    }

    SECTION("MJD_OFFSET value") {
        REQUIRE_THAT(Time::MJD_OFFSET, Catch::Matchers::WithinAbs(2400000.5, 1e-9));
    }

    SECTION("TT_TAI_OFFSET value") {
        REQUIRE_THAT(Time::TT_TAI_OFFSET, Catch::Matchers::WithinAbs(32.184, 1e-9));
    }
}
