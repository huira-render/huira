#include "huira/core/types.hpp"
#include "huira/detail/sampler.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    LightSample<TSpectral> PointLight<TSpectral>::sample_Li(const Vec3<float>& point, Sampler<float>& sampler) const
    {
        (void)sampler;
        LightSample<TSpectral> sample;

        Vec3<float> to_light = this->global_transform_.position - point;
        sample.distance = length(to_light);
        float distance_sq = static_cast<float>(sample.distance * sample.distance);

        sample.wi = to_light / sample.distance;
        sample.Li = this->spectral_intensity_ / distance_sq;
        sample.pdf = 1.f;  // Delta distribution

        return sample;
    }

    template <IsSpectral TSpectral>
    float PointLight<TSpectral>::pdf_Li(const Vec3<float>& point, const Vec3<float>& wi) const
    {
        (void)point;
        (void)wi;

        // Delta light - zero probability of sampling any specific direction
        return 0.f;
    }
}
