#include "huira/ephemeris/spice.hpp"

namespace huira {

    // Constructors
    inline Time::Time(const std::string& time_string) {
        this->et_ = huira::spice::string_to_et(time_string);
    }



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

}
