#pragma once

/**
 * @file time.hpp
 * @brief Time representation and conversion utilities for astrometric applications.
 *
 * This module provides a Time class that internally stores time as Barycentric
 * Dynamical Time (TDB), consistent with SPICE ephemeris time (ET). All inputs
 * and outputs require explicit specification of the timescale to avoid ambiguity.
 *
 * @note SPICE uses TDB as its fundamental timescale, referring to it as
 *       "Ephemeris Time" (ET). This class follows that convention.
 *
 * ## Timescale Overview
 *
 * | Scale | Description                                              |
 * |-------|----------------------------------------------------------|
 * | UTC   | Coordinated Universal Time (civil time, has leap seconds)|
 * | TAI   | International Atomic Time (continuous SI seconds)        |
 * | TT    | Terrestrial Time (TT = TAI + 32.184s)                    |
 * | TDB   | Barycentric Dynamical Time (≈TT, periodic offset ±1.7ms) |
 *
 * ## Key Relationships
 * ```
 * TT  = TAI + 32.184s       (exact, by definition)
 * TAI = UTC + ΔAT           (ΔAT = cumulative leap seconds)
 * TDB ≈ TT + periodic terms (amplitude ≤1.7ms)
 * ```
 */

#include <string>

#include "huira/core/units/units.hpp"

namespace huira {

/**
 * @brief Enumeration of supported astronomical timescales.
 *
 * Different timescales serve different purposes in astronomy and timekeeping.
 * This enum allows explicit specification of which timescale is being used
 * for input/output operations.
 */
enum class TimeScale {
    UTC, ///< Coordinated Universal Time.
    TAI, ///< International Atomic Time.
    TT,  ///< Terrestrial Time.
    TDB  ///< Barycentric Dynamical Time.
};

/**
 * @brief Represents a moment in time, stored internally as TDB (SPICE ET).
 *
 * The Time class provides a unified interface for working with astronomical
 * time, supporting conversion between multiple timescales and representations
 * (Julian Date, Modified Julian Date, calendar strings).
 *
 * Internally, time is stored as seconds past J2000.0 TDB, which is the
 * convention used by SPICE ("Ephemeris Time" or ET).
 *
 * @note All factory methods and accessors that involve Julian Dates or
 *       calendar representations require explicit specification of the
 *       timescale to prevent silent errors from timescale confusion.
 *
 * ## Example Usage
 * @code
 * // From a UTC date string (common case)
 * Time t1("2024-03-15T12:00:00");
 *
 * // From a Julian Date in TT (e.g., for catalog epoch calculations)
 * Time t2 = Time::from_julian_date(2451545.0, TimeScale::TT);  // J2000.0
 *
 * // Get Julian Date in TT for proper motion calculations
 * double jd_tt = t1.to_julian_date(TimeScale::TT);
 *
 * // Compute years since J2000.0 (in TT, for catalog work)
 * double dt = t1.julian_years_since_j2000(TimeScale::TT);
 * @endcode
 */
class Time {
public:
    explicit Time(const std::string& utc_string);

    static Time from_et(double et);
    static Time from_ephemeris_time(double et);

    static Time from_julian_date(double jd, TimeScale scale);
    static Time from_modified_julian_date(double mjd, TimeScale scale);



    double et() const;
    double ephemeris_time() const;

    double to_julian_date(TimeScale scale = TimeScale::TDB) const;
    double to_modified_julian_date(TimeScale scale = TimeScale::TDB) const;

    double julian_years_since_j2000(TimeScale scale = TimeScale::TT) const;

    std::string to_iso_8601() const;
    std::string to_utc_string(const std::string& format = "YYYY-MM-DD HR:MN:SC.### UTC") const;

    bool operator==(const Time& other) const;
    bool operator!=(const Time& other) const;
    bool operator<(const Time& other) const;
    bool operator<=(const Time& other) const;
    bool operator>(const Time& other) const;
    bool operator>=(const Time& other) const;

    Time operator+(units::Second delta) const;

    /**
     * @brief Julian Date of J2000.0 epoch (2451545.0).
     *
     * Defined as January 1, 2000, 12:00:00 TT.
     */
    static constexpr double J2000_JD = 2451545.0;

    /**
     * @brief Days per Julian year (365.25).
     *
     * Used for proper motion calculations and epoch conversions.
     */
    static constexpr double DAYS_PER_JULIAN_YEAR = 365.25;

    /**
     * @brief Offset between Julian Date and Modified Julian Date.
     *
     * MJD = JD - 2400000.5
     */
    static constexpr double MJD_OFFSET = 2400000.5;

    /**
     * @brief Offset between TAI and TT in seconds.
     *
     * TT = TAI + 32.184s (exact, by definition).
     */
    static constexpr double TT_TAI_OFFSET = 32.184;

private:
    /**
     * @brief Internal time representation: TDB seconds past J2000.0.
     *
     * This is equivalent to SPICE "Ephemeris Time" (ET).
     */
    double et_ = 0.0;

    /**
     * @brief Private constructor from raw ET value.
     *
     * @param et Ephemeris time in seconds past J2000.0 TDB.
     */
    explicit Time(double et) : et_(et) {}
};

}

#include "huira_impl/core/time.ipp"
