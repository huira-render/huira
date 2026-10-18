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
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class MaterialHandle : public Handle<Material<TSpectral>> {
  public:
    MaterialHandle() = delete;
    using Handle<Material<TSpectral>>::Handle;

    void set_bsdf(std::shared_ptr<BSDF<TSpectral>> bsdf);

    // Every material value is an image times a factor. set_X(value) makes it a constant (a
    // 1x1 image), set_X_image() a texture, and set_X_factor() a multiplier on either, 1 by
    // default. None of them changes another. reset_X() restores the default and a factor of 1.
    // Each setter validates its argument. See Material for the defaults.

    /// Set a constant albedo, from 0 to 1 in each bin.
    void set_albedo(TSpectral albedo);
    void set_albedo_image(const TextureHandle<TSpectral>& albedo_texture);
    /// Multiplies the albedo image; from 0 to 1 in each bin.
    void set_albedo_factor(TSpectral albedo_factor);
    void reset_albedo();

    /// Set a constant opacity, from 0 (clear) to 1 (opaque).
    void set_alpha(float alpha);
    void set_alpha_image(const TextureHandle<float>& alpha_texture);
    /// Multiplies the alpha image; from 0 to 1.
    void set_alpha_factor(float alpha_factor);
    void reset_alpha();

    /// Set a constant metalness, from 0 (dielectric) to 1 (metal).
    void set_metallic(float metallic);
    void set_metallic_image(const TextureHandle<float>& metallic_texture);
    /// Multiplies the metallic image; from 0 to 1.
    void set_metallic_factor(float metallic_factor);
    void reset_metallic();

    /// Set a constant roughness, from 0 (mirror) to 1 (rough).
    void set_roughness(float roughness);
    void set_roughness_image(const TextureHandle<float>& roughness_texture);
    /// Multiplies the roughness image; from 0 to 1.
    void set_roughness_factor(float roughness_factor);
    void reset_roughness();

    void set_normal_image(const TextureHandle<Vec3<float>>& normal_texture);
    /// Scales the normal map's perturbation; 1 leaves it as is.
    void set_normal_factor(float normal_factor);
    void reset_normal();

    /// Set a constant transmission: the fraction of light that passes through, from 0 to 1.
    void set_transmission(TSpectral transmission);
    void set_transmission_image(const TextureHandle<TSpectral>& transmission_texture);
    /// Multiplies the transmission image; from 0 to 1 in each bin.
    void set_transmission_factor(TSpectral transmission_factor);
    void reset_transmission();

    /// Set a constant emission: the radiance the surface emits, from either side, in
    /// W/(m^2 sr) in each spectral bin. Finite and non-negative.
    void set_emissive(TSpectral emissive);
    /// Set the emissive texture, in W/(m^2 sr) per bin; shown as it is until a factor is set.
    void set_emissive_image(const TextureHandle<TSpectral>& emissive_texture);
    /// Multiplies the emissive image; finite and non-negative.
    void set_emissive_factor(TSpectral emissive_factor);
    /// Remove the emissive texture and factor: the surface emits nothing.
    void reset_emissive();
};
} // namespace huira

#include "huira_impl/handles/material_handle.ipp"
