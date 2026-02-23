#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/materials/texture.hpp"

namespace huira {
    /**
     * @brief Handle for manipulating a Material in a scene.
     *
     * Provides a safe, reference-like interface for configuring and querying a Material
     * instance.  All operations are forwarded to the underlying Material.
     *
     * @tparam TSpectral The spectral type
     */
    template <typename TPixel>
    class TextureHandle : public Handle<Texture<TPixel>> {
    public:
        TextureHandle() = delete;
        using Handle<Texture<TPixel>>::Handle;

        Image<TPixel>* image() noexcept { return this->get_()->image(); }
        const Image<TPixel>* image() const noexcept { return this->get_()->image(); }

        Resolution resolution() const noexcept { return this->get_()->resolution(); }
    };
}
