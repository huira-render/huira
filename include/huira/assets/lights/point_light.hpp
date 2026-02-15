#pragma once

#include <optional>

#include "huira/assets/lights/light.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/types.hpp"
#include "huira/core/units/units.hpp"

namespace huira {
    /**
     * @brief A point light source that emits light uniformly in all directions.
     * 
     * Point lights are infinitesimally small light sources located at a single point
     * in space. The irradiance falls off with the inverse square of the distance.
     * 
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class PointLight : public Light<TSpectral> {
    public:
        PointLight(const units::SpectralWatts<TSpectral>& spectral_power);
        PointLight(const units::Watt& power);

        PointLight(const PointLight&) = delete;
        PointLight& operator=(const PointLight&) = delete;

        std::optional<LightSample<TSpectral>> sample_li(
            const Interaction<TSpectral>& ref,
            const Transform<float>& light_to_world,
            const Sampler<float>& sampler
        ) const override;

        float pdf_li(
            const Interaction<TSpectral>& ref,
            const Transform<float>& light_to_world,
            const Vec3<float>& wi
        ) const override;


        TSpectral irradiance_at(
            const Vec3<float>& position,
            const Transform<float>& light_to_world
        ) const override;

        LightType get_type() const override { return LightType::Point; }
        std::string type() const override { return "PointLight"; }

        void set_spectral_power(const units::SpectralWatts<TSpectral>& spectral_power);
        void set_spectral_power(const units::Watt& power);

    private:
        TSpectral irradiance_{ 0 };
    };
}

#include "huira_impl/assets/lights/point_light.ipp"
