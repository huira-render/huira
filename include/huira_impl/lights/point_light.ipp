#include "huira/core/types.hpp"
#include "huira/detail/sampler.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    LightSample<TSpectral, TFloat> PointLight<TSpectral, TFloat>::sample_Li(const Vec3<TFloat>& point, Sampler<TFloat>& sampler) const
    {
        (void)sampler;
        LightSample<TSpectral, TFloat> sample;

        Vec3<TFloat> to_light = this->global_transform_.position - point;
        sample.distance = length(to_light);
        float distance_sq = static_cast<float>(sample.distance * sample.distance);

        sample.wi = to_light / sample.distance;
        sample.Li = this->spectral_intensity_ / distance_sq;
        sample.pdf = 1.f;  // Delta distribution

        return sample;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    float PointLight<TSpectral, TFloat>::pdf_Li(const Vec3<TFloat>& point, const Vec3<TFloat>& wi) const
    {
        (void)point;
        (void)wi;

        // Delta light - zero probability of sampling any specific direction
        return 0.f;
    }
}
