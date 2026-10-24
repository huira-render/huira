#include <algorithm>
#include <cmath>

#include "huira/core/constants.hpp"
#include "huira/materials/bsdfs/cook_torrance_lobe.hpp"
#include "huira/materials/sampling_utils.hpp"
#include "huira_impl/materials/bsdfs/cook_torrance_albedo_lookup.ipp"

namespace huira {

template <IsSpectral TSpectral>
BSDFRequirements CookTorranceBSDF<TSpectral>::requirements() const
{
    BSDFRequirements reqs{};
    reqs.needs_albedo = true;
    reqs.needs_normal = true;
    reqs.needs_metallic = true;
    reqs.needs_roughness = true;
    return reqs;
}

template <IsSpectral TSpectral>
TSpectral CookTorranceBSDF<TSpectral>::eval(const Vec3<float>& wo,
                                            const Vec3<float>& wi,
                                            const Interaction<TSpectral>& isect,
                                            const ShadingParams<TSpectral>& params) const
{
    const Vec3<float>& n = isect.normal_s;
    const float n_dot_wo = glm::dot(n, wo);
    const float n_dot_wi = glm::dot(n, wi);

    if (n_dot_wo <= 0.0f || n_dot_wi <= 0.0f) {
        return TSpectral{0};
    }

    const float roughness = std::clamp(params.roughness, min_roughness_, 1.0f);
    const float metallic = std::clamp(params.metallic, 0.0f, 1.0f);
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;

    const TSpectral f0 = params.albedo * metallic +
                         TSpectral{detail::cook_torrance::DIELECTRIC_F0} * (1.0f - metallic);

    // Diffuse (Lambertian, scaled by 1 - metallic), of only the light the dielectric's specular
    // lobe does not reflect: 1 - E(cos_i) of the light from wi, weighted by 1 - E(cos_o) toward
    // wo, which keeps it reciprocal, and divided by 1 - E_avg, which makes it reflect albedo * (1
    // - E(cos_o)) of the light toward wo, all that the lobe leaves (see the class).
    static_assert(min_roughness_ == detail::DIELECTRIC_ALBEDO_MIN_ROUGHNESS,
                  "The albedo table must start at the BSDF's minimum roughness: rebuild it with "
                  "tools/build_cook_torrance_albedo");
    const detail::DielectricSpecularAlbedo specular_albedo(roughness);
    const float diffuse_weight = (1.0f - specular_albedo(n_dot_wi)) *
                                 (1.0f - specular_albedo(n_dot_wo)) /
                                 (1.0f - specular_albedo.average());
    const TSpectral diffuse =
        params.albedo * ((1.0f - metallic) * diffuse_weight * INV_PI<float>());

    // Specular (Cook-Torrance microfacet with GGX NDF)
    Vec3<float> h = glm::normalize(wo + wi);
    const float wo_dot_h = std::max(glm::dot(wo, h), 0.0f);

    const float D = ggx_D(n, h, alpha2);
    const float G = detail::cook_torrance::smith_G2(n_dot_wo, n_dot_wi, alpha2);
    const TSpectral F = f0 + (TSpectral{1} - f0) * detail::cook_torrance::schlick_weight(wo_dot_h);

    const TSpectral specular = F * (D * G / (4.0f * n_dot_wo * n_dot_wi));

    return diffuse + specular;
}

template <IsSpectral TSpectral>
BSDFSample<TSpectral> CookTorranceBSDF<TSpectral>::sample(const Vec3<float>& wo,
                                                          const Interaction<TSpectral>& isect,
                                                          const ShadingParams<TSpectral>& params,
                                                          float u1,
                                                          float u2) const
{
    const Vec3<float>& n = isect.normal_s;
    const float n_dot_wo = glm::dot(n, wo);
    if (n_dot_wo <= 0.0f) {
        BSDFSample<TSpectral> result{};
        result.wi = {};
        result.value = TSpectral{0};
        result.pdf = 0.0f;
        return result;
    }

    const float metallic = std::clamp(params.metallic, 0.0f, 1.0f);
    const float roughness = std::clamp(params.roughness, min_roughness_, 1.0f);

    // One-sample MIS: lobe selection weight biased by metallic
    // (more metallic -> more likely to sample specular lobe)
    const float spec_weight = 0.5f * (1.0f + metallic);

    Vec3<float> wi;
    if (u1 < spec_weight) {
        // Sample specular lobe via GGX VNDF
        const float remapped_u1 = u1 / spec_weight;

        Vec3<float> wo_local = sampling::world_to_local(wo, isect.tangent, isect.bitangent, n);

        auto ms = sampling::ggx_vndf_sample(wo_local, roughness, remapped_u1, u2);
        Vec3<float> wi_local = glm::reflect(-wo_local, ms.half_vector);

        if (wi_local.z <= 0.0f) {
            BSDFSample<TSpectral> result{};
            result.wi = {};
            result.value = TSpectral{0};
            result.pdf = 0.0f;
            return result;
        }

        wi = sampling::local_to_world(wi_local, isect.tangent, isect.bitangent, n);
    } else {
        // Sample diffuse lobe via cosine-weighted hemisphere
        const float remapped_u1 = (u1 - spec_weight) / (1.0f - spec_weight);

        auto hs = sampling::cosine_hemisphere(remapped_u1, u2);
        wi = sampling::local_to_world(hs.direction, isect.tangent, isect.bitangent, n);
    }

    const float n_dot_wi = glm::dot(n, wi);
    if (n_dot_wi <= 0.0f) {
        BSDFSample<TSpectral> result{};
        result.wi = wi;
        result.value = TSpectral{0};
        result.pdf = 0.0f;
        return result;
    }

    const TSpectral f = eval(wo, wi, isect, params);
    const float p = pdf(wo, wi, isect, params);

    if (p <= 0.0f) {
        BSDFSample<TSpectral> result{};
        result.wi = wi;
        result.value = TSpectral{0};
        result.pdf = 0.0f;
        return result;
    }

    BSDFSample<TSpectral> result{};
    result.wi = wi;
    result.value = f * (n_dot_wi / p);
    result.pdf = p;
    return result;
}

template <IsSpectral TSpectral>
float CookTorranceBSDF<TSpectral>::pdf(const Vec3<float>& wo,
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

    // Specular PDF: GGX VNDF
    const float D = ggx_D(n, h, alpha2);
    const float G1 = detail::cook_torrance::smith_G1(n_dot_wo, alpha2);
    const float spec_pdf = D * G1 / (4.0f * n_dot_wo);

    // Diffuse PDF: cosine-weighted hemisphere
    const float diff_pdf = n_dot_wi * INV_PI<float>();

    // Combined via one-sample MIS weights
    const float spec_weight = 0.5f * (1.0f + metallic);
    return spec_weight * spec_pdf + (1.0f - spec_weight) * diff_pdf;
}

/**
 * @brief GGX D for the microfacet normal h, zero below the surface. The sine of h's angle from
 * n comes from their cross product, which keeps its precision for the small angles a smooth
 * surface's lobe spans (see sampling::ggx_distribution()).
 */
template <IsSpectral TSpectral>
float CookTorranceBSDF<TSpectral>::ggx_D(const Vec3<float>& n,
                                         const Vec3<float>& h,
                                         float alpha2) noexcept
{
    const float n_dot_h = glm::dot(n, h);
    if (n_dot_h <= 0.0f) {
        return 0.0f;
    }
    const Vec3<float> n_cross_h = glm::cross(n, h);
    return sampling::ggx_distribution(n_dot_h * n_dot_h, glm::dot(n_cross_h, n_cross_h), alpha2);
}

} // namespace huira
