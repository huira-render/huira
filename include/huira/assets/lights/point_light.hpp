#pragma once

#include "huira/core/types.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/assets/lights/light.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class PointLight : public Light<TSpectral> {
    public:
        PointLight(const TSpectral& spectral_intensity) : spectral_intensity_{ spectral_intensity }
        {

        }

        // Delete copying:
        PointLight(const PointLight&) = delete;
        PointLight& operator=(const PointLight&) = delete;

        LightSample<TSpectral> sample_Li(const Vec3<float>& point, Sampler<float>& sampler) const override;

        float pdf_Li(const Vec3<float>& point, const Vec3<float>& wi) const override;

        LightType get_type() const override { return LightType::Point; }

        std::string get_type_name() const override { return "PointLight"; }

        // Setters for scene editing
        void set_intensity(const TSpectral& intensity) { spectral_intensity_ = intensity; }

    private:
        TSpectral spectral_intensity_{ 0 };
    };
}

#include "huira_impl/assets/lights/point_light.ipp"
