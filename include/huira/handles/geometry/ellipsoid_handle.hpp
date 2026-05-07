#pragma once

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/geometry/ellipsoid.hpp"
#include "huira/handles/geometry/geometry_handle.hpp"

namespace huira {
template <IsSpectral TSpectral>
class EllipsoidHandle : public GeometryHandle<TSpectral> {
  public:
    EllipsoidHandle() = delete;
    using GeometryHandle<TSpectral>::GeometryHandle;
};
} // namespace huira
