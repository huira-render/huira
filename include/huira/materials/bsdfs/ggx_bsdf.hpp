#pragma once

#include "huira/materials/bsdfs/bsdf.hpp"

namespace huira {

    /**
     * @brief GGX microfacet BSDF with metallic-roughness parameterization.
     *
     * Implements the Cook-Torrance microfacet model:
     *
     *   f(wo, wi) = D(h) * F(wo, h) * G(wo, wi) / (4 * |cos_o| * |cos_i|)
     *             + (1 - metallic) * base_color / pi
     *
     * Where:
     *   D = GGX (Trowbridge-Reitz) normal distribution
     *   F = Schlick Fresnel approximation
     *   G = Smith height-correlated masking-shadowing
     *
     * All spatially-varying parameters (base_color, roughness, metallic) come
     * from ShadingParams. This BSDF is stateless and a single instance can be
     * shared across all PBR materials in the scene.
     *
     * Uses VNDF sampling (Heitz 2018) for the specular lobe combined with
     * cosine-weighted sampling for the diffuse lobe via one-sample MIS.
     *
     * @tparam TSpectral The spectral type used in the rendering pipeline
     */
    template <IsSpectral TSpectral>
    class GGXMicrofacetBSDF final : public BSDF<TSpectral> {
    public:
        GGXMicrofacetBSDF() noexcept = default;

        [[nodiscard]] TSpectral eval(
            const Vec3<float>& wo,
            const Vec3<float>& wi,
            const Interaction<TSpectral>& isect,
            const ShadingParams<TSpectral>& params) const override;

        [[nodiscard]] BSDFSample<TSpectral> sample(
            const Vec3<float>& wo,
            const Interaction<TSpectral>& isect,
            const ShadingParams<TSpectral>& params,
            float u1, float u2) const override;

        [[nodiscard]] float pdf(
            const Vec3<float>& wo,
            const Vec3<float>& wi,
            const Interaction<TSpectral>& isect,
            const ShadingParams<TSpectral>& params) const override;

    private:
        static constexpr float min_roughness_ = 0.01f;

        [[nodiscard]] static float ggx_D(float n_dot_h, float alpha2) noexcept;

        [[nodiscard]] static float smith_G1(float n_dot_v, float alpha2) noexcept;

        [[nodiscard]] static float smith_G2(float n_dot_wo, float n_dot_wi, float alpha2) noexcept;

        [[nodiscard]] static TSpectral schlick_fresnel(float cos_theta, const TSpectral& f0) noexcept;
    };

}

#include "huira_impl/materials/bsdfs/ggx_bsdf.ipp"
