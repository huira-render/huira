#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/core/time.hpp"
#include "huira/stars/io/star_data.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Star {
    public:
        Star() = default;
        Star(const Vec3<double>& direction, TSpectral irradiance);
        Star(const StarData& star_data, Time time);
        Star(const StarData& star_data, double years_since_j2000);

        Vec3<double> get_direction() const { return direction_; }
        TSpectral get_irradiance() const { return irradiance_; }

    private:
        Vec3<double> direction_{ 0,0,0 };
        TSpectral irradiance_{ 0 };

        void compute_direction_(double RA, double DEC, float pmRAmas, float pmDECmas, double years_since_j2000);
        void compute_irradiance_(float temperature, double solid_angle);
    };

}

#include "huira_impl/stars/star.ipp"
