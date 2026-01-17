#pragma once

#include <string>

namespace huira {
    class Time {
    public:
        // Calendar (UTC)
        Time(double et) : et_(et) {}
        explicit Time(const std::string& timeString);

        // Factory methods for potentially ambiguous formats
        static Time from_et(double et) { return Time(et); }
        static Time from_ephemeris_time(double et) { return Time(et); }
        static Time from_julian_date(double jd);
        static Time from_modified_julian_date(double mjd);

        // Accessors
        double et() const { return et_; }
        double ephemeris_time() const { return et_; }
        double to_julian_date() const;
        double to_modified_julian_date() const;

        std::string to_iso_8601() const;
        std::string to_utc_string(const std::string& format = "YYYY-MM-DD HR:MN:SC.### UTC") const;

        // Comparison
        bool operator==(const Time& other) const;
        bool operator!=(const Time& other) const;
        bool operator<(const Time& other) const;
        bool operator<=(const Time& other) const;
        bool operator>(const Time& other) const;
        bool operator>=(const Time& other) const;

    private:
        double et_ = 0; // Ephemeris Time
    };
}

#include "huira_impl/core/time.ipp"
