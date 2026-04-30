#pragma once

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/handles/materials/texture_handle.hpp"
#include "huira/materials/material.hpp"

namespace huira {
    /**
     * @brief Handle for manipulating a Material in a scene.
     *
     * Provides a safe, reference-like interface for configuring and querying a Material
     * instance.  All operations are forwarded to the underlying Material.
     *
     * @tparam TSpectral The spectral type
     */
    template <IsSpectral TSpectral>
    class MaterialHandle : public Handle<Material<TSpectral>> {
    public:
        MaterialHandle() = delete;
        using Handle<Material<TSpectral>>::Handle;

        void set_bsdf(std::shared_ptr<BSDF<TSpectral>> bsdf);

        void set_albedo_image(const TextureHandle<TSpectral>& albedo_texture);
        void set_albedo_factor(TSpectral albedo_factor);
        void reset_albedo();

        void set_alpha_image(const TextureHandle<float>& alpha_texture);
        void set_alpha_factor(float alpha_factor);
        void reset_alpha();

        void set_metallic_image(const TextureHandle<float>& metallic_texture);
        void set_metallic_factor(float metallic_factor);
        void reset_metallic();

        void set_roughness_image(const TextureHandle<float>& roughness_texture);
        void set_roughness_factor(float roughness_factor);
        void reset_roughness();

        void set_normal_image(const TextureHandle<Vec3<float>>& normal_texture);
        void set_normal_factor(float normal_factor);
        void reset_normal();

        void set_transmission_image(const TextureHandle<TSpectral>& transmission_texture);
        void set_transmission_factor(TSpectral normal_factor);
        void reset_transmission();

        void set_emissive_image(const TextureHandle<TSpectral>& emissive_texture);
        void set_emissive_factor(TSpectral emissive_factor);
        void reset_emissive();
    };
}

#include "huira_impl/handles/material_handle.ipp"
