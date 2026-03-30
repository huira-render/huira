#pragma once

#include "huira/materials/bsdfs/bsdf.hpp"

namespace huira {

    /**
     * @brief Lommel-Seeliger BSDF for dark, porous particulate surfaces.
     *
     * The Lommel-Seeliger model approximates single isotropic scattering in a
     * semi-infinite, highly porous medium. It is widely used in planetary astronomy
     * to model the reflectance of low-albedo, dusty bodies such as asteroids and
     * lunar regolith.
     *
     * Unlike Lambertian surfaces which peak in brightness at the sub-solar point,
     * Lommel-Seeliger surfaces exhibit significant limb brightening.
     *
     * f(wo, wi) = albedo / (4 * pi * (cos_i + cos_o))
     *
     * @tparam TSpectral The spectral type used in the rendering pipeline
     */
    template <IsSpectral TSpectral>
    class LommelSeeligerBSDF final : public BSDF<TSpectral> {
    public:
        LommelSeeligerBSDF() noexcept = default;

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
    };

}

#include "huira_impl/materials/bsdfs/lommel_seeliger_bsdf.ipp"
