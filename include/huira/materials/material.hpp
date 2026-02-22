#pragma once

#include <memory>
#include <cstdint>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"
#include "huira/materials/bsdfs/bsdf.hpp"
#include "huira/materials/shading_params.hpp"
#include "huira/render/interaction.hpp"
#include "huira/scene/scene_object.hpp"

namespace huira {
    // Forward Declare
    template <IsSpectral TSpectral>
    class Scene;

    /**
     * @brief Result of texture evaluation at a surface point.
     */
    template <IsSpectral TSpectral>
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
    class Material : public SceneObject<Material<TSpectral>, TSpectral> {
    public:
        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        [[nodiscard]] MaterialEval<TSpectral> evaluate(const Interaction<TSpectral>& isect) const;

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

        void set_albedo(const Image<TSpectral>* albedo_image);
        void set_albedo_factor(TSpectral albedo_factor);
        void reset_albedo();

        std::uint64_t id() const override { return id_; }
        std::string type() const override { return "Material"; }

    private:
        Material(
            std::unique_ptr<BSDF<TSpectral>> bsdf,
            Image<TSpectral>* albedo_image,
            Image<float>* metallic_image,
            Image<float>* roughness_image,
            Image<Vec3<float>>* normal_image,
            Image<TSpectral>* emissive_image);

        std::unique_ptr<BSDF<TSpectral>> bsdf_;
        
        Image<TSpectral>*   albedo_image_;
        Image<float>*       metallic_image_;
        Image<float>*       roughness_image_;
        Image<Vec3<float>>* normal_image_;
        Image<TSpectral>*   emissive_image_;

        Image<TSpectral>*   default_albedo_image_;
        Image<float>*       default_metallic_image_;
        Image<float>*       default_roughness_image_;
        Image<Vec3<float>>* default_normal_image_;
        Image<TSpectral>*   default_emissive_image_;
        
        TSpectral albedo_factor_{ 1.0f };
        float     metallic_factor_  = 1.0f;
        float     roughness_factor_ = 1.0f;
        TSpectral emissive_factor_{ 1.0f };
        float     normal_scale_     = 1.0f;

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;

        friend class Scene<TSpectral>;
    };

}

#include "huira_impl/materials/material.ipp"
