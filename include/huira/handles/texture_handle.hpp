#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/images/image.hpp"

namespace huira {
    class TextureHandle {
    private:
        TextureHandle() = default;

        std::size_t id_;
        friend class Scene;
    };
}
