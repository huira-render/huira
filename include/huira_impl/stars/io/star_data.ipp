#include <algorithm>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdint>

#include "huira/core/physics.hpp"
#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {
    inline void StarData::process_magnitude(double BTmag, double VTmag)
    {
        bool has_bt = !std::isnan(BTmag);
        bool has_vt = !std::isnan(VTmag);
        if (!has_bt && !has_vt) {
            return;
        }
        
        // Compute visual magnitude and B-V color index:
        double bv_color_index = 0.3;  // Default: assume white star
        double vmag;
        if (has_bt && has_vt) {
            vmag = VTmag - 0.090 * (BTmag - VTmag);
            bv_color_index = 0.850 * (BTmag - VTmag);
        }
        else {
            vmag = has_vt ? VTmag : BTmag;
        }

        // Compute effective temperature from B-V color index:
        double temp = 4600.0 * (1.0 / (0.92 * bv_color_index + 1.7) + 1.0 / (0.92 * bv_color_index + 0.62));

        // Store single precision:
        visual_magnitude = static_cast<float>(vmag);
        temperature = static_cast<float>(temp);

        // Perform spectrophotometric calibration:
        double irradiance_ref = v_band_irradiance(vmag);
        std::size_t N = 1000;
        std::vector<double> lambda;
        std::vector<double> v_band_efficiency = johnson_vband_approximation(N, lambda);
        std::vector<double> radiance = plancks_law(temp, lambda);
        std::vector<double> photon_counts(N);
        for (size_t i = 0; i < N; ++i) {
            photon_counts[i] = v_band_efficiency[i] * radiance[i] / photon_energy(lambda[i]);
        }
        double radiance_from_temp = integrate(lambda, photon_counts);
        solid_angle = irradiance_ref / radiance_from_temp;
    }

    inline void StarData::normalize_epoch(double epochRA, double epochDEC)
    {
        if (epochRA == 2000.0f && epochDEC == 2000.0f) {
            return;
        }

        constexpr double MAS_TO_RAD = PI<double>() / (180.0 * 3600.0 * 1000.0);

        // Years from each epoch to J2000
        double years_RA = 2000.0 - static_cast<double>(epochRA);
        double years_DEC = 2000.0 - static_cast<double>(epochDEC);

        // Apply proper motion to bring position to J2000
        double pmRA_rad = static_cast<double>(pmRA) * MAS_TO_RAD;
        double pmDEC_rad = static_cast<double>(pmDEC) * MAS_TO_RAD;

        DEC += pmDEC_rad * years_DEC;
        RA += pmRA_rad * years_RA / std::cos(DEC);
    }

    inline void write_star_data(const fs::path& filepath, const std::vector<StarData>& stars)
    {
        // Create a mutable copy, removing any stars with NaN visual_magnitude
        std::vector<StarData> valid_stars;
        valid_stars.reserve(stars.size());
        std::copy_if(stars.begin(), stars.end(), std::back_inserter(valid_stars),
            [](const StarData& s) { return !std::isnan(s.visual_magnitude); });

        // Sort by visual_magnitude (brightest first = lowest magnitude)
        std::sort(valid_stars.begin(), valid_stars.end());

        std::ofstream out(filepath, std::ios::binary);
        if (!out) {
            HUIRA_THROW_ERROR("StarData - Failed to open file for writing: " + filepath.string());
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
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // Write all star data
        out.write(reinterpret_cast<const char*>(valid_stars.data()),
            static_cast<std::streamsize>(valid_stars.size() * sizeof(StarData)));

        HUIRA_LOG_INFO("StarData - " + std::to_string(valid_stars.size()) + " stars written to: " + filepath.string());
    }

    inline std::vector<StarData> read_star_data(const fs::path& filepath, float maximum_magnitude)
    {
        std::ifstream in(filepath, std::ios::binary);
        if (!in) {
            HUIRA_THROW_ERROR("StarData - Failed to open file for reading: " + filepath.string());
        }

        // Read and validate header
        HrscHeader header{};
        in.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.magic[0] != 'H' || header.magic[1] != 'R' ||
            header.magic[2] != 'S' || header.magic[3] != 'C') {
            HUIRA_THROW_ERROR("StarData - Invalid file format: " + filepath.string());
        }

        if (header.version != 1) {
            HUIRA_THROW_ERROR("StarData - Unsupported version: " + std::to_string(header.version));
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

        HUIRA_LOG_INFO("StarData - " + std::to_string(stars.size()) + " stars read from: " + filepath.string());

        return stars;
    }
}
