#pragma once

#include <limits>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace huira {
    struct StarData {
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

        // Allows sorting by magnitude:
        bool operator<(const StarData& other) const {
            return visual_magnitude < other.visual_magnitude;
        }
    };

    inline void write_star_data(const fs::path& filepath, const std::vector<StarData>& stars);

    inline std::vector<StarData> read_star_data(const fs::path& filepath, float maximum_magnitude);
}

#include "huira_impl/stars/io/star_data.ipp"
