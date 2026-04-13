#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/core/units/units.hpp"
#include "huira/logging/logger.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    struct ExponentialScatteringLayer {
        TSpectral scattering_base;
        TSpectral extinction_base;

        units::Kilometer scale_height;
        float asymmetry_g = 0.0f;
    };

    template <IsSpectral TSpectral>
    struct TentAbsorptionLayer {
        TSpectral absorption_base;
        units::Kilometer peak_altitude;
        units::Kilometer width;
    };



    template <IsSpectral TSpectral>
    struct AtmosphereParameters {
        units::Kilometer equatorial_radius;
        units::Kilometer polar_radius;
        units::Kilometer max_height;

        std::vector<ExponentialScatteringLayer<TSpectral>> exponential_layers;
        std::vector<TentAbsorptionLayer<TSpectral>>        tent_layers;

        void add_rayleigh_layer(
            const TSpectral& scattering_beta,
            units::Kilometer scale_height)
        {
            exponential_layers.push_back({
                .scattering_base = scattering_beta,
                .extinction_base = scattering_beta,
                .scale_height = scale_height,
                .asymmetry_g = 0.0f
                });
        }

        void add_mie_layer(
            const TSpectral& scattering_beta,
            const TSpectral& extinction_beta,
            units::Kilometer scale_height,
            float asymmetry_g)
        {
            exponential_layers.push_back({
                .scattering_base = scattering_beta,
                .extinction_base = extinction_beta,
                .scale_height = scale_height,
                .asymmetry_g = asymmetry_g
                });
        }


        void add_ozone_layer(
            const TSpectral& absorption_beta,
            units::Kilometer peak,
            units::Kilometer width)
        {
            tent_layers.push_back({
                .absorption_base = absorption_beta,
                .peak_altitude = peak,
                .width = width
                });
        }
    };




    template <IsSpectral TSpectral>
    TSpectral generate_rayleigh_beta(double reference_wavelength_meters, float reference_scattering)
    {
        TSpectral beta;
        auto bins = TSpectral::get_all_bins();

        for (std::size_t i = 0; i < TSpectral::size(); ++i) {
            double bin_center_m = bins[i].center_wavelength;
            if (bin_center_m <= 0.0) {
                beta[i] = 0.0f;
                continue;
            }

            double ratio = reference_wavelength_meters / bin_center_m;
            beta[i] = reference_scattering * static_cast<float>(std::pow(ratio, 4.0));
        }
        return beta;
    }

    template <IsSpectral TSpectral>
    TSpectral generate_mie_beta(double reference_wavelength_meters, float reference_scattering, float angstrom_exponent)
    {
        TSpectral beta;
        auto bins = TSpectral::get_all_bins();

        for (std::size_t i = 0; i < TSpectral::size(); ++i) {
            double bin_center_m = bins[i].center_wavelength;
            if (bin_center_m <= 0.0) {
                beta[i] = 0.0f;
                continue;
            }

            double ratio = reference_wavelength_meters / bin_center_m;
            beta[i] = reference_scattering * static_cast<float>(std::pow(ratio, static_cast<double>(angstrom_exponent)));
        }
        return beta;
    }




    template <IsSpectral TSpectral>
    class Atmosphere {
    public:
        explicit Atmosphere(AtmosphereParameters<TSpectral> parameters) :
            parameters_(std::move(parameters)),
            equatorial_radius_(parameters_.equatorial_radius.to_si_f()),
            polar_radius_(parameters_.polar_radius.to_si_f()),
            max_height_(parameters_.max_height.to_si_f())
        {
            if (equatorial_radius_ <= 0.0f || polar_radius_ <= 0.0f || max_height_ <= 0.0f) {
                HUIRA_THROW_ERROR("Atmosphere geometry parameters must be positive.");
            }
        }


        float calculate_altitude(const Vec3<float>& local_sample_pos) const {
            float r_sample = std::sqrt(local_sample_pos.x * local_sample_pos.x +
                local_sample_pos.y * local_sample_pos.y +
                local_sample_pos.z * local_sample_pos.z);

            float y_norm = local_sample_pos.y / r_sample;
            float xz_norm = std::sqrt(std::max(0.0f, 1.0f - y_norm * y_norm));

            float r_eq = equatorial_radius_;
            float r_pol = polar_radius_;

            float r_ellipsoid = (r_eq * r_pol) /
                std::sqrt((r_pol * xz_norm) * (r_pol * xz_norm) +
                    (r_eq * y_norm) * (r_eq * y_norm));

            return r_sample - r_ellipsoid;
        }

        const AtmosphereParameters<TSpectral>& get_parameters() const {
            return parameters_;
        }

    private:
        AtmosphereParameters<TSpectral> parameters_;

        float equatorial_radius_;
        float polar_radius_;
        float max_height_;
    };

    namespace atmosphere {
        // The wavelength at which the following base scattering coefficients were measured
        constexpr double reference_wavelength_meters = 550e-9;

        namespace earth {
            // Geometry (WGS-84)
            constexpr float equatorial_radius_km = 6378.137f;
            constexpr float polar_radius_km = 6356.752f;
            constexpr float max_height_km = 100.0f;

            // Rayleigh (Nitrogen/Oxygen mix)
            constexpr float rayleigh_scattering_base = 0.0058f; // km^-1
            constexpr float rayleigh_scale_height_km = 8.0f;

            // Mie (Water vapor, aerosols, haze)
            constexpr float mie_scattering_base = 0.0039f; // km^-1
            constexpr float mie_extinction_base = 0.0044f; // km^-1
            constexpr float mie_scale_height_km = 1.2f;
            constexpr float mie_asymmetry_g = 0.76f;       // Moderate forward scattering
            constexpr float mie_angstrom_exponent = 1.3f;
        }

        namespace mars {
            // Geometry (MOLA)
            constexpr float equatorial_radius_km = 3396.2f;
            constexpr float polar_radius_km = 3376.2f;
            constexpr float max_height_km = 120.0f;

            // Rayleigh (Thin CO2 atmosphere)
            // Surface pressure is ~0.6% of Earth's, so Rayleigh is significantly weaker
            constexpr float rayleigh_scattering_base = 0.0000196f; // km^-1
            constexpr float rayleigh_scale_height_km = 11.1f;      // Higher due to lower gravity

            // Mie (Heavy Iron-Oxide Dust)
            // Mars is incredibly dusty, and the dust is uniformly mixed high into the atmosphere
            constexpr float mie_scattering_base = 0.0150f; // km^-1
            constexpr float mie_extinction_base = 0.0180f; // km^-1 (High absorption from iron oxide)
            constexpr float mie_scale_height_km = 11.1f;   // Matches gas scale height due to high mixing
            constexpr float mie_asymmetry_g = 0.85f;       // Stronger forward scattering (larger particles)
            constexpr float mie_angstrom_exponent = 0.8f;  // Lower exponent indicates larger average particle size than Earth
        }
    }


    template <typename TSpectral>
    AtmosphereParameters<TSpectral> make_earth()
    {
        AtmosphereParameters<TSpectral> params;
        namespace earth = atmosphere::earth;
        constexpr double anchor_m = atmosphere::reference_wavelength_meters;

        // Geometry
        params.equatorial_radius = units::Kilometer(earth::equatorial_radius_km);
        params.polar_radius = units::Kilometer(earth::polar_radius_km);
        params.max_height = units::Kilometer(earth::max_height_km);

        // Generate spectral curves
        auto rayleigh_beta = generate_rayleigh_beta<TSpectral>(
            anchor_m, earth::rayleigh_scattering_base
        );

        auto mie_scattering = generate_mie_beta<TSpectral>(
            anchor_m, earth::mie_scattering_base, earth::mie_angstrom_exponent
        );

        auto mie_extinction = generate_mie_beta<TSpectral>(
            anchor_m, earth::mie_extinction_base, earth::mie_angstrom_exponent
        );

        // Build layers explicitly (no hidden default parameters)
        params.add_rayleigh_layer(
            rayleigh_beta,
            units::Kilometer(earth::rayleigh_scale_height_km)
        );

        params.add_mie_layer(
            mie_scattering,
            mie_extinction,
            units::Kilometer(earth::mie_scale_height_km),
            earth::mie_asymmetry_g
        );

        return params;
    }

    template <typename TSpectral>
    AtmosphereParameters<TSpectral> make_mars()
    {
        AtmosphereParameters<TSpectral> params;
        namespace mars = atmosphere::mars;
        constexpr double anchor_m = atmosphere::reference_wavelength_meters;

        // Geometry
        params.equatorial_radius = units::Kilometer(mars::equatorial_radius_km);
        params.polar_radius = units::Kilometer(mars::polar_radius_km);
        params.max_height = units::Kilometer(mars::max_height_km);

        // Generate spectral curves
        auto rayleigh_beta = generate_rayleigh_beta<TSpectral>(
            anchor_m, mars::rayleigh_scattering_base
        );

        auto mie_scattering = generate_mie_beta<TSpectral>(
            anchor_m, mars::mie_scattering_base, mars::mie_angstrom_exponent
        );

        auto mie_extinction = generate_mie_beta<TSpectral>(
            anchor_m, mars::mie_extinction_base, mars::mie_angstrom_exponent
        );

        // Build layers
        params.add_rayleigh_layer(
            rayleigh_beta,
            units::Kilometer(mars::rayleigh_scale_height_km)
        );

        params.add_mie_layer(
            mie_scattering,
            mie_extinction,
            units::Kilometer(mars::mie_scale_height_km),
            mars::mie_asymmetry_g
        );

        return params;
    }
}


