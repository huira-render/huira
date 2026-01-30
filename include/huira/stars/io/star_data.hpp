#pragma once

#include <limits>

#include "huira/core/physics.hpp"

namespace huira {
    struct StarData {
        double RA;
        double DEC;

        // Default proper motions are zero:
        double pmRA = 0.0;
        double pmDEC = 0.0;

        // Calibratied Spectrophotometric Properties:
        double temperature = std::numeric_limits<double>::quiet_NaN();
        double solid_angle = std::numeric_limits<double>::quiet_NaN();
        double visual_magnitude = std::numeric_limits<double>::quiet_NaN();

        // Process BT and VT magnitudes to compute temperature, solid angle, and visual magnitude
        void process_magnitude(double BTmag, double VTmag)
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
    };
}
