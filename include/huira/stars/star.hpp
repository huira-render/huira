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

        TSpectral get_irradiance() { return irradiance_; }
        Vec3<double> get_direction() { return direction_; }

    private:
        Vec3<double> direction_{ 0,0,0 };
        TSpectral irradiance_{ 0 };
    };

}

#include "huira_impl/stars/star.ipp"
