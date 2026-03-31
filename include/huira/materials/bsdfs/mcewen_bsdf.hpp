#pragma once

#include "huira/materials/bsdfs/bsdf.hpp"

namespace huira {

    /**
     * @brief McEwen BSDF for photometric planetary surface rendering.
     *
     * An empirically derived reflectance model by Alfred S. McEwen (1991) that blends
     * Lambertian and Lommel-Seeliger scattering based on the phase angle (the angle
     * between the incident light and the observer).
     *
     * It effectively captures the photometric properties of rough, dusty planetary
     * surfaces like the Moon. The mixing fraction beta is calculated as exp(-alpha / alpha_0),
     * where alpha is the phase angle and alpha_0 is a 60-degree falloff constant.
     *
     * f(wo, wi) = albedo * ((1 - beta) * Lambert + beta * Lommel_Seeliger)
     *
     * @tparam TSpectral The spectral type used in the rendering pipeline
     */
    template <IsSpectral TSpectral>
    class McEwenBSDF final : public BSDF<TSpectral> {
    public:
        McEwenBSDF() noexcept = default;

        [[nodiscard]] BSDFRequirements requirements() const override;

        [[nodiscard]] std::unique_ptr<BSDF<TSpectral>> clone() const override {
            // Calls the default copy constructor and wraps it in a unique_ptr
            return std::make_unique<McEwenBSDF<TSpectral>>(*this);
        }

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
    };

}

#include "huira_impl/materials/bsdfs/mcewen_bsdf.ipp"
