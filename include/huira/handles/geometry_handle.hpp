#pragma once

#include <cstddef>

#include "huira/handles/handle.hpp"
#include "huira/geometry/geometry.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward Declarations
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class GeometryHandle : public Handle<Geometry<TSpectral>> {
    public:
        GeometryHandle() = delete;
        using Handle<Geometry<TSpectral>>::Handle;


    private:
        std::shared_ptr<Geometry<TSpectral>> get_shared() const { return this->get_(); }

        friend class Scene<TSpectral>;
    };
}
