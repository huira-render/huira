#pragma once

#include <limits>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <string>

namespace fs = std::filesystem;

namespace huira {
    struct StarData {
        // Catalog identifier (interpretation depends on CatalogType in file header)
        std::uint64_t id = 0;

        // RA/DEC in radians:
        double RA;
        double DEC;

        // Proper motions in Milliarcseconds-per-year:
        float pmRA = 0.f;
        float pmDEC = 0.f;

        // Calibratied Spectrophotometric Properties:
        double solid_angle = std::numeric_limits<double>::quiet_NaN();
        float temperature = std::numeric_limits<float>::quiet_NaN();
        float visual_magnitude = std::numeric_limits<float>::quiet_NaN();

        // Process BT and VT magnitudes to compute temperature, solid angle, and visual magnitude
        inline void process_magnitude(double BTmag, double VTmag);

        // Normalize the Epoch to J2000.0 using proper motion and epoch of observation (if provided)
        inline void normalize_epoch(double epochRA, double epochDEC);

        // Allows sorting by magnitude:
        bool operator<(const StarData& other) const {
            return visual_magnitude < other.visual_magnitude;
        }
    };
}

#include "huira_impl/stars/io/star_data.ipp"
