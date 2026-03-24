#pragma once

/// @file ensure_proj_data.hpp
/// @brief Sets PROJ_LIB so GDAL/PROJ can find proj.db.
///
/// Relies on HUIRA_PROJ_DIR being defined at compile time by FindProjDB.cmake.
/// Users of huira do not need to call anything — this is invoked internally
/// before any GDAL operation that needs coordinate reference systems.

#include <cpl_conv.h>

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace huira {

#ifdef _WIN32
    static constexpr char kPathDelim = ';';
#else
    static constexpr char kPathDelim = ':';
#endif


    /// Ensures PROJ can find proj.db. Safe to call repeatedly.
    ///
    /// @param customPaths  Additional directories to prepend to the search path.
    /// @return true if a directory containing proj.db was configured.
    inline bool ensure_proj_data(const std::vector<fs::path>& custom_paths = {})
    {
        static bool done = false;
        if (done) {
            return true;
        }

        std::string result;

        // 1. Check custom paths first.
        for (const auto& p : custom_paths) {
            std::error_code ec;
            if (fs::is_regular_file(p / "proj.db", ec)) {
                result = p.string();
                break;
            }
        }

        // 2. Fall back to the compile-time path from FindProjDB.cmake.
#ifdef HUIRA_PROJ_DIR
        if (result.empty()) {
            std::error_code ec;
            if (fs::is_regular_file(fs::path(HUIRA_PROJ_DIR) / "proj.db", ec)) {
                result = HUIRA_PROJ_DIR;
            }
        }
#endif

        if (result.empty()) return false;

        // Append any additional custom paths.
        for (const auto& p : custom_paths) {
            if (p.string() != result) {
                result += kPathDelim;
                result += p.string();
            }
        }

        CPLSetConfigOption("PROJ_LIB", result.c_str());
        done = true;
        return true;
    }

}
