#pragma once

#include <chrono>
#include <string>

#include "huira/detail/platform/feature_detection.hpp"

namespace huira {
    class Time {
    public:
        // Calendar (UTC)
        Time(double et) : et_(et) {}
        explicit Time(const std::string& timeString);
        explicit Time(std::chrono::system_clock::time_point system_time);
#if HUIRA_HAS_UTC_CLOCK
        explicit Time(std::chrono::utc_clock::time_point utc_time);
#endif

        // Factory methods for potentially ambiguous formats
        static Time from_et(double et) { return Time(et); }
        static Time from_ephemeris_time(double et) { return Time(et); }
        static Time from_julian_date(double jd);
        static Time from_modified_julian_date(double mjd);

        static Time now() { return Time(std::chrono::system_clock::now()); }

        // Accessors
        double et() const { return et_; }
        double ephemeris_time() const { return et_; }
        double to_julian_date() const;
        double to_modified_julian_date() const;

        std::string to_iso_8601() const;
        std::string to_utc_string(const std::string& format = "YYYY-MM-DD HR:MN:SC.### UTC") const;
        std::chrono::system_clock::time_point to_system_clock() const;
#if HUIRA_HAS_UTC_CLOCK
        std::chrono::utc_clock::time_point to_utc_clock() const;
#endif

        // Comparison
        bool operator==(const Time& other) const;
        bool operator!=(const Time& other) const;
        bool operator<(const Time& other) const;
        bool operator<=(const Time& other) const;
        bool operator>(const Time& other) const;
        bool operator>=(const Time& other) const;

        // Arithmetic (very useful for time-based operations)
        Time operator+(std::chrono::duration<double> dt) const;
        Time operator-(std::chrono::duration<double> dt) const;
        std::chrono::duration<double> operator-(const Time& other) const;

    private:
        double et_ = 0; // Ephemeris Time
    };
}

#include "huira_impl/core/time.ipp"
