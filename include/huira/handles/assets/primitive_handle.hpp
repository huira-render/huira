#pragma once

#include <cstddef>

#include "huira/handles/handle.hpp"
#include "huira/assets/primitive.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class PrimitiveHandle : public Handle<Primitive<TSpectral>> {
    public:
        PrimitiveHandle() = delete;
        using Handle<Primitive<TSpectral>>::Handle;
    };
}
