#pragma once

#include <cstddef>

#include "huira/handles/handle.hpp"
#include "huira/assets/mesh.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward Declarations
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class MeshHandle : public Handle<Mesh<TSpectral>> {
    public:
        MeshHandle() = delete;
        using Handle<Mesh<TSpectral>>::Handle;

        std::size_t get_vertex_count() const {
            return this->get()->get_vertex_buffer().size();
        }

        friend Scene<TSpectral>;
    };
}
