#pragma once

#include <vector>
#include <cmath>
#include <cstddef>

#include "huira/core/types.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    inline double photon_energy(double lambda_meters);

    inline std::vector<double> plancks_law(double temp, const std::vector<double>& lambda);

    template <IsFloatingPoint T>
    std::vector<T> linspace(T min, T max, size_t N);

    template <IsSpectral TSpectral>
    TSpectral black_body(double temperature, std::size_t steps = 100);


    // Johnson V-Band Approximation (Gaussian fit):
    // Paper: Bessell, M. S. (1990). UBVRI passbands. Publications of the Astronomical Society of
    // the Pacific, 102, 1181.
    // 
    // Center (mu) = 551nm (Standard Johnson V effective wavelength)
    // FWHM ~ 88nm -> Sigma ~ 37-38nm
    inline std::vector<double> johnson_vband_approximation(std::size_t N, std::vector<double>& lambda);


    // V-Band Irradiance (Reference zero point)
    // Cohen, M., Walker, R. G., Barlow, M. J., & Deacon, J. R. (1992).
    // Spectral irradiance calibration in the infrared. I - Ground-based and IRAS broadband calibrations.
    // The Astronomical Journal, 104, 1650.
    inline double v_band_irradiance(double visual_magnitude);


    inline double integrate(const std::vector<double>& x, const std::vector<double>& y);

    inline Vec3<double> compute_aberrated_direction(Vec3<double> direction, Vec3<double> obs_velocity);

}

#include "huira_impl/core/physics.ipp"
