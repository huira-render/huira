#pragma once

#include <limits>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <string>

#include "huira/stars/io/catalog_types.hpp"

namespace fs = std::filesystem;

namespace huira {
    struct HrscHeader {
        char magic[4]; // "HRSC"
        uint8_t version;
        uint64_t reserved;
        uint64_t star_count;
    };

    struct StarData {
        // Catalog identifier (interpretation depends on CatalogType in file header)
        std::uint64_t id = 0;
        CatalogType catalog = CatalogType::Unknown;

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

        inline void normalize_epoch(double epochRA, double epochDEC);

        // Allows sorting by magnitude:
        bool operator<(const StarData& other) const {
            return visual_magnitude < other.visual_magnitude;
        }

        inline std::string format_star_id() {
            switch (catalog) {
            case CatalogType::Tycho2:
                return format_tycho2_id(id);
            default:
                return std::to_string(id);
            }
        }
    };

    inline void write_star_data(const fs::path& filepath, const std::vector<StarData>& stars);

    inline std::vector<StarData> read_star_data(const fs::path& filepath, float maximum_magnitude = std::numeric_limits<float>::infinity());
}

#include "huira_impl/stars/io/star_data.ipp"
