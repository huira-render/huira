#pragma once

#include <cstdint>
#include <memory>

#include "huira/concepts/spectral_concepts.hpp"
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

    std::shared_ptr<Image<TPixel>> shared_image() const { return this->get_()->shared_image(); }

    std::uint64_t id() const { return this->get_()->id(); }

    Resolution resolution() const noexcept { return this->get_()->resolution(); }
};
} // namespace huira
