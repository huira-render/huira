#pragma once

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/objects/lights/light.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class PointLight : public Light<TSpectral, TFloat> {
    public:
        PointLight(Scene<TSpectral, TFloat>* scene, const TSpectral& spectral_intensity)
            : Light<TSpectral, TFloat>(scene)
            , spectral_intensity_{ spectral_intensity }
        {

        }

        // Delete copying:
        PointLight(const PointLight&) = delete;
        PointLight& operator=(const PointLight&) = delete;

        LightSample<TSpectral, TFloat> sample_Li(const Vec3<TFloat>& point, Sampler<TFloat>& sampler) const override;

        float pdf_Li(const Vec3<TFloat>& point, const Vec3<TFloat>& wi) const override;

        LightType get_type() const override { return LightType::Point; }

        std::string get_type_name() const override { return "PointLight"; }

        // Setters for scene editing
        void set_intensity(const TSpectral& intensity) { spectral_intensity_ = intensity; }

    private:
        TSpectral spectral_intensity_{ 0 };
    };
}

#include "huira_impl/objects/lights/point_light.ipp"
