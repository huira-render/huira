#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>

#include "huira/core/rotation.hpp"
#include "huira/core/types.hpp"

#include "huira/core/concepts/numeric_concepts.hpp"

namespace fs = std::filesystem;

// Forward declaration of huira::Time
namespace huira {
    class Time;
}

namespace huira::spice {
    // SPICE furnsh interfaces
    inline void furnsh(const fs::path& file_path);
    inline void furnsh_relative_to_file(const fs::path& kernel_path);


    // Default SPICE kernel management
    inline std::once_flag lsk_init_flag;
    inline std::atomic<bool> lsk_loaded{ false };

    inline fs::path get_default_lsk_path();
    inline void ensure_lsk_loaded();
    inline fs::path get_default_pck_path();
    inline void load_default_pck();


    // SPICE time interfaces
    inline double str2et(const std::string& time_string);
    inline double deltet(double epoch, const std::string& eptype);
    inline double unitim(double epoch, const std::string& insys, const std::string& outsys);
    inline std::string timout(double et, const std::string& pictur, int lenout);

    // SPICE state interfaces
    template <huira::IsFloatingPoint T>
    inline std::tuple<Vec3<T>, Vec3<T>, double> spkezr(
        const std::string& TARGET,
        const huira::Time& time,
        const std::string& FRAME,
        const std::string& ABCORR,
        const std::string& OBSERVER);

    template <huira::IsFloatingPoint T>
    inline huira::Rotation<T> pxform(
        const std::string& FROM,
        const std::string& TO,
        const huira::Time& time);

    template <huira::IsFloatingPoint T>
    inline std::pair<huira::Rotation<T>, huira::Vec3<T>> sxform(
        const std::string& FROM,
        const std::string& TO,
        const huira::Time& time
    );
}

#include "huira_impl/ephemeris/spice.ipp"
