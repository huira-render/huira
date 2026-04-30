#pragma once

#include <optional>

#include "huira/assets/lights/light.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
#include "huira/render/interaction.hpp"
#include "huira/render/sampler.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class SphereLight : public Light<TSpectral> {
    public:

        SphereLight(const units::Meter& radius, const units::SpectralWatts<TSpectral>& spectral_power);
        SphereLight(const units::Meter& radius, const units::SpectralWattsPerMeterSquaredSteradian<TSpectral>& spectral_radiance);
        SphereLight(const units::Meter& radius, const units::Watt& power);
        SphereLight(const units::Meter& radius, const units::Kelvin& temperature);

        SphereLight(const SphereLight&) = delete;
        SphereLight(SphereLight&&) = delete;
        SphereLight& operator=(const SphereLight&) = delete;
        SphereLight& operator=(SphereLight&&) = delete;

        std::optional<LightSample<TSpectral>> sample_li(
            const Interaction<TSpectral>& isect,
            const Transform<float>& transform,
            Sampler<float>& sampler) const override;

        float pdf_li(
            const Interaction<TSpectral>& isect,
            const Transform<float>& transform,
            const Vec3<float>& wi) const override;

        units::Meter radius() const { return units::Meter(radius_); }

        TSpectral radiance(const Vec3<float>& point_on_light, const Vec3<float>& outgoing_direction) const override;

        TSpectral irradiance_at(
            const Vec3<float>& position,
            const Transform<float>& light_to_world
        ) const override;

        LightType get_type() const override { return LightType::Sphere; }
        std::string type() const override { return "SphereLight"; }

    private:
        void set_spectral_power(const units::Watt& power);
        void set_spectral_power(const units::SpectralWatts<TSpectral>& spectral_power);

        float radius_;
        TSpectral radiance_;
    };
}

#include "huira_impl/assets/lights/sphere_light.ipp"
