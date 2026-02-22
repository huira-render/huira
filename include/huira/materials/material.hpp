#pragma once

#include <memory>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"
#include "huira/materials/bsdfs/bsdf.hpp"
#include "huira/materials/shading_params.hpp"
#include "huira/render/interaction.hpp"

namespace huira {
    /**
     * @brief Result of texture evaluation at a surface point.
     */
    template <IsSpecctral TSpectral>
    struct MaterialEval {
        ShadingParams<TSpectral> params;
        Interaction<TSpectral>   isect;
    };

    /**
     * @brief Surface material: holds image pointers and a BSDF pointer, provides
     *        the primary shading interface for integrators and rasterizers.
     *
     * Material is a concrete class. It does not own any of its referenced data —
     * the Scene owns all Images, BSDFs, and Materials. Material holds non-owning
     * pointers to Scene-managed assets.
     *
     * Every image slot is a non-null pointer. For slots without a texture, the
     * Scene provides a 1x1 Image filled with the appropriate default value:
     *
     *   - base_color_image:  1x1 white (TSpectral{1})
     *   - metallic_image:    1x1 with metallic_factor (typically 0.0)
     *   - roughness_image:   1x1 with roughness_factor (typically 0.5)
     *   - normal_image:      1x1 with {0.5, 0.5, 1.0} (unperturbed normal)
     *   - emissive_image:    1x1 black (TSpectral{0})
     *
     * @tparam TSpectral The spectral type used in the rendering pipeline
     */
    template <IsSpectral TSpectral>
    class Material {
    public:

        [[nodiscard]] MaterialEval<TSpectral> evaluate(const Interaction<TSpectral>& isect) const;

        [[nodiscard]] bool is_emissive() const noexcept;

        [[nodiscard]] TSpectral emitted(const Interaction<TSpectral>& isect) const;

        [[nodiscard]] TSpectral bsdf_eval(
            const Vec3<float>& wo,
            const Vec3<float>& wi,
            const MaterialEval<TSpectral>& eval) const;

        [[nodiscard]] BSDFSample<TSpectral> bsdf_sample(
            const Vec3<float>& wo,
            const MaterialEval<TSpectral>& eval,
            float u1, float u2) const;

        [[nodiscard]] float bsdf_pdf(
            const Vec3<float>& wo,
            const Vec3<float>& wi,
            const MaterialEval<TSpectral>& eval) const;

    private:
        std::unique_ptr<BSDF<TSpectral>> bsdf = nullptr;
        
        Image<TSpectral>*   base_color_image = nullptr;
        Image<float>*       metallic_image   = nullptr;
        Image<float>*       roughness_image  = nullptr;
        Image<Vec3<float>>* normal_image     = nullptr;
        Image<TSpectral>*   emissive_image   = nullptr;
        
        TSpectral base_color_factor{ 1 };
        float     metallic_factor  = 0.0f;
        float     roughness_factor = 0.5f;
        TSpectral emissive_factor{ 0 };
        float     normal_scale     = 1.0f;
    };

}

#include "huira_impl/materials/material.ipp"
