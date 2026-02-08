#include <filesystem>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <fstream>

#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {
    inline void StarCatalog::clip_by_magnitude(float maximum_magnitude) {
        star_data_.erase(
            std::remove_if(star_data_.begin(), star_data_.end(),
                [maximum_magnitude](const StarData& s) {
                    return s.visual_magnitude > maximum_magnitude || std::isnan(s.visual_magnitude);
                }),
            star_data_.end());
    }

    inline void StarCatalog::write_star_data(const fs::path& filepath)
    {
        // Create a mutable copy, removing any stars with NaN visual_magnitude
        std::vector<StarData> valid_stars;
        valid_stars.reserve(star_data_.size());
        std::copy_if(star_data_.begin(), star_data_.end(), std::back_inserter(valid_stars),
            [](const StarData& s) { return !std::isnan(s.visual_magnitude); });

        // Sort by visual_magnitude (brightest first = lowest magnitude)
        std::sort(valid_stars.begin(), valid_stars.end());

        std::ofstream out(filepath, std::ios::binary);
        if (!out) {
            HUIRA_THROW_ERROR("StarCatalog - Failed to open file for writing: " + filepath.string());
        }

        // Write header
        HrscHeader header{};
        header.magic[0] = 'H';
        header.magic[1] = 'R';
        header.magic[2] = 'S';
        header.magic[3] = 'C';
        header.version = 1; // Current HRSC version
        header.reserved = 0;
        header.star_count = valid_stars.size();
        header.catalog_type = catalog_type_;
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // Write all star data
        out.write(reinterpret_cast<const char*>(valid_stars.data()),
            static_cast<std::streamsize>(valid_stars.size() * sizeof(StarData)));

        HUIRA_LOG_INFO("StarCatalog - " + std::to_string(valid_stars.size()) + " stars written to: " + filepath.string());
    }

    StarCatalog StarCatalog::read_star_data(const fs::path& filepath, float maximum_magnitude)
    {
        std::ifstream in(filepath, std::ios::binary);
        if (!in) {
            HUIRA_THROW_ERROR("StarCatalog - Failed to open file for reading: " + filepath.string());
        }

        // Read and validate header
        HrscHeader header{};
        in.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.magic[0] != 'H' || header.magic[1] != 'R' ||
            header.magic[2] != 'S' || header.magic[3] != 'C') {
            HUIRA_THROW_ERROR("StarData - Invalid file format: " + filepath.string());
        }

        if (header.version != 1) {
            auto name = filepath.filename().string();
            HUIRA_THROW_ERROR("StarCatalog - " + name + " is out of date (version = " +
                std::to_string(static_cast<unsigned int>(header.version)) + ").  Please re-generate.");
        }

        // Binary search on file to find cutoff index
        std::uint64_t low = 0, high = header.star_count;
        StarData temp;

        while (low < high) {
            std::uint64_t mid = low + (high - low) / 2;
            in.seekg(static_cast<std::streamoff>(sizeof(HrscHeader) + mid * sizeof(StarData)));
            in.read(reinterpret_cast<char*>(&temp), sizeof(StarData));

            if (temp.visual_magnitude <= maximum_magnitude) {
                low = mid + 1;
            }
            else {
                high = mid;
            }
        }

        // Read only the stars we need
        std::vector<StarData> stars(low);
        in.seekg(static_cast<std::streamoff>(sizeof(HrscHeader)));
        in.read(reinterpret_cast<char*>(stars.data()),
            static_cast<std::streamsize>(low * sizeof(StarData)));

        StarCatalog star_catalog(stars);
        star_catalog.set_catalog_type(header.catalog_type);

        HUIRA_LOG_INFO("StarCatalog - " + std::to_string(stars.size()) + " stars read from: " + filepath.string());

        return star_catalog;
    }

    inline std::string StarCatalog::get_star_id(std::size_t index) const {
        if (index >= star_data_.size()) {
            HUIRA_THROW_ERROR("StarCatalog::get_star_id - Index out of bounds: " + std::to_string(index) + " (has " + std::to_string(star_data_.size()) + " stars)");
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
}
