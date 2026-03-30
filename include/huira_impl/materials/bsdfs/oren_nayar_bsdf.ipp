#include <algorithm>
#include <cmath>

#include "huira/core/constants.hpp"
#include "huira/materials/sampling_utils.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    BSDFRequirements OrenNayarBSDF<TSpectral>::requirements() const
    {
        BSDFRequirements reqs{};
        reqs.needs_albedo = true;
        reqs.needs_normal = true;
        reqs.needs_metallic = false;
        reqs.needs_roughness = true;
        return reqs;
    }

    template <IsSpectral TSpectral>
    TSpectral OrenNayarBSDF<TSpectral>::eval(
        const Vec3<float>& wo,
        const Vec3<float>& wi,
        const Interaction<TSpectral>& isect,
        const ShadingParams<TSpectral>& params) const
    {
        const float cos_i = glm::dot(wi, isect.normal_s);
        const float cos_o = glm::dot(wo, isect.normal_s);

        if (cos_i <= 0.0f || cos_o <= 0.0f) {
            return TSpectral{ 0 };
        }

        // Map linear roughness to the microfacet std-dev angle (sigma) [0, PI/2]
        float sigma = params.roughness * HALF_PI<float>();
        float sigma2 = sigma * sigma;

        // Calculate the Oren-Nayar A and B coefficients
        float A = 1.0f - 0.5f * (sigma2 / (sigma2 + 0.33f));
        float B = 0.45f * (sigma2 / (sigma2 + 0.09f));

        // Optimized algebraic expansion of: max(0, cos(phi_i - phi_o)) * sin(alpha) * tan(beta)
        float d = glm::dot(wi, wo) - cos_i * cos_o;
        float max_cos = std::max(cos_i, cos_o);
        
        float term = 0.0f;
        if (d > 0.0f && max_cos > 1e-6f) {
            term = d / max_cos;
        }

        return params.albedo * INV_PI<float>() * (A + B * term);
    }

    template <IsSpectral TSpectral>
    BSDFSample<TSpectral> OrenNayarBSDF<TSpectral>::sample(
        const Vec3<float>& wo,
        const Interaction<TSpectral>& isect,
        const ShadingParams<TSpectral>& params,
        float u1, float u2) const
    {
        auto hs = sampling::cosine_hemisphere(u1, u2);
        const Vec3<float> wi = sampling::local_to_world(
            hs.direction, isect.tangent, isect.bitangent, isect.normal_s);

        const float cos_i = glm::dot(wi, isect.normal_s);
        const float cos_o = glm::dot(wo, isect.normal_s);

        if (cos_i <= 0.0f || cos_o <= 0.0f) {
            BSDFSample<TSpectral> result{};
            result.wi = wi;
            result.value = TSpectral{ 0 };
            result.pdf = 0.f;
            return result;
        }

        // Calculate the raw Oren-Nayar reflectance
        TSpectral f = this->eval(wo, wi, isect, params);

        BSDFSample<TSpectral> result{};
        result.wi = wi;
        result.value = f * PI<float>();
        result.pdf = hs.pdf;
        return result;
    }

    template <IsSpectral TSpectral>
    float OrenNayarBSDF<TSpectral>::pdf(
        const Vec3<float>& wo,
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
}
