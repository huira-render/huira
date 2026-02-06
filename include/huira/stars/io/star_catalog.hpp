#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <limits>
#include <filesystem>
#include <cstdint>

#include "huira/stars/io/star_data.hpp"
#include "huira/stars/io/tycho2_id.hpp"

namespace fs = std::filesystem;

namespace huira {
    enum class CatalogType : std::uint8_t {
        Unknown = 0,
        Tycho2 = 1,
    };

    struct HrscHeader {
        char magic[4]; // "HRSC"
        std::uint8_t version;
        std::uint64_t reserved;
        CatalogType catalog_type;
        std::uint64_t star_count;
    };

    class StarCatalog {
    public:
        StarCatalog() = default;

        StarCatalog(std::vector<StarData> star_data)
            : star_data_(std::move(star_data)) {}

        void set_catalog_type(CatalogType type) { catalog_type_ = type; }

        void add_star(const StarData& star) { star_data_.push_back(star); }

        const std::vector<StarData>& get_star_data() const { return star_data_; }

        void sort() { std::sort(star_data_.begin(), star_data_.end()); }

        inline void clip_by_magnitude(float maximum_magnitude);

        inline void write_star_data(const fs::path& filepath);

        static StarCatalog read_star_data(const fs::path& filepath, float maximum_magnitude = std::numeric_limits<float>::infinity());

        inline std::string get_star_id(std::size_t index) const;


        StarData& operator[](std::size_t index) {
            return star_data_[index];
        }
        const StarData& operator[](std::size_t index) const {
            return star_data_[index];
        }

        void print_header(std::ostream& os = std::cout) const {
            os << std::left
                << std::setw(16) << "ID" << " "
                << std::setw(10) << "RA (deg)" << " "
                << std::setw(10) << "DEC (deg)" << "  "
                << std::setw(8) << "Temp (K)" << " "
                << std::setw(8) << "Vmag" << " "
                << std::setw(8) << "Omega (sr)\n";
            os << "----------------------------------------------------------------------\n";
        }

        void print_entry(std::size_t index, std::ostream& os = std::cout) const {
            constexpr double rad2deg = 180.0 / PI<double>();

            StarData star = star_data_[index];
            os << std::fixed
                << std::setw(16) << get_star_id(index) << " "
                << std::setprecision(4) << std::setw(10) << star.RA * rad2deg << " "
                << std::setprecision(4) << std::setw(10) << star.DEC * rad2deg << "  "
                << std::setprecision(2) << std::setw(8) << star.temperature << " "
                << std::setprecision(4) << std::setw(8) << star.visual_magnitude << " "
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
    };
}

#include "huira_impl/stars/io/star_catalog.ipp"
