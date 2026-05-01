#pragma once

namespace huira {

template <IsSpectral TSpectral>
BSDFRequirements NullBSDF<TSpectral>::requirements() const
{
    return BSDFRequirements{false, false, false, false};
}

template <IsSpectral TSpectral>
TSpectral NullBSDF<TSpectral>::eval(const Vec3<float>& wo,
                                    const Vec3<float>& wi,
                                    const Interaction<TSpectral>& isect,
                                    const ShadingParams<TSpectral>& params) const
{
    (void)wo;
    (void)wi;
    (void)isect;
    (void)params;
    return TSpectral{0.0f};
}

template <IsSpectral TSpectral>
BSDFSample<TSpectral> NullBSDF<TSpectral>::sample(const Vec3<float>& wo,
                                                  const Interaction<TSpectral>& isect,
                                                  const ShadingParams<TSpectral>& params,
                                                  float u1,
                                                  float u2) const
{
    (void)wo;
    (void)isect;
    (void)params;
    (void)u1;
    (void)u2;

    BSDFSample<TSpectral> result;
    result.wi = -wo;
    result.value = TSpectral{1.0f};
    result.pdf = 1.0f;
    result.is_delta = true;

    return result;
}

template <IsSpectral TSpectral>
float NullBSDF<TSpectral>::pdf(const Vec3<float>& wo,
                               const Vec3<float>& wi,
                               const Interaction<TSpectral>& isect,
                               const ShadingParams<TSpectral>& params) const
{
    (void)wo;
    (void)wi;
    (void)isect;
    (void)params;
    return 0.0f;
}

} // namespace huira
