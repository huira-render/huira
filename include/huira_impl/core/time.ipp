#include "huira/ephemeris/spice.hpp"

namespace huira {
    namespace detail {
        inline constexpr double SECONDS_PER_DAY = 86400.0;
        inline constexpr double SECONDS_PER_JULIAN_YEAR = 365.25 * SECONDS_PER_DAY;
    }

    /**
     * @brief Constructs a Time from a UTC date-time string.
     *
     * Parses a variety of date-time string formats and converts to internal
     * TDB representation. The input is assumed to be in UTC.
     *
     * @param utc_string A date-time string in a format recognized by SPICE,
     *                   such as:
     *                   - "2024-03-15T12:00:00"
     *                   - "2024-03-15 12:00:00"
     *                   - "March 15, 2024 12:00:00"
     *                   - "2024-MAR-15 12:00:00.000"
     *
     * @throws std::runtime_error if the string cannot be parsed.
     *
     * @note This constructor assumes UTC. For other timescales, use the
     *       factory methods with explicit TimeScale specification.
     */
    inline Time::Time(const std::string& utc_string) {
        // SPICE str2et interprets strings as UTC by default and returns TDB
        et_ = spice::str2et(utc_string);
    }
    
    /**
     * @brief Constructs a Time from SPICE ephemeris time (TDB seconds past J2000).
     *
     * This is the most direct construction method, as ET is the internal
     * representation.
     *
     * @param et Ephemeris time in seconds past J2000.0 TDB.
     * @return Time object representing the specified moment.
     */
    inline Time Time::from_et(double et) {
        return Time(et);
    }
    
    /// @copydoc from_et
    /// @note Alias for from_et() for clarity in code that uses the full name.
    inline Time Time::from_ephemeris_time(double et) {
        return Time(et);
    }

    /**
     * @brief Constructs a Time from a Julian Date in the specified timescale.
     *
     * @param jd Julian Date value.
     * @param scale The timescale of the input Julian Date.
     * @return Time object representing the specified moment.
     *
     * @note J2000.0 is defined as JD 2451545.0 TT (not TDB), though the
     *       difference is sub-millisecond.
     *
     * ## Example
     * @code
     * // J2000.0 epoch (January 1, 2000, 12:00:00 TT)
     * Time j2000 = Time::from_julian_date(2451545.0, TimeScale::TT);
     * @endcode
     */
    inline Time Time::from_julian_date(double jd, TimeScale scale) {
        // Convert JD to seconds past J2000 in the input timescale
        double seconds_past_j2000 = (jd - J2000_JD) * detail::SECONDS_PER_DAY;
    
        switch (scale) {
            case TimeScale::TDB:
                // Already in TDB, this is our internal representation
                return Time(seconds_past_j2000);
    
            case TimeScale::TT:
                // Use SPICE unitim to convert TT -> TDB
                return Time(spice::unitim(seconds_past_j2000, "TT", "TDB"));
    
            case TimeScale::TAI:
                // Use SPICE unitim to convert TAI -> TDB
                return Time(spice::unitim(seconds_past_j2000, "TAI", "TDB"));
    
            case TimeScale::UTC: {
                // For UTC input, we use SPICE's deltet to get ΔET = TDB - UTC
                // deltet with "UTC" type gives us the offset to add to UTC to get ET
                double delta_et = spice::deltet(seconds_past_j2000, "UTC");
                return Time(seconds_past_j2000 + delta_et);
            }
    
            default:
                return Time(seconds_past_j2000);
        }
    }

    /**
     * @brief Constructs a Time from a Modified Julian Date in the specified timescale.
     *
     * Modified Julian Date is defined as MJD = JD - 2400000.5, shifting the
     * epoch to midnight of November 17, 1858.
     *
     * @param mjd Modified Julian Date value.
     * @param scale The timescale of the input MJD.
     * @return Time object representing the specified moment.
     */
    inline Time Time::from_modified_julian_date(double mjd, TimeScale scale) {
        return from_julian_date(mjd + MJD_OFFSET, scale);
    }
    

    /**
     * @brief Returns the time as SPICE ephemeris time (TDB seconds past J2000).
     * @return Ephemeris time in seconds.
     */
    inline double Time::et() const {
        return et_;
    }

    /// @copydoc et
    inline double Time::ephemeris_time() const {
        return et_;
    }
    
    /**
     * @brief Returns the time as a Julian Date in the specified timescale.
     *
     * @param scale The desired output timescale (default: TDB).
     * @return Julian Date in the specified timescale.
     *
     * @note For stellar catalog work (e.g., proper motion from J2000.0),
     *       use TimeScale::TT since catalog epochs are defined in TT.
     */
    inline double Time::to_julian_date(TimeScale scale) const {
        double seconds_past_j2000;
    
        switch (scale) {
            case TimeScale::TDB:
                seconds_past_j2000 = et_;
                break;
    
            case TimeScale::TT:
                seconds_past_j2000 = spice::unitim(et_, "TDB", "TT");
                break;
    
            case TimeScale::TAI:
                seconds_past_j2000 = spice::unitim(et_, "TDB", "TAI");
                break;
    
            case TimeScale::UTC: {
                // For UTC output, use deltet to get ΔET = TDB - UTC
                // deltet with "ET" type takes ET input and gives ΔET
                double delta_et = spice::deltet(et_, "ET");
                seconds_past_j2000 = et_ - delta_et;
                break;
            }
    
            default:
                seconds_past_j2000 = et_;
                break;
        }
    
        return J2000_JD + (seconds_past_j2000 / detail::SECONDS_PER_DAY);
    }

    /**
     * @brief Returns the time as a Modified Julian Date in the specified timescale.
     *
     * @param scale The desired output timescale (default: TDB).
     * @return Modified Julian Date (JD - 2400000.5) in the specified timescale.
     */
    inline double Time::to_modified_julian_date(TimeScale scale) const {
        return to_julian_date(scale) - MJD_OFFSET;
    }
    
    /**
     * @brief Returns the elapsed Julian years since J2000.0.
     *
     * Computes (JD - 2451545.0) / 365.25 in the specified timescale.
     * This is the standard formula for proper motion calculations.
     *
     * @param scale The timescale for the calculation (default: TT).
     * @return Elapsed time in Julian years (365.25 days each).
     *
     * @note For stellar proper motion calculations, use TimeScale::TT
     *       since J2000.0 and catalog positions are defined in TT.
     *
     * ## Example
     * @code
     * Time obs_time("2024-06-15T00:00:00");
     * double dt = obs_time.julian_years_since_j2000(TimeScale::TT);
     * // dt ≈ 24.45 years
     *
     * // Apply proper motion: new_ra = ra_j2000 + pm_ra * dt
     * @endcode
     */
    inline double Time::julian_years_since_j2000(TimeScale scale) const {
        double seconds_past_j2000;
    
        switch (scale) {
            case TimeScale::TDB:
                seconds_past_j2000 = et_;
                break;
    
            case TimeScale::TT:
                seconds_past_j2000 = spice::unitim(et_, "TDB", "TT");
                break;
    
            case TimeScale::TAI:
                seconds_past_j2000 = spice::unitim(et_, "TDB", "TAI");
                break;
    
            case TimeScale::UTC: {
                double delta_et = spice::deltet(et_, "ET");
                seconds_past_j2000 = et_ - delta_et;
                break;
            }
    
            default:
                seconds_past_j2000 = et_;
                break;
        }
    
        return seconds_past_j2000 / detail::SECONDS_PER_JULIAN_YEAR;
    }
    
    /**
     * @brief Returns the time as an ISO 8601 formatted UTC string.
     *
     * @return String in the format "YYYY-MM-DDTHH:MM:SS.sssZ".
     *
     * @note The trailing 'Z' indicates UTC (Zulu time).
     */
    inline std::string Time::to_iso_8601() const {
        return spice::timout(et_, "YYYY-MM-DDTHR:MN:SC.### ::RND ::UTC", 128) + "Z";
    }

    /**
     * @brief Returns the time as a formatted UTC string.
     *
     * @param format SPICE-compatible format string. Common tokens:
     *               - YYYY: 4-digit year
     *               - MM: 2-digit month
     *               - DD: 2-digit day
     *               - HR: 2-digit hour
     *               - MN: 2-digit minute
     *               - SC: 2-digit second
     *               - ###: milliseconds
     * @return Formatted time string in UTC.
     *
     * @note Output is always in UTC regardless of format string content.
     */
    inline std::string Time::to_utc_string(const std::string& format) const {
        std::string spice_format = format;
        if (format.find("::UTC") == std::string::npos) {
            spice_format += " ::UTC";
        }
        return spice::timout(et_, spice_format, 128);
    }
    

    /// @name Comparison Operators
    /// @brief Compare two Time objects by their internal TDB representation.
    /// @{
    inline bool Time::operator==(const Time& other) const {
        return et_ == other.et_;
    }
    
    inline bool Time::operator!=(const Time& other) const {
        return !(*this == other);
    }
    
    inline bool Time::operator<(const Time& other) const {
        return et_ < other.et_;
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
    /// @}
}
