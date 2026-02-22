#include <algorithm>
#include <cmath>

#include "huira/core/constants.hpp"
#include "huira/materials/sampling_utils.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    TSpectral GGXMicrofacetBSDF<TSpectral>::eval(
        const Vec3<float>& wo,
        const Vec3<float>& wi,
        const Interaction<TSpectral>& isect,
        const ShadingParams<TSpectral>& params) const
    {
        const Vec3<float>& n = isect.normal_s;
        const float n_dot_wo = glm::dot(n, wo);
        const float n_dot_wi = glm::dot(n, wi);

        if (n_dot_wo <= 0.0f || n_dot_wi <= 0.0f) {
            return TSpectral{ 0 };
        }

        const float roughness = std::clamp(params.roughness, min_roughness_, 1.0f);
        const float metallic = std::clamp(params.metallic, 0.0f, 1.0f);
        const float alpha = roughness * roughness;
        const float alpha2 = alpha * alpha;

        const TSpectral f0 = params.albedo * metallic
            + TSpectral{ 0.04f } * (1.0f - metallic);

        // Diffuse
        const TSpectral diffuse = params.albedo * ((1.0f - metallic) * INV_PI<float>());

        // Specular
        Vec3<float> h = glm::normalize(wo + wi);
        const float n_dot_h = std::max(glm::dot(n, h), 0.0f);
        const float wo_dot_h = std::max(glm::dot(wo, h), 0.0f);

        const float D = ggx_D(n_dot_h, alpha2);
        const float G = smith_G2(n_dot_wo, n_dot_wi, alpha2);
        const TSpectral F = schlick_fresnel(wo_dot_h, f0);

        const TSpectral specular = F * (D * G / (4.0f * n_dot_wo * n_dot_wi));

        return diffuse + specular;
    }

    template <IsSpectral TSpectral>
    BSDFSample<TSpectral> GGXMicrofacetBSDF<TSpectral>::sample(
        const Vec3<float>& wo,
        const Interaction<TSpectral>& isect,
        const ShadingParams<TSpectral>& params,
        float u1, float u2) const
    {
        const Vec3<float>& n = isect.normal_s;
        const float n_dot_wo = glm::dot(n, wo);
        if (n_dot_wo <= 0.0f) {
            return { .wi = {}, .value = TSpectral{ 0 }, .pdf = 0.0f };
        }

        const float metallic = std::clamp(params.metallic, 0.0f, 1.0f);
        const float roughness = std::clamp(params.roughness, min_roughness_, 1.0f);

        // Lobe selection weight
        const float spec_weight = 0.5f * (1.0f + metallic);

        Vec3<float> wi;
        if (u1 < spec_weight) {
            const float remapped_u1 = u1 / spec_weight;

            Vec3<float> wo_local = sampling::world_to_local(
                wo, isect.tangent, isect.bitangent, n);

            auto ms = sampling::ggx_vndf_sample(wo_local, roughness, remapped_u1, u2);
            Vec3<float> wi_local = glm::reflect(-wo_local, ms.half_vector);

            if (wi_local.z <= 0.0f) {
                return { .wi = {}, .value = TSpectral{ 0 }, .pdf = 0.0f };
            }

            wi = sampling::local_to_world(wi_local, isect.tangent, isect.bitangent, n);
        }
        else {
            const float remapped_u1 = (u1 - spec_weight) / (1.0f - spec_weight);

            auto hs = sampling::cosine_hemisphere(remapped_u1, u2);
            wi = sampling::local_to_world(
                hs.direction, isect.tangent, isect.bitangent, n);
        }

        const float n_dot_wi = glm::dot(n, wi);
        if (n_dot_wi <= 0.0f) {
            return { .wi = wi, .value = TSpectral{ 0 }, .pdf = 0.0f };
        }

        const TSpectral f = eval(wo, wi, isect, params);
        const float p = pdf(wo, wi, isect, params);

        if (p <= 0.0f) {
            return { .wi = wi, .value = TSpectral{ 0 }, .pdf = 0.0f };
        }


        BSDFSample<TSpectral> result{};
        result.wi = wi;
        result.value = f * (n_dot_wi / p);
        result.pdf = p;
        return result;
    }

    template <IsSpectral TSpectral>
    float GGXMicrofacetBSDF<TSpectral>::pdf(
        const Vec3<float>& wo,
        const Vec3<float>& wi,
        const Interaction<TSpectral>& isect,
        const ShadingParams<TSpectral>& params) const
    {
        const Vec3<float>& n = isect.normal_s;
        const float n_dot_wo = glm::dot(n, wo);
        const float n_dot_wi = glm::dot(n, wi);

        if (n_dot_wo <= 0.0f || n_dot_wi <= 0.0f) {
            return 0.0f;
        }

        const float roughness = std::clamp(params.roughness, min_roughness_, 1.0f);
        const float metallic = std::clamp(params.metallic, 0.0f, 1.0f);
        const float alpha = roughness * roughness;
        const float alpha2 = alpha * alpha;

        Vec3<float> h = glm::normalize(wo + wi);
        const float n_dot_h = std::max(glm::dot(n, h), 0.0f);

        const float D = ggx_D(n_dot_h, alpha2);
        const float G1 = smith_G1(n_dot_wo, alpha2);
        const float spec_pdf = D * G1 / (4.0f * n_dot_wo);

        const float diff_pdf = n_dot_wi * INV_PI<float>();

        const float spec_weight = 0.5f * (1.0f + metallic);
        return spec_weight * spec_pdf + (1.0f - spec_weight) * diff_pdf;
    }


    template <IsSpectral TSpectral>
    float GGXMicrofacetBSDF<TSpectral>::ggx_D(float n_dot_h, float alpha2) noexcept {
        const float cos2 = n_dot_h * n_dot_h;
        const float denom = cos2 * (alpha2 - 1.0f) + 1.0f;
        return alpha2 / (std::numbers::pi_v<float> *denom * denom);
    }

    template <IsSpectral TSpectral>
    float GGXMicrofacetBSDF<TSpectral>::smith_G1(float n_dot_v, float alpha2) noexcept {
        const float cos2 = n_dot_v * n_dot_v;
        const float tan2 = (1.0f - cos2) / std::max(cos2, 1e-8f);
        return 2.0f / (1.0f + std::sqrt(1.0f + alpha2 * tan2));
    }

    template <IsSpectral TSpectral>
    float GGXMicrofacetBSDF<TSpectral>::smith_G2(float n_dot_wo, float n_dot_wi, float alpha2) noexcept {
        return smith_G1(n_dot_wo, alpha2) * smith_G1(n_dot_wi, alpha2);
    }

    template <IsSpectral TSpectral>
    TSpectral GGXMicrofacetBSDF<TSpectral>::schlick_fresnel(float cos_theta, const TSpectral& f0) noexcept {
        const float t = 1.0f - cos_theta;
        const float t2 = t * t;
        const float t5 = t2 * t2 * t;
        return f0 + (TSpectral{ 1 } - f0) * t5;
    }
}
