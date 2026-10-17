#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <istream>
#include <iterator>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {

namespace hrsc_detail {

// The file is little-endian. On a little-endian host, a record's bytes are its memory
// image; on a big-endian host, every field would need swapping. No supported platform is
// big-endian, so the reader refuses rather than carrying untested swap code.
constexpr bool host_is_little_endian = std::endian::native == std::endian::little;

template <typename T>
void put(std::array<char, HRSC_HEADER_SIZE>& buffer, std::size_t offset, T value)
{
    static_assert(std::is_unsigned_v<T>);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        buffer[offset + i] = static_cast<char>((value >> (8 * i)) & 0xFF);
    }
}

template <typename T>
T get(const std::array<char, HRSC_HEADER_SIZE>& buffer, std::size_t offset)
{
    static_assert(std::is_unsigned_v<T>);
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value |= static_cast<T>(static_cast<T>(static_cast<unsigned char>(buffer[offset + i]))
                                << (8 * i));
    }
    return value;
}

} // namespace hrsc_detail

inline void HrscHeader::write(std::ostream& os) const
{
    std::array<char, HRSC_HEADER_SIZE> buffer{};
    std::memcpy(buffer.data(), HRSC_MAGIC, 4);
    hrsc_detail::put(buffer, 4, format_version);
    hrsc_detail::put(buffer, 6, data_version);
    hrsc_detail::put(buffer, 8, static_cast<std::uint8_t>(catalog_type));
    hrsc_detail::put(buffer, 9, flags);
    hrsc_detail::put(buffer, 10, record_size);
    hrsc_detail::put(buffer, 12, header_size);
    hrsc_detail::put(buffer, 16, star_count);
    hrsc_detail::put(buffer, 24, reserved);
    os.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
}

inline HrscHeader HrscHeader::read(std::istream& is, const fs::path& filepath)
{
    std::array<char, HRSC_HEADER_SIZE> buffer{};
    is.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (!is || std::memcmp(buffer.data(), HRSC_MAGIC, 4) != 0) {
        HUIRA_THROW_ERROR("StarCatalog - Not a Huira star catalog (.hrsc) file: " +
                          filepath.string());
    }

    HrscHeader header;
    header.format_version = hrsc_detail::get<std::uint16_t>(buffer, 4);
    header.data_version = hrsc_detail::get<std::uint16_t>(buffer, 6);
    header.catalog_type = static_cast<CatalogType>(hrsc_detail::get<std::uint8_t>(buffer, 8));
    header.flags = hrsc_detail::get<std::uint8_t>(buffer, 9);
    header.record_size = hrsc_detail::get<std::uint16_t>(buffer, 10);
    header.header_size = hrsc_detail::get<std::uint32_t>(buffer, 12);
    header.star_count = hrsc_detail::get<std::uint64_t>(buffer, 16);
    header.reserved = hrsc_detail::get<std::uint64_t>(buffer, 24);
    return header;
}

inline void StarCatalog::clip_by_magnitude(float maximum_magnitude)
{
    star_data_.erase(std::remove_if(star_data_.begin(),
                                    star_data_.end(),
                                    [maximum_magnitude](const StarData& s) {
                                        return s.visual_magnitude > maximum_magnitude ||
                                               std::isnan(s.visual_magnitude);
                                    }),
                     star_data_.end());
}

inline void StarCatalog::write_star_data(const fs::path& filepath) const
{
    static_assert(hrsc_detail::host_is_little_endian,
                  "The .hrsc writer stores records as their memory image, which is only the "
                  "little-endian file layout on a little-endian host");

    // Keep the stars with a magnitude, sorted brightest first; the reader's magnitude cutoff
    // relies on this order.
    std::vector<StarData> valid_stars;
    valid_stars.reserve(star_data_.size());
    std::copy_if(star_data_.begin(),
                 star_data_.end(),
                 std::back_inserter(valid_stars),
                 [](const StarData& s) { return !std::isnan(s.visual_magnitude); });
    std::sort(valid_stars.begin(), valid_stars.end());

    std::ofstream out(filepath, std::ios::binary);
    if (!out) {
        HUIRA_THROW_ERROR("StarCatalog - Failed to open file for writing: " + filepath.string());
    }

    HrscHeader header;
    header.catalog_type = catalog_type_;
    header.star_count = valid_stars.size();
    header.write(out);

    out.write(reinterpret_cast<const char*>(valid_stars.data()),
              static_cast<std::streamsize>(valid_stars.size() * sizeof(StarData)));
    if (!out) {
        HUIRA_THROW_ERROR("StarCatalog - Failed to write the stars to: " + filepath.string());
    }

    HUIRA_LOG_INFO("StarCatalog - " + std::to_string(valid_stars.size()) +
                   " stars written to: " + filepath.string() + " (format " +
                   std::to_string(static_cast<unsigned>(header.format_version)) +
                   ", data version " + std::to_string(static_cast<unsigned>(header.data_version)) +
                   ")");
}

inline HrscHeader StarCatalog::read_header(const fs::path& filepath)
{
    std::ifstream in(filepath, std::ios::binary);
    if (!in) {
        HUIRA_THROW_ERROR("StarCatalog - Failed to open file for reading: " + filepath.string());
    }
    return HrscHeader::read(in, filepath);
}

inline StarCatalog StarCatalog::read_star_data(const fs::path& filepath, float maximum_magnitude)
{
    if (std::isnan(maximum_magnitude)) {
        HUIRA_THROW_ERROR("StarCatalog - The maximum magnitude must not be NaN, which would load "
                          "no stars");
    }

    std::ifstream in(filepath, std::ios::binary);
    if (!in) {
        HUIRA_THROW_ERROR("StarCatalog - Failed to open file for reading: " + filepath.string());
    }

    const HrscHeader header = HrscHeader::read(in, filepath);
    const std::string name = filepath.filename().string();

    // The layout must be one this build reads. The message says which side is behind:
    if (header.format_version > HRSC_FORMAT_VERSION) {
        HUIRA_THROW_ERROR("StarCatalog - " + name + " uses star catalog format " +
                          std::to_string(static_cast<unsigned>(header.format_version)) +
                          ", newer than this build of "
                          "huira reads (format " +
                          std::to_string(static_cast<unsigned>(HRSC_FORMAT_VERSION)) +
                          "). Please update huira.");
    }
    if (header.format_version < HRSC_FORMAT_VERSION) {
        HUIRA_THROW_ERROR("StarCatalog - " + name + " uses star catalog format " +
                          std::to_string(static_cast<unsigned>(header.format_version)) +
                          ", which this build of huira "
                          "no longer reads (format " +
                          std::to_string(static_cast<unsigned>(HRSC_FORMAT_VERSION)) +
                          "). Please download or build a current catalog.");
    }
    if (!(header.flags & HRSC_FLAG_LITTLE_ENDIAN) || !hrsc_detail::host_is_little_endian) {
        HUIRA_THROW_ERROR("StarCatalog - " + name +
                          " is not a little-endian catalog read on a "
                          "little-endian host, the only combination huira supports");
    }
    if (header.record_size != sizeof(StarData)) {
        HUIRA_THROW_ERROR("StarCatalog - " + name + " has " +
                          std::to_string(static_cast<unsigned>(header.record_size)) +
                          "-byte star records; format " +
                          std::to_string(static_cast<unsigned>(HRSC_FORMAT_VERSION)) + " expects " +
                          std::to_string(sizeof(StarData)));
    }
    if (header.header_size < HRSC_HEADER_SIZE) {
        HUIRA_THROW_ERROR("StarCatalog - " + name + " declares a " +
                          std::to_string(header.header_size) + "-byte header, shorter than the " +
                          std::to_string(HRSC_HEADER_SIZE) + " bytes format " +
                          std::to_string(static_cast<unsigned>(HRSC_FORMAT_VERSION)) + " defines");
    }

    // The stars it claims must all be there, or the search below reads past the end:
    const std::uintmax_t size = fs::file_size(filepath);
    const std::uint64_t data_offset = header.header_size;
    if (size < data_offset || header.star_count > (size - data_offset) / sizeof(StarData)) {
        HUIRA_THROW_ERROR("StarCatalog - " + filepath.string() +
                          " is truncated: its header lists " + std::to_string(header.star_count) +
                          " stars, more than it holds");
    }

    auto record_offset = [data_offset](std::uint64_t index) {
        return static_cast<std::streamoff>(data_offset + index * sizeof(StarData));
    };

    // The records are sorted by magnitude, so the cutoff is a binary search on the file:
    std::uint64_t low = 0, high = header.star_count;
    StarData temp;
    while (low < high) {
        std::uint64_t mid = low + (high - low) / 2;
        in.seekg(record_offset(mid));
        in.read(reinterpret_cast<char*>(&temp), sizeof(StarData));
        if (!in) {
            HUIRA_THROW_ERROR("StarCatalog - Failed to read the stars from: " + filepath.string());
        }

        if (temp.visual_magnitude <= maximum_magnitude) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    // Read only the stars we need
    std::vector<StarData> stars(low);
    in.seekg(record_offset(0));
    in.read(reinterpret_cast<char*>(stars.data()),
            static_cast<std::streamsize>(low * sizeof(StarData)));
    if (!in) {
        HUIRA_THROW_ERROR("StarCatalog - Failed to read the stars from: " + filepath.string());
    }

    StarCatalog star_catalog(std::move(stars));
    star_catalog.catalog_type_ = header.catalog_type;
    star_catalog.format_version_ = header.format_version;
    star_catalog.data_version_ = header.data_version;

    HUIRA_LOG_INFO("StarCatalog - " + std::to_string(star_catalog.size()) +
                   " stars read from: " + filepath.string() + " (format " +
                   std::to_string(static_cast<unsigned>(header.format_version)) +
                   ", data version " + std::to_string(static_cast<unsigned>(header.data_version)) +
                   ")");

    return star_catalog;
}

inline std::string StarCatalog::get_star_id(std::size_t index) const
{
    if (index >= star_data_.size()) {
        HUIRA_THROW_ERROR(
            "StarCatalog::get_star_id - Index out of bounds: " + std::to_string(index) + " (has " +
            std::to_string(star_data_.size()) + " stars)");
    }

    switch (catalog_type_) {
    case CatalogType::Tycho2:
        return format_tycho2_id(star_data_[index].id);

    case CatalogType::Unknown:
        return std::to_string(star_data_[index].id);

    default:
        return std::to_string(star_data_[index].id);
    }
}
} // namespace huira
