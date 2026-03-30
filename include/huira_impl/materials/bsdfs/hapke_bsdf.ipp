#include <algorithm>
#include <cmath>

#include "huira/core/constants.hpp"
#include "huira/materials/sampling_utils.hpp"

namespace huira {

    template <IsSpectral TSpectral>
    BSDFRequirements HapkeBSDF<TSpectral>::requirements() const
    {
        BSDFRequirements reqs{};
        reqs.needs_albedo = true;
        reqs.needs_normal = true;
        reqs.needs_metallic = false;
        reqs.needs_roughness = false;
        return reqs;
    }

    template <IsSpectral TSpectral>
    float HapkeBSDF<TSpectral>::phase_function_dhg(float cos_alpha) const noexcept
    {
        // Double Henyey-Greenstein (DHG) phase function
        auto hg = [](float g, float cos_a) {
            float g2 = g * g;
            float denom = 1.0f + g2 - 2.0f * g * cos_a;
            return (1.0f - g2) / std::pow(denom, 1.5f);
            };
        
        return (1.0f - c_) * hg(-b_, cos_alpha) + c_ * hg(b_, cos_alpha);
    }

    template <IsSpectral TSpectral>
    float HapkeBSDF<TSpectral>::opposition_effect(float tan_half_alpha) const noexcept
    {
        // Shadow-Hiding Opposition Effect (SHOE)
        // B(alpha) = B0 / (1 + (1/h) * tan(alpha/2))
        if (h_ <= 0.0f) return 0.0f;
        return B0_ / (1.0f + (tan_half_alpha / h_));
    }

    template <IsSpectral TSpectral>
    TSpectral HapkeBSDF<TSpectral>::h_function(float mu, const TSpectral& omega) const noexcept
    {
        // Analytical approximation of Chandrasekhar's H-function
        // H(mu) = (1 + 2*mu) / (1 + 2*mu * sqrt(1 - omega))
        TSpectral inv_omega = 1.0f - omega;
        TSpectral gamma = inv_omega.sqrt();
        return (1.0f + 2.0f * mu) / (1.0f + gamma * (2.0f * mu));
    }
    

    template <IsSpectral TSpectral>
    TSpectral HapkeBSDF<TSpectral>::eval(
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

        const TSpectral& omega = params.albedo;

        // Phase angle calculations
        float cos_alpha = std::clamp(glm::dot(wi, wo), -1.0f, 1.0f);
        float alpha = std::acos(cos_alpha);
        float tan_half_alpha = std::tan(alpha * 0.5f);

        // Single Scattering Phase Function
        float P = phase_function_dhg(cos_alpha);

        // Opposition Effect
        float B = opposition_effect(tan_half_alpha);

        // Multiple Scattering (H-Functions)
        TSpectral H_i = h_function(cos_i, omega);
        TSpectral H_o = h_function(cos_o, omega);

        // Full Hapke Reflectance Formula:
        // f = (omega / (4 * pi * (cos_i + cos_o))) * [ (1 + B) * P + H(cos_i)*H(cos_o) - 1 ]

        TSpectral lommel_seeliger_base = omega / (4.0f * PI<float>() * (cos_i + cos_o));
        TSpectral bracket_term = TSpectral{ (1.0f + B) * P - 1.0f } + (H_i * H_o);

        return lommel_seeliger_base * bracket_term;
    }

    template <IsSpectral TSpectral>
    BSDFSample<TSpectral> HapkeBSDF<TSpectral>::sample(
        const Vec3<float>& wo,
        const Interaction<TSpectral>& isect,
        const ShadingParams<TSpectral>& params,
        float u1, float u2) const
    {
        // Cosine-weighted hemisphere sampling
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

        TSpectral f = this->eval(wo, wi, isect, params);

        BSDFSample<TSpectral> result{};
        result.wi = wi;
        result.value = f * PI<float>(); // f * cos_i / (cos_i / pi)
        result.pdf = hs.pdf;
        return result;
    }

    template <IsSpectral TSpectral>
    float HapkeBSDF<TSpectral>::pdf(
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
