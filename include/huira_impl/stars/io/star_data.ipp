#include <vector>
#include <filesystem>

#include "huira/core/physics.hpp"

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

    
}
