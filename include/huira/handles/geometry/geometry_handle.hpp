#pragma once

#include <cstddef>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/geometry/geometry.hpp"
#include "huira/handles/handle.hpp"

namespace huira {
template <IsSpectral TSpectral>
class GeometryHandle : public Handle<Geometry<TSpectral>> {
  public:
    GeometryHandle() = delete;
    using Handle<Geometry<TSpectral>>::Handle;
};
} // namespace huira
