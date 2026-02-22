#include "huira/core/constants.hpp"
#include "huira/materials/sampling_utils.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    TSpectral LambertBSDF<TSpectral>::eval(
        const Vec3<float>& wo,
        const Vec3<float>& wi,
        const Interaction<TSpectral>& isect,
        const ShadingParams<TSpectral>& params) const
    {
        const float cos_theta_i = glm::dot(wi, isect.normal_s);
        if (cos_theta_i <= 0.0f) {
            return TSpectral{ 0 };
        }
        return params.base_color * INV_PI<float>();
    }

    template <IsSpectral TSpectral>
    BSDFSample<TSpectral> LambertBSDF<TSpectral>::sample(
        const Vec3<float>& wo,
        const Interaction<TSpectral>& isect,
        const ShadingParams<TSpectral>& params,
        float u1, float u2) const
    {
        auto hs = sampling::cosine_hemisphere(u1, u2);
        const Vec3<float> wi = sampling::local_to_world(
            hs.direction, isect.tangent, isect.bitangent, isect.normal_s);

        const float cos_theta_i = glm::dot(wi, isect.normal_s);
        if (cos_theta_i <= 0.0f) {
            return { .wi = wi, .value = TSpectral{ 0 }, .pdf = 0.0f };
        }

        // f * |cos| / pdf = (base_color/pi) * cos / (cos/pi) = base_color
        BSDFSample<TSpectral> result{};
        result.wi = wi;
        result.value = value;
        result.pdf = hs.pdf;
        return result;
    }

    template <IsSpectral TSpectral>
    float LambertBSDF<TSpectral>::pdf(
        const Vec3<float>& wo,
        const Vec3<float>& wi,
        const Interaction<TSpectral>& isect,
        const ShadingParams<TSpectral>& params) const
    {
        const float cos_theta_i = glm::dot(wi, isect.normal_s);
        if (cos_theta_i <= 0.0f) {
            return 0.0f;
        }
        return cos_theta_i * INV_PI<float>();
    }
}
