#include <filesystem>
#include <vector>
#include <fstream>
#include <cstddef>
#include <algorithm>
#include <limits>

#include "huira/util/logger.hpp"
#include "huira/stars/io/star_data.hpp"
#include "huira/stars/io/tycho2_id.hpp"

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

    static bool has_mean_position_(const std::string& line) {
        if (line.size() < 14) {
            return false;
        }
        return line[13] != 'X';
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
            HUIRA_THROW_ERROR("read_tycho2_dat - Failed to open file: " + filepath.string());
        }

        std::string line;
        std::size_t i = 0;
        while (std::getline(file, line)) {
            if (is_valid_line_(line)) {
                // Read identifer:
                int tyc1 = static_cast<int>(read_entry_(line, 0, 4));
                int tyc2 = static_cast<int>(read_entry_(line, 5, 10));
                int tyc3 = static_cast<int>(read_entry_(line, 11, 12));
                star_data[i].id = pack_tycho2_id(
                    static_cast<uint16_t>(tyc1),
                    static_cast<uint16_t>(tyc2),
                    static_cast<uint8_t>(tyc3)
                );


                float epochRA = 2000.0;
                float epochDEC = 2000.0;
                if (has_mean_position_(line)) {
                    // Mean position at bytes 15-27 and 28-40, epoch J2000.0
                    double RA = read_entry_(line, 15, 27);
                    double DEC = read_entry_(line, 28, 40);
                    if (std::isnan(RA) || std::isnan(DEC)) {
                        continue;
                    }
                    star_data[i].RA = RA * PI<double>() / 180.;
                    star_data[i].DEC = DEC * PI<double>() / 180.;
                }
                else {
                    // Observed position at bytes 153-165 and 166-178
                    double RA = read_entry_(line, 153, 165);
                    double DEC = read_entry_(line, 166, 178);
                    if (std::isnan(RA) || std::isnan(DEC)) {
                        continue;
                    }
                    star_data[i].RA = RA * PI<double>() / 180.;
                    star_data[i].DEC = DEC * PI<double>() / 180.;

                    // Epoch of observed position: epRA at bytes 179-183, epDE at 184-188
                    // These are "epoch - 1990", so add 1990 to get Julian year
                    double epRA = read_entry_(line, 179, 183);
                    double epDE = read_entry_(line, 184, 188);
                    if (std::isnan(epRA) || std::isnan(epDE)) {
                        continue;
                    }
                    // Take average of epRA and epDE to get a single epoch value for the star
                    epochRA = 1990.f + static_cast<float>(epRA);
                    epochDEC = 1990.f + static_cast<float>(epDE);
                }
                
                // Proper motion at bytes 41-48 and 49-56
                double pmRA = read_entry_(line, 41, 48);
                double pmDEC = read_entry_(line, 49, 56);
                if (!std::isnan(pmRA)) {
                    star_data[i].pmRA = static_cast<float>(pmRA);
                }
                if (!std::isnan(pmDEC)) {
                    star_data[i].pmDEC = static_cast<float>(pmDEC);
                }

                // Normalize to epoch J2000.0
                star_data[i].normalize_epoch(epochRA, epochDEC);

                
                // BT magnitude at bytes 110-116, VT at 123-129
                double BTmag = read_entry_(line, 110, 116);
                double VTmag = read_entry_(line, 123, 129);
                if (std::isnan(BTmag) && std::isnan(VTmag)) {
                    continue;
                }
                star_data[i].process_magnitude(BTmag, VTmag);
                i++;
            }
        }

        star_data.resize(i);
        return star_data;
    }

    std::vector<StarData> read_tycho2_suppl(const fs::path& filepath)
    {
        std::vector<StarData> star_data(count_lines_(filepath));
        std::ifstream file(filepath);
        if (!file.is_open()) {
            HUIRA_THROW_ERROR("read_tycho2_suppl - Failed to open file: " + filepath.string());
        }

        std::string line;
        std::size_t i = 0;
        while (std::getline(file, line)) {
            if (is_valid_line_(line)) {
                // Read identifer:
                int tyc1 = static_cast<int>(read_entry_(line, 0, 4));
                int tyc2 = static_cast<int>(read_entry_(line, 5, 10));
                int tyc3 = static_cast<int>(read_entry_(line, 11, 12));
                star_data[i].id = pack_tycho2_id(
                    static_cast<uint16_t>(tyc1),
                    static_cast<uint16_t>(tyc2),
                    static_cast<uint8_t>(tyc3)
                );

                // Supplement position at bytes 15-27 and 28-40
                double RA = read_entry_(line, 15, 27);
                double DEC = read_entry_(line, 28, 40);
                if (std::isnan(RA) || std::isnan(DEC)) {
                    continue;
                }
                star_data[i].RA = RA * PI<double>() / 180.;
                star_data[i].DEC = DEC * PI<double>() / 180.;

                // Proper motion at bytes 42-49 and 50-57
                double pmRA = read_entry_(line, 41, 48);
                double pmDEC = read_entry_(line, 49, 56);
                if (!std::isnan(pmRA)) {
                    star_data[i].pmRA = static_cast<float>(pmRA);
                }
                if (!std::isnan(pmDEC)) {
                    star_data[i].pmDEC = static_cast<float>(pmDEC);
                }

                // Normalize to epoch J2000.0
                star_data[i].normalize_epoch(1991.25f, 1991.25f);

                // BT magnitude at bytes 83-89, VT at 96-102
                double BTmag = read_entry_(line, 83, 89);
                double VTmag = read_entry_(line, 96, 102);
                if (std::isnan(BTmag) && std::isnan(VTmag)) {
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
