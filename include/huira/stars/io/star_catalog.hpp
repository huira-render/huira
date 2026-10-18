#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

#include "huira/core/constants.hpp"
#include "huira/stars/io/star_data.hpp"
#include "huira/stars/io/tycho2_id.hpp"

namespace fs = std::filesystem;

namespace huira {

/**
 * @brief The Huira star catalog (.hrsc) file format.
 *
 * A file is a 32-byte header followed by `star_count` records of `record_size` bytes each,
 * sorted by ascending visual magnitude (brightest first). All integers and floating-point
 * values are little-endian; floating-point values are IEEE 754 binary32/binary64.
 *
 * Header layout (byte offsets):
 *
 *     0   char[4]   magic           "HRSC"
 *     4   uint16    format_version  Layout of the file. See HRSC_FORMAT_VERSION.
 *     6   uint16    data_version    How the values were computed. See HRSC_DATA_VERSION.
 *     8   uint8     catalog_type    CatalogType: how `StarData::id` is interpreted.
 *     9   uint8     flags           HRSC_FLAG_* bits; currently only the endianness marker.
 *     10  uint16    record_size     Bytes per star record (sizeof(StarData), 48).
 *     12  uint32    header_size     Bytes before the first record (32).
 *     16  uint64    star_count      Number of records.
 *     24  uint64    reserved        Zero.
 *
 * Record layout (StarData, byte offsets): id uint64 @0, RA double @8, DEC double @16,
 * pmRA float @24, pmDEC float @28, solid_angle double @32, temperature float @40,
 * visual_magnitude float @44. This layout is part of the format: changing StarData means
 * bumping HRSC_FORMAT_VERSION.
 *
 * The two versions change independently:
 *  - `format_version` is bumped when the layout changes (header fields, record layout,
 *    compression). A reader rejects any format it does not know.
 *  - `data_version` is bumped when the catalog builder computes different *values* for the
 *    same layout (a better B-V to temperature relation, say). A reader accepts any
 *    data version and only reports it, so a user can tell which build they have.
 */

/// Bump when the *layout* of an .hrsc file changes.
///   1  (retired) Before the format was versioned, huira <= 0.9.10 wrote the header as the
///               memory image of a struct with a one-byte version of 1 at byte 4, and its
///               reader accepted any file with a 1 there, then took star_count from bytes
///               24-31, which the versioned layout holds as reserved zeros. A versioned file
///               marked 1 would load in those versions as an empty catalog, without error.
///               Starting at 2 makes them reject it instead.
///   2  2026-10  First versioned layout: 32-byte header, 48-byte StarData records.
constexpr std::uint16_t HRSC_FORMAT_VERSION = 2;
static_assert(HRSC_FORMAT_VERSION != 1,
              "Format version 1 is the unversioned pre-release layout; see the note above");

/// Bump when the *values* written by the catalog builder change for the same layout.
///   1  2026-10  Initial release: V = VT - 0.090 (BT - VT); B-V = 0.850 (BT - VT);
///               T_eff from B-V by Ballesteros (2012); solid angle from a Planck spectrum
///               through an approximate Johnson V band; B-V = 0.3 when only one band exists,
///               and is clamped to [-0.4, 2.5] (BV_COLOR_INDEX_MIN/MAX) before the relation.
constexpr std::uint16_t HRSC_DATA_VERSION = 1;

constexpr std::size_t HRSC_HEADER_SIZE = 32;
constexpr char HRSC_MAGIC[4] = {'H', 'R', 'S', 'C'};

/// The values are little-endian (the only endianness written; a reader on a big-endian host
/// must byte-swap). Set in every file so a future big-endian writer could say otherwise.
constexpr std::uint8_t HRSC_FLAG_LITTLE_ENDIAN = 0x01;

enum class CatalogType : std::uint8_t {
    Unknown = 0,
    Tycho2 = 1,
};

struct HrscHeader {
    std::uint16_t format_version = HRSC_FORMAT_VERSION;
    std::uint16_t data_version = HRSC_DATA_VERSION;
    CatalogType catalog_type = CatalogType::Unknown;
    std::uint8_t flags = HRSC_FLAG_LITTLE_ENDIAN;
    std::uint16_t record_size = sizeof(StarData);
    std::uint32_t header_size = HRSC_HEADER_SIZE;
    std::uint64_t star_count = 0;
    std::uint64_t reserved = 0;

    /// Serializes to the on-disk layout documented above.
    inline void write(std::ostream& os) const;

    /// Parses the on-disk layout; throws if the magic is wrong or the stream is short.
    static inline HrscHeader read(std::istream& is, const fs::path& filepath);
};

// The record is written to disk as-is, so its layout must be what the format documents:
static_assert(std::is_trivially_copyable_v<StarData>, "StarData is written to disk as bytes");
static_assert(sizeof(StarData) == 48, "StarData layout is part of the .hrsc format");
static_assert(offsetof(StarData, id) == 0 && offsetof(StarData, RA) == 8 &&
                  offsetof(StarData, DEC) == 16 && offsetof(StarData, pmRA) == 24 &&
                  offsetof(StarData, pmDEC) == 28 && offsetof(StarData, solid_angle) == 32 &&
                  offsetof(StarData, temperature) == 40 &&
                  offsetof(StarData, visual_magnitude) == 44,
              "StarData layout is part of the .hrsc format");
static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
              "The .hrsc format stores IEEE 754 floating-point values");
static_assert(sizeof(float) == 4 && sizeof(double) == 8);

class StarCatalog {
  public:
    StarCatalog() = default;

    StarCatalog(std::vector<StarData> star_data) : star_data_(std::move(star_data)) {}

    void set_catalog_type(CatalogType type) { catalog_type_ = type; }
    CatalogType get_catalog_type() const { return catalog_type_; }

    /// The data version read from the file, or HRSC_DATA_VERSION for a catalog built here.
    std::uint16_t get_data_version() const { return data_version_; }

    /// The format version read from the file, or HRSC_FORMAT_VERSION for a catalog built here.
    std::uint16_t get_format_version() const { return format_version_; }

    void add_star(const StarData& star) { star_data_.push_back(star); }

    const std::vector<StarData>& get_star_data() const { return star_data_; }

    void sort() { std::sort(star_data_.begin(), star_data_.end()); }

    inline void clip_by_magnitude(float maximum_magnitude);

    /// Writes the stars with a magnitude, sorted by magnitude, stamped with the current
    /// HRSC_FORMAT_VERSION and HRSC_DATA_VERSION.
    inline void write_star_data(const fs::path& filepath) const;

    /// Reads the stars no fainter than `maximum_magnitude` from an .hrsc file.
    /// @throws std::runtime_error if the file is not an .hrsc file, has a format version this
    ///         build cannot read, or is truncated.
    static inline StarCatalog
    read_star_data(const fs::path& filepath,
                   float maximum_magnitude = std::numeric_limits<float>::infinity());

    /// Reads only the header of an .hrsc file.
    static inline HrscHeader read_header(const fs::path& filepath);

    inline std::string get_star_id(std::size_t index) const;

    StarData& operator[](std::size_t index) { return star_data_[index]; }
    const StarData& operator[](std::size_t index) const { return star_data_[index]; }

    void print_header(std::ostream& os = std::cout) const
    {
        os << std::left << std::setw(16) << "ID" << " " << std::setw(10) << "RA (deg)" << " "
           << std::setw(10) << "DEC (deg)" << "  " << std::setw(8) << "Temp (K)" << " "
           << std::setw(8) << "Vmag" << " " << std::setw(8) << "Omega (sr)\n";
        os << "----------------------------------------------------------------------\n";
    }

    void print_entry(std::size_t index, std::ostream& os = std::cout) const
    {
        constexpr double rad2deg = 180.0 / PI<double>();

        const StarData& star = star_data_[index];
        os << std::fixed << std::setw(16) << get_star_id(index) << " " << std::setprecision(4)
           << std::setw(10) << star.RA * rad2deg << " " << std::setprecision(4) << std::setw(10)
           << star.DEC * rad2deg << "  " << std::setprecision(2) << std::setw(8) << star.temperature
           << " " << std::setprecision(4) << std::setw(8) << star.visual_magnitude << " "
           << std::scientific << std::setprecision(8) << std::setw(8) << star.solid_angle << "\n";
    }

    using value_type = StarData;
    using iterator = std::vector<StarData>::iterator;
    using const_iterator = std::vector<StarData>::const_iterator;
    using size_type = std::vector<StarData>::size_type;
    using reference = StarData&;
    using const_reference = const StarData&;

    // Iterator methods
    iterator begin() { return star_data_.begin(); }
    iterator end() { return star_data_.end(); }
    const_iterator begin() const { return star_data_.begin(); }
    const_iterator end() const { return star_data_.end(); }
    const_iterator cbegin() const { return star_data_.cbegin(); }
    const_iterator cend() const { return star_data_.cend(); }

    size_type size() const { return star_data_.size(); }
    bool empty() const { return star_data_.empty(); }

  private:
    std::vector<StarData> star_data_;

    CatalogType catalog_type_ = CatalogType::Unknown;
    std::uint16_t format_version_ = HRSC_FORMAT_VERSION;
    std::uint16_t data_version_ = HRSC_DATA_VERSION;
};
} // namespace huira

#include "huira_impl/stars/io/star_catalog.ipp"
