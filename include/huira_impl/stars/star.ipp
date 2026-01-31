#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/core/time.hpp"
#include "huira/stars/io/star_data.hpp"

namespace huira {

    template <IsSpectral TSpectral>
    Star<TSpectral>::Star(const Vec3<double>& direction, TSpectral irradiance) :
        direction_{ direction }, irradiance_{ irradiance }
    {

    }

    template <IsSpectral TSpectral>
    Star<TSpectral>::Star(const StarData& star_data, Time time)
    {
        (void)star_data;
        (void)time;
    }
}
