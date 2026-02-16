
#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/types.hpp"


namespace fs = std::filesystem;


// Forward declaration of huira::Time
namespace huira {
    class Time;
}

/**
 * @brief SPICE ephemeris and time interface for huira.
 *
 * Provides wrappers for CSPICE kernel loading, time conversion, and state/rotation queries.
 * All units are SI unless otherwise noted. Functions are thread-safe and handle SPICE error management.
 */
namespace huira::spice {
    /**
     * @brief Load a SPICE kernel file.
     * @param file_path Path to kernel file
     */
    inline void furnsh(const fs::path& file_path);

    /**
     * @brief Load a SPICE kernel file, resolving relative to its parent directory.
     * @param kernel_path Path to kernel file
     */
    inline void furnsh_relative_to_file(const fs::path& kernel_path);

    // Default SPICE kernel management
    inline std::once_flag lsk_init_flag;
    inline std::atomic<bool> lsk_loaded{ false };

    /**
     * @brief Get the default LSK (leap seconds kernel) path.
     * @return fs::path Path to default LSK
     */
    inline fs::path get_default_lsk_path();

    /**
     * @brief Ensure the default LSK is loaded.
     */
    inline void ensure_lsk_loaded();

    /**
     * @brief Get the default PCK (planetary constants kernel) path.
     * @return fs::path Path to default PCK
     */
    inline fs::path get_default_pck_path();

    /**
     * @brief Load the default PCK.
     */
    inline void load_default_pck();

    // SPICE time interfaces
    /**
     * @brief Convert a time string to ephemeris time (ET, seconds past J2000).
     * @param time_string Time string (e.g., "2000-001T12:00:00")
     * @return double Ephemeris time (seconds past J2000)
     */
    inline double str2et(const std::string& time_string);

    /**
     * @brief Compute delta ET for a given epoch and type.
     * @param epoch Epoch (seconds past J2000)
     * @param eptype Type of delta (e.g., "DELTET")
     * @return double Delta ET
     */
    inline double deltet(double epoch, const std::string& eptype);

    /**
     * @brief Convert an epoch from one time system to another.
     * @param epoch Epoch (seconds)
     * @param insys Input time system
     * @param outsys Output time system
     * @return double Converted epoch
     */
    inline double unitim(double epoch, const std::string& insys, const std::string& outsys);

    /**
     * @brief Format ephemeris time as a string.
     * @param et Ephemeris time (seconds past J2000)
     * @param pictur Output format string
     * @param lenout Output string length
     * @return std::string Formatted time string
     */
    inline std::string timout(double et, const std::string& pictur, int lenout);

    // SPICE state interfaces
    /**
     * @brief Get the state (position, velocity, light time) of a target relative to an observer.
     * @tparam T Floating point type
     * @param TARGET Target body name
     * @param time Observation time
     * @param FRAME Reference frame
     * @param ABCORR Aberration correction
     * @param OBSERVER Observer body name
     * @return std::tuple<Vec3<T>, Vec3<T>, double> {position [m], velocity [m/s], light time [s]}
     */
    template <huira::IsFloatingPoint T>
    inline std::tuple<Vec3<T>, Vec3<T>, double> spkezr(
        const std::string& TARGET,
        const huira::Time& time,
        const std::string& FRAME,
        const std::string& ABCORR,
        const std::string& OBSERVER);

    /**
     * @brief Get the rotation from one frame to another at a given time.
     * @tparam T Floating point type
     * @param FROM Source frame
     * @param TO Destination frame
     * @param time Observation time
     * @return huira::Rotation<T> Rotation object
     */
    template <huira::IsFloatingPoint T>
    inline huira::Rotation<T> pxform(
        const std::string& FROM,
        const std::string& TO,
        const huira::Time& time);

    /**
     * @brief Get the rotation and angular velocity from one frame to another at a given time.
     * @tparam T Floating point type
     * @param FROM Source frame
     * @param TO Destination frame
     * @param time Observation time
     * @return std::pair<huira::Rotation<T>, huira::Vec3<T>> {rotation, angular velocity}
     */
    template <huira::IsFloatingPoint T>
    inline std::pair<huira::Rotation<T>, huira::Vec3<T>> sxform(
        const std::string& FROM,
        const std::string& TO,
        const huira::Time& time
    );
}

#include "huira_impl/ephemeris/spice.ipp"
