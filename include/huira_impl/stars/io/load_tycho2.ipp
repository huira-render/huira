#include <filesystem>
#include <vector>
#include <fstream>
#include <cstddef>
#include <algorithm>
#include <limits>

#include "huira/core/constants.hpp"
#include "huira/core/physics.hpp"
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


    static void process_star_(double BTmag, double VTmag, StarData& star)
    {
        bool has_bt = !std::isnan(BTmag);
        bool has_vt = !std::isnan(VTmag);

        double bv_color_index = 0.3;  // Default: assume white star

        if (has_bt && has_vt) {
            star.visual_magnitude = VTmag - 0.090 * (BTmag - VTmag);
            bv_color_index = 0.850 * (BTmag - VTmag);
        }
        else {
            star.visual_magnitude = has_vt ? VTmag : BTmag;
        }

        star.temperature = 4600.0 * (1.0 / (0.92 * bv_color_index + 1.7) + 1.0 / (0.92 * bv_color_index + 0.62));

        // Perform spectrophotometric calibration:
        double irradiance_ref = v_band_irradiance(star.visual_magnitude);

        std::size_t N = 1000; // Number of bins to use
        std::vector<double> lambda;
        std::vector<double> v_band_efficiency = johnson_vband_approximation(N, lambda);
        std::vector<double> radiance = plancks_law(star.temperature, lambda);
        std::vector<double> photon_counts(N);
        for (size_t i = 0; i < N; ++i) {
            photon_counts[i] = v_band_efficiency[i] * radiance[i] / photon_energy(lambda[i]);
        }
        double radiance_from_temp = integrate(lambda, photon_counts);
        star.solid_angle = irradiance_ref / radiance_from_temp;
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

                star_data[i].pmRA = read_entry_(line, 41, 48);
                star_data[i].pmDE = read_entry_(line, 49, 56);
                if (std::isnan(star_data[i].pmRA) || std::isnan(star_data[i].pmDE)) {
                    // Proper motion not available, set to zero
                    star_data[i].pmRA = 0.0;
                    star_data[i].pmDE = 0.0;
                }

                double BTmag = read_entry_(line, 110, 116);
                double VTmag = read_entry_(line, 123, 129);

                if (std::isnan(BTmag) && std::isnan(VTmag)) {
                    // Skip entry if no magnitude information is given:
                    continue;
                }

                // Compute the temperature, solid angle, and visual magnitude of the star:
                process_star_(BTmag, VTmag, star_data[i]);

                i++;
            }
        }
        star_data.resize(i);
        return star_data;
    }
}
