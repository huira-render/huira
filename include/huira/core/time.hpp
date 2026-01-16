#pragma once

#include <chrono>
#include <string>

#include <cspice/SpiceUsr.h>

namespace huira {
    class Time {
    public:
        // Calendar (UTC)
        Time(int year, int month, int day, int hour = 0, int minute = 0, double second = 0.0);
        explicit Time(const std::string& timeString);
        explicit Time(std::chrono::system_clock::time_point system_time);
        explicit Time(std::chrono::utc_clock::time_point utc_time);

        // Factory methods for potentially ambiguous formats
        static Time from_ephemeris_time(double et);
        static Time from_julian_date(double jd);
        static Time from_modified_julian_date(double mjd);
        static Time from_unix_time(double unixSeconds);

        static Time now() { return Time(std::chrono::system_clock::now()); }

        // Accessors
        double et() const { return et_; }
        double ephemeris_time() const { return et_; }
        double to_julian_date() const;
        double to_modified_julian_date() const;

        std::string to_iso_8601() const;
        std::string to_utc_string(const std::string& format = "YYYY-MM-DD HR:MN:SC.### UTC") const;
        std::chrono::system_clock::time_point to_system_clock() const;
        std::chrono::utc_clock::time_point to_utc_clock() const;

        // Comparison
        bool operator==(const Time& other) const;
        bool operator<(const Time& other) const;

        // Arithmetic (very useful for time-based operations)
        Time operator+(std::chrono::duration<double> dt) const;
        Time operator-(std::chrono::duration<double> dt) const;
        std::chrono::duration<double> operator-(const Time& other) const;

    private:
        double et_ = 0; // Ephemeris Time
    };
}

#include "huira_impl/core/time.ipp"
