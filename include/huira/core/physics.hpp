
#pragma once

#include <cstddef>
#include <cmath>
#include <vector>

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"


namespace huira {
    /**
     * @brief Physical and astronomical utility functions for radiometry, black-body, and photometry.
     *
     * Provides functions for photon energy, Planck's law, black-body radiation, V-band photometry, and relativistic aberration.
     * Includes both scalar and spectral (template) versions for use in rendering and simulation.
     */
    
    inline double photon_energy(double lambda_meters);

    inline std::vector<double> plancks_law(double temp, const std::vector<double>& lambda);

    template <IsFloatingPoint T>
    std::vector<T> linspace(T min, T max, size_t N);

    template <IsSpectral TSpectral>
    TSpectral black_body(double temperature, std::size_t steps = 100);

    /**
     * @brief Approximate Johnson V-band filter response as a Gaussian.
     * @param N Number of wavelength samples
     * @param lambda Output vector of wavelengths (meters)
     * @return std::vector<double> Filter efficiency at each wavelength
     */
    inline std::vector<double> johnson_vband_approximation(std::size_t N, std::vector<double>& lambda);

    /**
     * @brief Compute V-band photon irradiance for a given visual magnitude.
     * @param visual_magnitude Visual magnitude (V)
     * @return double Photon flux (photons/s/m^2)
     */
    inline double v_band_irradiance(double visual_magnitude);

    template <IsSpectral TSpectral>
    TSpectral visual_magnitude_to_irradiance(double visual_magnitude, TSpectral albedo = TSpectral{ 1.f });

    /**
     * @brief Numerically integrate y(x) using the trapezoidal rule.
     * @param x X values
     * @param y Y values
     * @return double Integral
     */
    inline double integrate(const std::vector<double>& x, const std::vector<double>& y);

    /**
     * @brief Compute relativistic aberration of a direction vector for a moving observer.
     * @param direction Incoming direction (unit vector)
     * @param obs_velocity Observer velocity (m/s)
     * @return Vec3<double> Aberrated direction
     */
    inline Vec3<double> compute_aberrated_direction(Vec3<double> direction, Vec3<double> obs_velocity);

}

#include "huira_impl/core/physics.ipp"
