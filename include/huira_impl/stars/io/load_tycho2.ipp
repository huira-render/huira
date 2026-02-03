#include <filesystem>
#include <vector>
#include <fstream>
#include <cstddef>
#include <algorithm>
#include <limits>

#include "huira/core/constants.hpp"
#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {
    static std::size_t count_lines_(const fs::path& filepath) {
        std::ifstream file(filepath, std::ios::binary);

        // Count new-lines:
        auto count = std::count(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>(),
            '\n'
        );

        return static_cast<std::size_t>(count);
    }

    static double read_entry_(const std::string& line, std::size_t start, std::size_t end) {
        if (start >= line.size()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        end = std::min(end, line.size());
        std::string field = line.substr(start, end - start);
        std::size_t first = field.find_first_not_of(" \t");
        if (first == std::string::npos) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        std::size_t last = field.find_last_not_of(" \t");
        field = field.substr(first, last - first + 1);
        try {
            return std::stod(field);
        }
        catch (...) {
            return std::numeric_limits<double>::quiet_NaN();
        }
    }

    static bool is_valid_line_(const std::string& line) {
        // Data lines start with a digit (TYC1 number)
        return !line.empty() && std::isdigit(static_cast<unsigned char>(line[0]));
    }

    std::vector<StarData> read_tycho2_dat(const fs::path& filepath)
    {
        std::vector<StarData> star_data(count_lines_(filepath));
        std::ifstream file(filepath);
        if (!file.is_open()) {
            HUIRA_THROW_ERROR("read_tycho2 - Failed to open file: " + filepath.string());
        }

        std::string line;
        std::size_t i = 0;
        while (std::getline(file, line)) {
            if (is_valid_line_(line)) {
                double RA = read_entry_(line, 153, 165);
                double DEC = read_entry_(line, 166, 178);
                if (std::isnan(RA) || std::isnan(DEC)) {
                    // Skip entry if RA/DEC is invalid
                    continue;
                }
                star_data[i].RA = RA * PI<double>() / 180.;
                star_data[i].DEC = DEC * PI<double>() / 180.;

                double pmRA = read_entry_(line, 41, 48);
                double pmDEC = read_entry_(line, 49, 56);
                if (!std::isnan(pmRA)) {
                    star_data[i].pmRA = static_cast<float>(pmRA);
                }
                if (!std::isnan(pmDEC)) {
                    star_data[i].pmDEC = static_cast<float>(pmDEC);
                }

                double BTmag = read_entry_(line, 110, 116);
                double VTmag = read_entry_(line, 123, 129);
                if (std::isnan(BTmag) && std::isnan(VTmag)) {
                    // Skip entry if no magnitude information is given:
                    continue;
                }
                star_data[i].process_magnitude(BTmag, VTmag);

                i++;
            }
        }
        star_data.resize(i);
        return star_data;
    }
}
