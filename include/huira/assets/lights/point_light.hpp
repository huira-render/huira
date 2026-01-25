#pragma once

#include <optional>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/assets/lights/light.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class PointLight : public Light<TSpectral> {
    public:
        PointLight(const TSpectral& intensity) : intensity_{ intensity } {}

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


        LightType get_type() const override { return LightType::Point; }
        std::string get_info() const override { return "PointLight[" + std::to_string(this->id()) + "]" + (this->name_.empty() ? "" : " " + this->name_); }
        
        void set_intensity(const TSpectral& intensity) { intensity_ = intensity; }

    private:
        TSpectral intensity_{ 0 };
    };
}

#include "huira_impl/assets/lights/point_light.ipp"
