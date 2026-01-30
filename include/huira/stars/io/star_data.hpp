#pragma once

#include <limits>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace huira {
    struct StarData {
        double RA;
        double DEC;

        // Default proper motions are zero:
        double pmRA = 0.0;
        double pmDEC = 0.0;

        // Calibratied Spectrophotometric Properties:
        double temperature = std::numeric_limits<double>::quiet_NaN();
        double solid_angle = std::numeric_limits<double>::quiet_NaN();
        double visual_magnitude = std::numeric_limits<double>::quiet_NaN();

        // Process BT and VT magnitudes to compute temperature, solid angle, and visual magnitude
        inline void process_magnitude(double BTmag, double VTmag);

        // Allows sorting by magnitude:
        bool operator<(const StarData& other) const {
            return visual_magnitude < other.visual_magnitude;
        }
    };

    inline void write_star_data(const fs::path& filepath, const std::vector<StarData>& stars);

    inline std::vector<StarData> read_star_data(const fs::path& filepath, double maximum_magnitude);
}

#include "huira_impl/stars/io/star_data.ipp"
