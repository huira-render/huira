#pragma once

#include <cstddef>

#include "huira/assets/primitive.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"

namespace huira {
template <IsSpectral TSpectral>
class PrimitiveHandle : public Handle<Primitive<TSpectral>> {
  public:
    PrimitiveHandle() = delete;
    using Handle<Primitive<TSpectral>>::Handle;
};
} // namespace huira
