#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace huira {
/// The B-V color index is clamped to this range before the temperature is computed from it:
/// about 21,700 K to 2,700 K. Outside it, Tycho-2's colors are photometric noise.
constexpr double BV_COLOR_INDEX_MIN = -0.4;
constexpr double BV_COLOR_INDEX_MAX = 2.5;

// The layout of this struct is the record layout of the .hrsc file (see star_catalog.hpp):
// 48 bytes, every field at its natural alignment, no padding. Reordering, adding or
// changing a field changes the file format.
struct StarData {
    // Catalog identifier (interpretation depends on CatalogType in file header)
    std::uint64_t id = 0;

    // RA/DEC in radians (ICRS, epoch J2000.0):
    double RA = 0.0;
    double DEC = 0.0;

    // Proper motions in Milliarcseconds-per-year:
    float pmRA = 0.f;
    float pmDEC = 0.f;

    // Calibrated Spectrophotometric Properties:
    double solid_angle = std::numeric_limits<double>::quiet_NaN();
    float temperature = std::numeric_limits<float>::quiet_NaN();
    float visual_magnitude = std::numeric_limits<float>::quiet_NaN();

    // Process BT and VT magnitudes to compute temperature, solid angle, and visual magnitude
    inline void process_magnitude(double BTmag, double VTmag);

    // Normalize the Epoch to J2000.0 using proper motion and epoch of observation (if provided)
    inline void normalize_epoch(double epochRA, double epochDEC);

    // Allows sorting by magnitude:
    bool operator<(const StarData& other) const
    {
        return visual_magnitude < other.visual_magnitude;
    }
};
} // namespace huira

#include "huira_impl/stars/io/star_data.ipp"
