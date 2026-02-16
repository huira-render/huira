#include <vector>
#include <cmath>
#include <cstddef>

#include "huira/core/constants.hpp"
#include "huira/util/logger.hpp"
#include "huira/core/types.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"


namespace huira {
    /**
     * @brief Compute the energy of a photon for a given wavelength (in meters).
     * @param lambda_meters Wavelength in meters
     * @return double Photon energy in joules
     */
    inline double photon_energy(double lambda_meters) {
        // E = hc / lambda
        return (H_PLANCK<double>() * SPEED_OF_LIGHT<double>()) / lambda_meters;
    }


    /**
     * @brief Compute spectral radiance using Planck's law for a given temperature and wavelengths.
     * @param temp Temperature in Kelvin
     * @param lambda Vector of wavelengths (meters)
     * @return std::vector<double> Spectral radiance at each wavelength
     */
    inline std::vector<double> plancks_law(double temp, const std::vector<double>& lambda) {
        std::vector<double> radiance(lambda.size());

        // Precompute constants to save division ops
        double c1 = 2.0 * H_PLANCK<double>() * SPEED_OF_LIGHT<double>() * SPEED_OF_LIGHT<double>();
        double c2 = (H_PLANCK<double>() * SPEED_OF_LIGHT<double>()) / K_BOLTZ<double>();

        for (size_t i = 0; i < lambda.size(); ++i) {
            double lam = lambda[i];
            // Planck's Law: B(T) = (2hc^2 / lam^5) * (1 / (exp(hc/lamkT) - 1))
            double exponential = std::exp(c2 / (lam * temp)) - 1.0;
            radiance[i] = (c1 / std::pow(lam, 5)) / exponential;
        }
        return radiance;
    }


    /**
     * @brief Generate N linearly spaced values between min and max (inclusive).
     * @tparam T Floating point type
     * @param min Minimum value
     * @param max Maximum value
     * @param N Number of samples
     * @return std::vector<T> Linearly spaced values
     */
    template <IsFloatingPoint T>
    std::vector<T> linspace(T min, T max, size_t N)
    {
        std::vector<T> output(N);
        T step = (max - min) / static_cast<T>(N - 1);
        for (size_t i = 0; i < N; ++i) {
            output[i] = min + (static_cast<T>(i) * step);
        }
        return output;
    }


    /**
     * @brief Compute the black-body spectral radiance for a given temperature.
     * @tparam TSpectral Spectral type
     * @param temperature Temperature in Kelvin
     * @param steps Number of integration steps per bin
     * @return TSpectral Black-body radiance in each spectral bin
     */
    template <IsSpectral TSpectral>
    TSpectral black_body(double temperature, std::size_t steps)
    {
        TSpectral radiance{ 0 };
        auto bins = TSpectral::get_all_bins();
        for (std::size_t i = 0; i < bins.size(); ++i) {
            // Integrate black-body spectrum over the computed bounds:
            std::vector<double> lambda = linspace(bins[i].min_wavelength, bins[i].max_wavelength, steps);
            std::vector<double> rad = plancks_law(temperature, lambda);
            radiance[i] = static_cast<float>(integrate(lambda, rad));
        }

        return radiance;
    }

    // Johnson V-Band Approximation (Gaussian fit):
    // Paper: Bessell, M. S. (1990). UBVRI passbands. Publications of the Astronomical Society of
    // the Pacific, 102, 1181.
    // 
    // Center (mu) = 551nm (Standard Johnson V effective wavelength)
    // FWHM ~ 88nm -> Sigma ~ 37-38nm

    /**
     * @brief Approximate the Johnson V-band filter response as a Gaussian.
     * @param N Number of wavelength samples
     * @param lambda Output vector of wavelengths (meters)
     * @return std::vector<double> Filter efficiency at each wavelength
     */
    inline std::vector<double> johnson_vband_approximation(std::size_t N, std::vector<double>& lambda) {
        std::vector<double> efficiency(N);
        lambda.resize(N);

        double lambda_min = 350e-9; // Start of visual range
        double lambda_max = 850e-9; // End of visual range
        double step = (lambda_max - lambda_min) / static_cast<double>(N - 1);

        // Standard Gaussian parameters for V-band
        double mu = 551e-9;
        double sigma = 38e-9;
        double two_sigma_sq = 2.0 * sigma * sigma;

        for (std::size_t i = 0; i < N; ++i) {
            double l = lambda_min + static_cast<double>(i) * step;
            lambda[i] = l;

            // Simple Gaussian profile: exp( - (x-mu)^2 / 2sigma^2 )
            double diff = l - mu;
            efficiency[i] = std::exp(-(diff * diff) / two_sigma_sq);
        }
        return efficiency;
    }

    // V-Band Irradiance (Reference zero point)
    // Cohen, M., Walker, R. G., Barlow, M. J., & Deacon, J. R. (1992).
    // Spectral irradiance calibration in the infrared. I - Ground-based and IRAS broadband calibrations.
    // The Astronomical Journal, 104, 1650.

    /**
     * @brief Compute V-band photon irradiance for a given visual magnitude.
     * @param visual_magnitude Visual magnitude (V)
     * @return double Photon flux (photons/s/m^2)
     */
    inline double v_band_irradiance(double visual_magnitude) {
        // Reference Zero Point (Vega, V=0).
        // The spectral flux density of Vega at 555nm is approx 3.63e-11 W/m^2/nm.
        // However, since we are working in PHOTONS in the main loop, we need
        // the Integrated Photon Flux for a V=0 star over the V-band.

        // Approximate Integrated Photon Flux for V=0:
        // ~ 8.75 * 10^9 photons · s^-1 · m^-2
        // (Source: Derived from Bessell 1979 & Cohen 1992 calibration)
        const double photon_flux_zero_point = 8.75e9;

        // Flux formula: F = F0 * 10^(-0.4 * mag)
        return photon_flux_zero_point * std::pow(10.0, -0.4 * visual_magnitude);
    }




    /**
     * @brief Convert visual magnitude to spectral irradiance, assuming a solar spectrum.
     * @tparam TSpectral Spectral type
     * @param visual_magnitude Visual magnitude (V)
     * @param albedo Optional albedo scaling (default 1)
     * @return TSpectral Spectral irradiance
     */
    template <IsSpectral TSpectral>
    TSpectral visual_magnitude_to_irradiance(double visual_magnitude, TSpectral albedo)
    {
        // Solar spectral shape (coarse, in arbitrary radiance units — only ratios matter):
        TSpectral solar_template = black_body<TSpectral>(5778.0, 100);
        double solar_total = static_cast<double>(solar_template.total());

        // Fine-sample the solar spectrum convolved with V-band response:
        constexpr std::size_t N = 500;
        std::vector<double> vband_lambda;
        std::vector<double> vband_response = johnson_vband_approximation(N, vband_lambda);
        std::vector<double> solar_fine = plancks_law(5778.0, vband_lambda);

        // V-baband weighted solar radiance:
        std::vector<double> weighted(N);
        for (std::size_t i = 0; i < N; ++i) {
            weighted[i] = solar_fine[i] * vband_response[i];
        }
        double solar_vband_weighted = integrate(vband_lambda, weighted);

        auto bins = TSpectral::get_all_bins();
        double lambda_min = bins[0].min_wavelength;
        double lambda_max = bins[0].max_wavelength;
        for (std::size_t i = 1; i < TSpectral::size(); ++i) {
            if (bins[i].min_wavelength < lambda_min) lambda_min = bins[i].min_wavelength;
            if (bins[i].max_wavelength > lambda_max) lambda_max = bins[i].max_wavelength;
        }

        auto full_lambda = linspace(lambda_min, lambda_max, N);
        auto solar_full = plancks_law(5778.0, full_lambda);
        double solar_full_integral = integrate(full_lambda, solar_full);

        // V-band observed photon flux:
        double observed_vband_flux = v_band_irradiance(visual_magnitude);

        // Total photon flux across all wavelengths, assuming solar spectrum:
        double total_flux = observed_vband_flux * solar_full_integral / solar_vband_weighted;

        // Distribute across bins proportionally to solar template:
        double scale = total_flux / solar_total;

        TSpectral photon_counts = solar_template * static_cast<float>(scale) * albedo;
        TSpectral irradiance = photon_counts * TSpectral::photon_energies();
        return irradiance;
    }



    /**
     * @brief Numerically integrate y(x) using the trapezoidal rule.
     * @param x X values
     * @param y Y values
     * @return double Integral
     */
    inline double integrate(const std::vector<double>& x, const std::vector<double>& y) {
        double sum = 0.0;
        for (size_t i = 0; i < x.size() - 1; ++i) {
            double dx = x[i + 1] - x[i];
            double avg_y = 0.5 * (y[i] + y[i + 1]);
            sum += avg_y * dx;
        }
        return sum;
    }


    /**
     * @brief Compute relativistic aberration of a direction vector for a moving observer.
     * @param direction Incoming direction (unit vector)
     * @param v_obs Observer velocity (m/s)
     * @return Vec3<double> Aberrated direction
     */
    inline Vec3<double> compute_aberrated_direction(Vec3<double> direction, Vec3<double> v_obs)
    {
        // Calculate Relativistic Beta and Gamma
        Vec3<double> beta = v_obs / SPEED_OF_LIGHT<double>();

        double beta_sq = glm::dot(beta, beta);

        // Check Observer Speed
        if (beta_sq < 0.999999) {
            double gamma = 1.0 / std::sqrt(1.0 - beta_sq);
            double u_dot_beta = glm::dot(direction, beta);

            // Relativistic Aberration Formula
            //    Transforms the direction vector 'u' into the moving frame 'u_app'.
            //    Formula: u_app = (u + beta + (gamma / (1+gamma)) * (u . beta) * beta) 
            //                     ----------------------------------------------------
            //                             gamma * (1 + u . beta)
            //
            // Because 'u' is (Observer -> Object) and we move 'v', 
            // the object should appear shifted TOWARDS 'v'. 
            // (u + beta) in the numerator achieves this correctly.

            Vec3<double> num = direction + beta + (gamma / (1.0 + gamma)) * u_dot_beta * beta;
            double den = gamma * (1.0 + u_dot_beta);

            return num / den;
        }
        else {
            HUIRA_THROW_ERROR("Observer is faster than speed of light");
        }
    }
}
