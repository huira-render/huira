#pragma once

#include <cstddef>

#include "huira/handles/handle.hpp"
#include "huira/geometry/geometry.hpp"
#include "huira/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class GeometryHandle : public Handle<Geometry<TSpectral>> {
    public:
        GeometryHandle() = delete;
        using Handle<Geometry<TSpectral>>::Handle;
    };
}
