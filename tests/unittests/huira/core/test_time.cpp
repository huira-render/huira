#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "huira/core/time.hpp"
#include <chrono>
#include <thread>

using namespace huira;
using namespace std::chrono_literals;

TEST_CASE("Time: Constructor from ephemeris time", "[time]") {
    SECTION("Zero ephemeris time") {
        Time t(0.0);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(0.0, 1e-9));
        REQUIRE_THAT(t.ephemeris_time(), Catch::Matchers::WithinAbs(0.0, 1e-9));
    }

    SECTION("Positive ephemeris time") {
        Time t(1000.0);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(1000.0, 1e-9));
    }

    SECTION("Negative ephemeris time") {
        Time t(-5000.0);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(-5000.0, 1e-9));
    }
}

TEST_CASE("Time: Constructor from string", "[time]") {
    SECTION("ISO 8601 format with space delimiter") {
        Time t("2000-01-01 12:00:00.000 UTC");
        REQUIRE(t.et() > 0.0);
    }

    SECTION("J2000 epoch") {
        Time t("2000-01-01 11:58:55.816 UTC");  // J2000 epoch
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

TEST_CASE("Time: Constructor from system_clock", "[time]") {
    auto now_sys = std::chrono::system_clock::now();
    Time t(now_sys);
    
    // Convert back and verify roundtrip
    auto converted = t.to_system_clock();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(converted - now_sys);
    
    // Should be very close (within 1 second due to conversion precision)
    REQUIRE(std::abs(diff.count()) < 1000);
}

#if HUIRA_HAS_UTC_CLOCK
TEST_CASE("Time: Constructor from utc_clock", "[time]") {
    auto now_utc = std::chrono::utc_clock::now();
    Time t(now_utc);
    
    // Convert back and verify roundtrip
    auto converted = t.to_utc_clock();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(converted - now_utc);
    
    REQUIRE(std::abs(diff.count()) < 1000);
}
#endif

TEST_CASE("Time: Factory methods", "[time]") {
    SECTION("from_et") {
        Time t = Time::from_et(12345.678);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(12345.678, 1e-9));
    }

    SECTION("from_ephemeris_time") {
        Time t = Time::from_ephemeris_time(98765.432);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(98765.432, 1e-9));
    }

    SECTION("from_julian_date") {
        double jd = 2451545.0;  // J2000.0
        Time t = Time::from_julian_date(jd);
        // J2000 epoch is approximately ET = 0
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(0.0, 100.0));
    }

    SECTION("from_modified_julian_date") {
        double mjd = 51544.5;  // J2000.0
        Time t = Time::from_modified_julian_date(mjd);
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(0.0, 100.0));
    }

    SECTION("now") {
        Time t1 = Time::now();
        std::this_thread::sleep_for(10ms);
        Time t2 = Time::now();
        
        REQUIRE(t2.et() > t1.et());
        auto diff = t2 - t1;
        REQUIRE(diff.count() >= 0.01);  // At least 10ms
    }
}

TEST_CASE("Time: Julian date conversions", "[time]") {
    SECTION("Roundtrip JD conversion") {
        double original_jd = 2459000.5;
        Time t = Time::from_julian_date(original_jd);
        double converted_jd = t.to_julian_date();
        
        REQUIRE_THAT(converted_jd, Catch::Matchers::WithinAbs(original_jd, 1e-6));
    }

    SECTION("Roundtrip MJD conversion") {
        double original_mjd = 58849.0;
        Time t = Time::from_modified_julian_date(original_mjd);
        double converted_mjd = t.to_modified_julian_date();
        
        REQUIRE_THAT(converted_mjd, Catch::Matchers::WithinAbs(original_mjd, 1e-6));
    }

    SECTION("MJD and JD relationship") {
        constexpr double mjd_offset = 2400000.5;
        double mjd = 60000.0;
        
        Time t1 = Time::from_modified_julian_date(mjd);
        Time t2 = Time::from_julian_date(mjd + mjd_offset);
        
        REQUIRE_THAT(t1.et(), Catch::Matchers::WithinAbs(t2.et(), 1e-9));
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

TEST_CASE("Time: Chrono conversions", "[time]") {
    SECTION("Roundtrip system_clock conversion") {
        Time t1 = Time::from_et(86400.0);  // Some arbitrary time
        auto sys_tp = t1.to_system_clock();
        Time t2(sys_tp);
        
        // Should be very close (within 1 millisecond)
        REQUIRE_THAT(t2.et(), Catch::Matchers::WithinAbs(t1.et(), 0.001));
    }

#if HUIRA_HAS_UTC_CLOCK
    SECTION("Roundtrip utc_clock conversion") {
        Time t1 = Time::from_et(172800.0);  // Some arbitrary time
        auto utc_tp = t1.to_utc_clock();
        Time t2(utc_tp);
        
        REQUIRE_THAT(t2.et(), Catch::Matchers::WithinAbs(t1.et(), 0.001));
    }
#endif
}

TEST_CASE("Time: Comparison operators", "[time]") {
    Time t1(1000.0);
    Time t2(2000.0);
    Time t3(1000.0);

    SECTION("Equality") {
        REQUIRE(t1 == t3);
        REQUIRE_FALSE(t1 == t2);
    }

    SECTION("Inequality (derived from equality)") {
        REQUIRE(t1 != t2);
        REQUIRE_FALSE(t1 != t3);
    }

    SECTION("Less than") {
        REQUIRE(t1 < t2);
        REQUIRE_FALSE(t2 < t1);
        REQUIRE_FALSE(t1 < t3);
    }

    SECTION("Other comparison operators (derived)") {
        REQUIRE(t2 > t1);
        REQUIRE(t1 <= t2);
        REQUIRE(t1 <= t3);
        REQUIRE(t2 >= t1);
        REQUIRE(t1 >= t3);
    }
}

TEST_CASE("Time: Arithmetic with duration", "[time]") {
    Time t(1000.0);

    SECTION("Addition with duration") {
        Time t2 = t + std::chrono::duration<double>(500.0);
        REQUIRE_THAT(t2.et(), Catch::Matchers::WithinAbs(1500.0, 1e-9));
    }

    SECTION("Subtraction with duration") {
        Time t2 = t - std::chrono::duration<double>(300.0);
        REQUIRE_THAT(t2.et(), Catch::Matchers::WithinAbs(700.0, 1e-9));
    }

    SECTION("Addition with chrono literals") {
        Time t2 = t + 1h;  // 1 hour = 3600 seconds
        REQUIRE_THAT(t2.et(), Catch::Matchers::WithinAbs(1000.0 + 3600.0, 1e-6));
    }

    SECTION("Subtraction with chrono literals") {
        Time t2 = t + 1min;  // 1 minute = 60 seconds
        REQUIRE_THAT(t2.et(), Catch::Matchers::WithinAbs(1000.0 + 60.0, 1e-6));
    }
}

TEST_CASE("Time: Difference between times", "[time]") {
    Time t1(1000.0);
    Time t2(2500.0);

    SECTION("Positive difference") {
        auto diff = t2 - t1;
        REQUIRE_THAT(diff.count(), Catch::Matchers::WithinAbs(1500.0, 1e-9));
    }

    SECTION("Negative difference") {
        auto diff = t1 - t2;
        REQUIRE_THAT(diff.count(), Catch::Matchers::WithinAbs(-1500.0, 1e-9));
    }

    SECTION("Zero difference") {
        Time t3(1000.0);
        auto diff = t1 - t3;
        REQUIRE_THAT(diff.count(), Catch::Matchers::WithinAbs(0.0, 1e-9));
    }

    SECTION("Convert difference to other units") {
        auto diff = t2 - t1;
        auto diff_minutes = std::chrono::duration_cast<std::chrono::minutes>(diff);
        auto diff_hours = std::chrono::duration_cast<std::chrono::hours>(diff);
        
        REQUIRE(diff_minutes.count() == 25);  // 1500 seconds = 25 minutes
        REQUIRE(diff_hours.count() == 0);     // 1500 seconds = 0 hours (truncated)
    }
}

TEST_CASE("Time: Complex arithmetic scenarios", "[time]") {
    SECTION("Multiple additions and subtractions") {
        Time t = Time::from_et(0.0);
        t = t + 1h + 30min + 45s;
        t = t - 15min;
        
        double expected = 3600.0 + 1800.0 + 45.0 - 900.0;  // 4545 seconds
        REQUIRE_THAT(t.et(), Catch::Matchers::WithinAbs(expected, 1e-6));
    }

    SECTION("Time arithmetic preserves order") {
        Time t1 = Time::from_et(100.0);
        Time t2 = t1 + 50s;
        Time t3 = t2 + 50s;
        
        REQUIRE(t1 < t2);
        REQUIRE(t2 < t3);
        REQUIRE(t1 < t3);
    }

    SECTION("Symmetric operations") {
        Time t = Time::from_et(1000.0);
        auto duration = 500s;
        
        Time t_plus = t + duration;
        Time t_minus = t_plus - duration;
        
        REQUIRE_THAT(t_minus.et(), Catch::Matchers::WithinAbs(t.et(), 1e-9));
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

    SECTION("Zero duration arithmetic") {
        Time t = Time::from_et(1000.0);
        Time t2 = t + 0s;
        REQUIRE(t == t2);
    }
}

TEST_CASE("Time: Real-world scenarios", "[time]") {
    SECTION("Calculate time difference in days") {
        Time t1 = Time("2024-01-01 00:00:00 UTC");
        Time t2 = Time("2024-01-15 00:00:00 UTC");
        
        auto diff = t2 - t1;
        auto days = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<86400>>>(diff);
        
        REQUIRE_THAT(days.count(), Catch::Matchers::WithinAbs(14.0, 0.1));
    }

    SECTION("Schedule future event") {
        Time now = Time::now();
        Time future = now + 24h;  // 24 hours from now
        
        auto diff = future - now;
        REQUIRE_THAT(diff.count(), Catch::Matchers::WithinAbs(86400.0, 1.0));
    }

    SECTION("Historical date") {
        Time apollo11 = Time("1969-07-20 20:17:40 UTC");  // Apollo 11 moon landing
        REQUIRE(apollo11.et() < 0.0);  // Before J2000 epoch
    }

    SECTION("Far future date") {
        Time future = Time("2100-01-01 00:00:00 UTC");
        REQUIRE(future.et() > 0.0);  // After J2000 epoch
    }
}
