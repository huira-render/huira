#include "huira/core/time.hpp"
#include "huira/spice/spice_time.hpp"
#include <cstdio>

namespace huira {

    // Constructors
    inline Time::Time(const std::string& time_string) {
        this->et_ = huira::spice::string_to_et(time_string);
    }

    inline Time::Time(std::chrono::system_clock::time_point system_time) {
#if HUIRA_HAS_UTC_CLOCK
        *this = Time(std::chrono::utc_clock::from_sys(system_time));
#else
        // Fallback: Convert system_clock directly to string and let SPICE handle it
        auto sys_days = std::chrono::floor<std::chrono::days>(system_time);
        std::chrono::year_month_day ymd{ sys_days };
        auto time_of_day = system_time - sys_days;

        auto hours = std::chrono::duration_cast<std::chrono::hours>(time_of_day);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(time_of_day - hours);
        auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(time_of_day - hours - minutes);

        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02u %02d:%02d:%012.9f UTC",
            static_cast<int>(ymd.year()),
            static_cast<unsigned>(ymd.month()),
            static_cast<unsigned>(ymd.day()),
            static_cast<int>(hours.count()),
            static_cast<int>(minutes.count()),
            seconds.count());

        this->et_ = huira::spice::string_to_et(buffer);
#endif
    }

#if HUIRA_HAS_UTC_CLOCK
    inline Time::Time(std::chrono::utc_clock::time_point utc_time) {
        auto sys_time = std::chrono::utc_clock::to_sys(utc_time);

        std::chrono::year_month_day ymd{ std::chrono::floor<std::chrono::days>(sys_time) };
        auto time_of_day = sys_time - std::chrono::floor<std::chrono::days>(sys_time);

        auto hours = std::chrono::duration_cast<std::chrono::hours>(time_of_day);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(time_of_day - hours);
        auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(time_of_day - hours - minutes);

        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02u %02d:%02d:%012.9f UTC",
            static_cast<int>(ymd.year()),
            static_cast<unsigned>(ymd.month()),
            static_cast<unsigned>(ymd.day()),
            static_cast<int>(hours.count()),
            static_cast<int>(minutes.count()),
            seconds.count());

        this->et_ = huira::spice::string_to_et(buffer);
    }
#endif



    // Static Factory methods
    inline Time Time::from_julian_date(double jd) {
        double et = huira::spice::julian_date_to_et(jd);
        return Time(et);
    }

    inline Time Time::from_modified_julian_date(double mjd) {
        constexpr double mjd_offset = 2400000.5;
        return from_julian_date(mjd + mjd_offset);
    }



    // Accessors
    inline double Time::to_julian_date() const {
        return huira::spice::et_to_julian_date(this->et_);
    }

    inline double Time::to_modified_julian_date() const {
        constexpr double mjd_offset = 2400000.5;
        return to_julian_date() - mjd_offset;
    }

    inline std::string Time::to_iso_8601() const {
        return huira::spice::et_to_string(this->et_, "YYYY-MM-DDTHR:MN:SC.### ::RND ::UTC") + "Z";
    }

    inline std::string Time::to_utc_string(const std::string& format) const {
        std::string spice_format = format;
        if (format.find("::UTC") == std::string::npos) {
            spice_format += " ::UTC";
        }
        return huira::spice::et_to_string(this->et_, spice_format);
    }

    inline std::chrono::system_clock::time_point Time::to_system_clock() const {
        std::string iso = huira::spice::et_to_string(this->et_, "YYYY-MM-DDTHR:MN:SC.#########::UTC::RND");

        int year, month, day, hour, minute;
        double second;
        
#ifdef _MSC_VER
        sscanf_s(iso.c_str(), "%d-%d-%dT%d:%d:%lf", &year, &month, &day, &hour, &minute, &second);
#else
        std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%lf", &year, &month, &day, &hour, &minute, &second);
#endif

        auto ymd = std::chrono::year{ year } / std::chrono::month{ static_cast<unsigned>(month) }
        / std::chrono::day{ static_cast<unsigned>(day) };
        auto sys_days = std::chrono::sys_days{ ymd };

        auto time_of_day = std::chrono::hours{ hour }
            + std::chrono::minutes{ minute }
        + std::chrono::duration<double>{second};

        return sys_days + std::chrono::duration_cast<std::chrono::system_clock::duration>(time_of_day);
    }

#if HUIRA_HAS_UTC_CLOCK
    inline std::chrono::utc_clock::time_point Time::to_utc_clock() const {
        return std::chrono::utc_clock::from_sys(to_system_clock());
    }
#endif



    // Comparison operators
    inline bool Time::operator==(const Time& other) const {
        return this->et_ == other.et_;
    }

    inline bool Time::operator!=(const Time& other) const {
        return !(*this == other);
    }

    inline bool Time::operator<(const Time& other) const {
        return this->et_ < other.et_;
    }

    inline bool Time::operator<=(const Time& other) const {
        return !(other < *this);
    }

    inline bool Time::operator>(const Time& other) const {
        return other < *this;
    }

    inline bool Time::operator>=(const Time& other) const {
        return !(*this < other);
    }

    // Arithmetic operators

    inline Time Time::operator+(std::chrono::duration<double> dt) const {
        return Time(this->et_ + dt.count());
    }

    inline Time Time::operator-(std::chrono::duration<double> dt) const {
        return Time(this->et_ - dt.count());
    }

    inline std::chrono::duration<double> Time::operator-(const Time& other) const {
        return std::chrono::duration<double>(this->et_ - other.et_);
    }

}
