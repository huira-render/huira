#include <algorithm>
#include <cmath>

#include "huira/core/constants.hpp"
#include "huira/materials/sampling_utils.hpp"

namespace huira {
template <IsSpectral TSpectral>
BSDFRequirements McEwenBSDF<TSpectral>::requirements() const
{
    BSDFRequirements reqs{};
    reqs.needs_albedo = true;
    reqs.needs_normal = true;
    reqs.needs_metallic = false;
    reqs.needs_roughness = false;
    return reqs;
}

template <IsSpectral TSpectral>
TSpectral McEwenBSDF<TSpectral>::eval(const Vec3<float>& wo,
                                      const Vec3<float>& wi,
                                      const Interaction<TSpectral>& isect,
                                      const ShadingParams<TSpectral>& params) const
{
    const float cos_i = glm::dot(wi, isect.normal_s);
    const float cos_o = glm::dot(wo, isect.normal_s);

    if (cos_i <= 0.0f || cos_o <= 0.0f) {
        return TSpectral{0};
    }

    // Phase angle (angle between incoming and outgoing light directions)
    float cos_alpha = std::clamp(glm::dot(wi, wo), -1.0f, 1.0f);
    float alpha = std::acos(cos_alpha);

    // McEwen's empirically derived beta factor
    // 60 degrees = PI / 3 radians, so division by (PI/3) is multiplication by (3/PI)
    constexpr float inv_alpha0 = 3.0f * INV_PI<float>();
    float beta = std::exp(-alpha * inv_alpha0);

    float lambert = INV_PI<float>();
    float lommel_seeliger = 1.0f / (4.0f * PI<float>() * (cos_i + cos_o));

    return params.albedo * ((1.0f - beta) * lambert + beta * lommel_seeliger);
}

template <IsSpectral TSpectral>
BSDFSample<TSpectral> McEwenBSDF<TSpectral>::sample(const Vec3<float>& wo,
                                                    const Interaction<TSpectral>& isect,
                                                    const ShadingParams<TSpectral>& params,
                                                    float u1,
                                                    float u2) const
{
    auto hs = sampling::cosine_hemisphere(u1, u2);
    const Vec3<float> wi =
        sampling::local_to_world(hs.direction, isect.tangent, isect.bitangent, isect.normal_s);

    const float cos_i = glm::dot(wi, isect.normal_s);
    const float cos_o = glm::dot(wo, isect.normal_s);

    if (cos_i <= 0.0f || cos_o <= 0.0f) {
        BSDFSample<TSpectral> result{};
        result.wi = wi;
        result.value = TSpectral{0};
        result.pdf = 0.f;
        return result;
    }

    TSpectral f = this->eval(wo, wi, isect, params);

    BSDFSample<TSpectral> result{};
    result.wi = wi;
    result.value = f * PI<float>();
    result.pdf = hs.pdf;
    return result;
}

template <IsSpectral TSpectral>
float McEwenBSDF<TSpectral>::pdf(const Vec3<float>& wo,
                                 const Vec3<float>& wi,
                                 const Interaction<TSpectral>& isect,
                                 const ShadingParams<TSpectral>& params) const
{
    (void)params;

    const float cos_i = glm::dot(wi, isect.normal_s);
    const float cos_o = glm::dot(wo, isect.normal_s);

    if (cos_i <= 0.0f || cos_o <= 0.0f) {
        return 0.0f;
    }

    return cos_i * INV_PI<float>();
}
} // namespace huira
