#pragma once

#include "huira/materials/bsdfs/bsdf.hpp"

namespace huira {

    /**
     * @brief Oren-Nayar BSDF for rough diffuse surfaces.
     *
     * An implementation of the Oren-Nayar reflectance model, which extends
     * Lambertian diffuse reflection to account for surface roughness using a
     * V-cavity microfacet distribution.
     *
     * It correctly models the backscattering (retro-reflection) and flattened
     * appearance of rough materials like clay, matte paint, and cloth.
     *
     * The roughness parameter is mapped to the standard deviation of the
     * microfacet angle (sigma) in the range [0, PI/2].
     *
     * @tparam TSpectral The spectral type used in the rendering pipeline
     */
    template <IsSpectral TSpectral>
    class OrenNayarBSDF final : public BSDF<TSpectral> {
    public:
        OrenNayarBSDF() noexcept = default;

        [[nodiscard]] BSDFRequirements requirements() const override;

        [[nodiscard]] std::unique_ptr<BSDF<TSpectral>> clone() const override {
            // Calls the default copy constructor and wraps it in a unique_ptr
            return std::make_unique<OrenNayarBSDF<TSpectral>>(*this);
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

#include "huira_impl/materials/bsdfs/oren_nayar_bsdf.ipp"
