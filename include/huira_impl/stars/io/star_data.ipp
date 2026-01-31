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
        if (has_bt && has_vt) {
            visual_magnitude = VTmag - 0.090 * (BTmag - VTmag);
            bv_color_index = 0.850 * (BTmag - VTmag);
        }
        else {
            visual_magnitude = has_vt ? VTmag : BTmag;
        }

        // Compute effective temperature from B-V color index:
        temperature = 4600.0 * (1.0 / (0.92 * bv_color_index + 1.7) + 1.0 / (0.92 * bv_color_index + 0.62));

        // Perform spectrophotometric calibration:
        double irradiance_ref = v_band_irradiance(visual_magnitude);
        std::size_t N = 1000;
        std::vector<double> lambda;
        std::vector<double> v_band_efficiency = johnson_vband_approximation(N, lambda);
        std::vector<double> radiance = plancks_law(temperature, lambda);
        std::vector<double> photon_counts(N);
        for (size_t i = 0; i < N; ++i) {
            photon_counts[i] = v_band_efficiency[i] * radiance[i] / photon_energy(lambda[i]);
        }
        double radiance_from_temp = integrate(lambda, photon_counts);
        solid_angle = irradiance_ref / radiance_from_temp;
    }

    inline void write_star_data(const fs::path& filepath, std::vector<StarData>& stars)
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
            HUIRA_THROW_ERROR("Failed to open file for writing: " + filepath.string());
        }

        // Write count first
        std::uint64_t count = valid_stars.size();
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));

        // Write all star data
        out.write(reinterpret_cast<const char*>(valid_stars.data()),
            static_cast<std::streamsize>(valid_stars.size() * sizeof(StarData)));
    }

    inline std::vector<StarData> read_star_data(const fs::path& filepath, double maximum_magnitude)
    {
        std::ifstream in(filepath, std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open file for reading: " + filepath.string());
        }

        uint64_t count = 0;
        in.read(reinterpret_cast<char*>(&count), sizeof(count));

        // Binary search on file to find cutoff index
        std::uint64_t low = 0, high = count;
        StarData temp;

        while (low < high) {
            std::uint64_t mid = low + (high - low) / 2;
            in.seekg(static_cast<std::streamoff>(sizeof(std::uint64_t) + mid * sizeof(StarData)));
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
        in.seekg(static_cast<std::streamoff>(sizeof(uint64_t)));
        in.read(reinterpret_cast<char*>(stars.data()),
            static_cast<std::streamsize>(low * sizeof(StarData)));

        return stars;
    }
}
