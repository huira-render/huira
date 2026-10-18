#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "huira/stars/io/star_data.hpp"
#include "huira/stars/io/tycho2_id.hpp"
#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {
inline std::size_t count_lines_(const fs::path& filepath)
{
    std::ifstream file(filepath, std::ios::binary);

    // Count new-lines:
    auto count =
        std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n');

    return static_cast<std::size_t>(count);
}

// Field positions below are 0-based [start, end), i.e. 1-based ReadMe columns start+1..end.
//
// Two shapes of tyc2.dat are read. The raw files from the CDS FTP archive follow the ReadMe
// for I/259 exactly. VizieR's text export of the same catalog (viz-bin/nph-Cat/txt, which is
// what media.huira.space mirrors, recognizable by its "#Table:" header lines) puts a '|'
// between every pair of columns, and the ReadMe has one pair without one: HIP (143-148) and
// CCDM (149-151). So in the export every column from CCDM on sits one byte to the right. Only
// the observed-position block (RAdeg, DEdeg, EpRA-1990, EpDE-1990) of the records without a
// mean position is read from past that point; observed_offset_() tells the shapes apart by
// where that block's separator is. The supplement files have no such pair.

/// 0 for a raw CDS record, 1 for a VizieR export record (see above), from the separators
/// before RAdeg (ReadMe byte 152) and before EpRA-1990 (byte 178), which the export shifts
/// to 153 and 179.
inline std::size_t observed_offset_(const std::string& line)
{
    if (line.size() > 178 && line[152] == '|' && line[178] == '|') {
        return 1;
    }
    if (line.size() > 177 && line[151] == '|' && line[177] == '|') {
        return 0;
    }
    HUIRA_THROW_ERROR("read_tycho2_dat - Record " + line.substr(0, 12) +
                      " is neither a raw CDS tyc2.dat record nor a VizieR text export of one: "
                      "the column separators around the observed position are not where "
                      "either puts them");
}

inline bool has_mean_position_(const std::string& line)
{
    if (line.size() < 14) {
        return false;
    }
    return line[13] != 'X';
}

inline double read_entry_(const std::string& line, std::size_t start, std::size_t end)
{
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
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

inline bool is_valid_line_(const std::string& line)
{
    // Data lines start with a digit (TYC1 number)
    return !line.empty() && std::isdigit(static_cast<unsigned char>(line[0]));
}

inline std::vector<StarData> read_tycho2_dat(const fs::path& filepath)
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
            // A record that is skipped below leaves this slot partly written, and a blank
            // proper motion does not overwrite it, so start each record from defaults:
            star_data[i] = StarData{};

            // Read identifier:
            int tyc1 = static_cast<int>(read_entry_(line, 0, 4));
            int tyc2 = static_cast<int>(read_entry_(line, 5, 10));
            int tyc3 = static_cast<int>(read_entry_(line, 11, 12));
            star_data[i].id = pack_tycho2_id(static_cast<uint16_t>(tyc1),
                                             static_cast<uint16_t>(tyc2),
                                             static_cast<uint8_t>(tyc3));

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
            } else {
                // Observed position: ReadMe bytes 153-164 and 166-177, shifted in the
                // VizieR export (see the note above)
                const std::size_t o = observed_offset_(line);
                double RA = read_entry_(line, 152 + o, 164 + o);
                double DEC = read_entry_(line, 165 + o, 177 + o);
                if (std::isnan(RA) || std::isnan(DEC)) {
                    continue;
                }
                star_data[i].RA = RA * PI<double>() / 180.;
                star_data[i].DEC = DEC * PI<double>() / 180.;

                // Epoch of observed position: ReadMe bytes 179-182 and 184-187, shifted
                // likewise. These are "epoch - 1990", so add 1990 to get the Julian year
                double epRA = read_entry_(line, 178 + o, 182 + o);
                double epDE = read_entry_(line, 183 + o, 187 + o);
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

inline std::vector<StarData> read_tycho2_suppl(const fs::path& filepath)
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
            // A record that is skipped below leaves this slot partly written, and a blank
            // proper motion does not overwrite it, so start each record from defaults:
            star_data[i] = StarData{};

            // Read identifier:
            int tyc1 = static_cast<int>(read_entry_(line, 0, 4));
            int tyc2 = static_cast<int>(read_entry_(line, 5, 10));
            int tyc3 = static_cast<int>(read_entry_(line, 11, 12));
            star_data[i].id = pack_tycho2_id(static_cast<uint16_t>(tyc1),
                                             static_cast<uint16_t>(tyc2),
                                             static_cast<uint8_t>(tyc3));

            // Supplement position at bytes 15-27 and 28-40
            double RA = read_entry_(line, 15, 27);
            double DEC = read_entry_(line, 28, 40);
            if (std::isnan(RA) || std::isnan(DEC)) {
                continue;
            }
            star_data[i].RA = RA * PI<double>() / 180.;
            star_data[i].DEC = DEC * PI<double>() / 180.;

            // Proper motion at bytes 42-48 and 50-56
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
} // namespace huira
