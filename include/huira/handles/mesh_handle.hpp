#pragma once

#include <cstddef>

#include "huira/handles/handle.hpp"
#include "huira/assets/mesh.hpp"
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
    class MeshHandle : public Handle<Mesh<TSpectral>> {
    public:
        MeshHandle() = delete;
        using Handle<Mesh<TSpectral>>::Handle;

        std::size_t get_vertex_count() const {
            return this->get_()->vertex_buffer().size();
        }

        std::shared_ptr<Mesh<TSpectral>> get_shared() const {
            return this->get_();
        }

        friend class Scene<TSpectral>;
        friend class FrameHandle<TSpectral>;
        friend class ModelLoader<TSpectral>;
    };
}
