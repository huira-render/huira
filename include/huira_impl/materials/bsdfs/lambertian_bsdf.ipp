#include "huira/core/constants.hpp"
#include "huira/materials/sampling_utils.hpp"

namespace huira {
template <IsSpectral TSpectral>
BSDFRequirements LambertianBSDF<TSpectral>::requirements() const
{
    BSDFRequirements reqs{};
    reqs.needs_albedo = true;
    reqs.needs_normal = true;
    reqs.needs_metallic = false;
    reqs.needs_roughness = false;
    return reqs;
}

template <IsSpectral TSpectral>
TSpectral LambertianBSDF<TSpectral>::eval(const Vec3<float>& wo,
                                          const Vec3<float>& wi,
                                          const Interaction<TSpectral>& isect,
                                          const ShadingParams<TSpectral>& params) const
{
    (void)wo;

    const float cos_theta_i = glm::dot(wi, isect.normal_s);
    if (cos_theta_i <= 0.0f) {
        return TSpectral{0};
    }
    return params.albedo * INV_PI<float>();
}

template <IsSpectral TSpectral>
BSDFSample<TSpectral> LambertianBSDF<TSpectral>::sample(const Vec3<float>& wo,
                                                        const Interaction<TSpectral>& isect,
                                                        const ShadingParams<TSpectral>& params,
                                                        float u1,
                                                        float u2) const
{
    (void)wo;

    auto hs = sampling::cosine_hemisphere(u1, u2);
    const Vec3<float> wi =
        sampling::local_to_world(hs.direction, isect.tangent, isect.bitangent, isect.normal_s);

    const float cos_theta_i = glm::dot(wi, isect.normal_s);
    if (cos_theta_i <= 0.0f) {
        BSDFSample<TSpectral> result{};
        result.wi = wi;
        result.value = TSpectral{0};
        result.pdf = 0.f;
        return result;
    }

    // f * |cos| / pdf = (base_color/pi) * cos / (cos/pi) = base_color
    BSDFSample<TSpectral> result{};
    result.wi = wi;
    result.value = params.albedo;
    result.pdf = hs.pdf;
    return result;
}

template <IsSpectral TSpectral>
float LambertianBSDF<TSpectral>::pdf(const Vec3<float>& wo,
                                     const Vec3<float>& wi,
                                     const Interaction<TSpectral>& isect,
                                     const ShadingParams<TSpectral>& params) const
{
    (void)wo;
    (void)params;

    const float cos_theta_i = glm::dot(wi, isect.normal_s);
    if (cos_theta_i <= 0.0f) {
        return 0.0f;
    }
    return cos_theta_i * INV_PI<float>();
}
} // namespace huira
