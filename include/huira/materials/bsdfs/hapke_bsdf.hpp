#pragma once

#include "huira/materials/bsdfs/bsdf.hpp"

namespace huira {

    /**
     * @brief Hapke BSDF (5-Parameter Model) for planetary regolith.
     *
     * An implementation of the classic 5-parameter Hapke photometric model,
     * widely used for rendering asteroids, moons, and dusty planetary bodies.
     * * It simulates complex particulate scattering including:
     * - Shadow-Hiding Opposition Effect (Surge in brightness at 0 phase angle)
     * - Double Henyey-Greenstein phase function (Forward/Backward scattering)
     * - Isotropic multiple scattering approximation via Chandrasekhar H-functions
     *
     * Parameters:
     * - omega: Single scattering albedo (Provided spatially via params.albedo)
     * - h:     Opposition effect width (porosity/compaction)
     * - B0:    Opposition effect amplitude
     * - b:     DHG phase function asymmetry
     * - c:     DHG phase function forward/backward fraction
     *
     * @tparam TSpectral The spectral type used in the rendering pipeline
     */
    template <IsSpectral TSpectral>
    class HapkeBSDF final : public BSDF<TSpectral> {
    public:
        HapkeBSDF(float h, float B0, float b, float c) noexcept
            : h_(h), B0_(B0), b_(b), c_(c) {}

        [[nodiscard]] BSDFRequirements requirements() const override;

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
        float h_;
        float B0_;
        float b_;
        float c_;

        [[nodiscard]] float phase_function_dhg(float cos_alpha) const noexcept;
        [[nodiscard]] float opposition_effect(float tan_half_alpha) const noexcept;
        [[nodiscard]] TSpectral h_function(float mu, const TSpectral& omega) const noexcept;
    };

}

#include "huira_impl/materials/bsdfs/hapke_bsdf.ipp"
