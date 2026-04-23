#pragma once

#include <cstddef>

#include "huira/handles/handle.hpp"
#include "huira/assets/primitive.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward Declarations
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class FrameHandle;

    template <IsSpectral TSpectral>
    class ModelLoader;

    template <IsSpectral TSpectral>
    class PrimitiveHandle : public Handle<Primitive<TSpectral>> {
    public:
        PrimitiveHandle() = delete;
        using Handle<Primitive<TSpectral>>::Handle;

        std::shared_ptr<Primitive<TSpectral>> get_shared() const {
            return this->get_();
        }

        friend class Scene<TSpectral>;
        friend class FrameHandle<TSpectral>;
        friend class ModelLoader<TSpectral>;
    };
}
