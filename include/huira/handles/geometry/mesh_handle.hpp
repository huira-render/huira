#pragma once

#include <cstddef>

#include "huira/handles/geometry/geometry_handle.hpp"
#include "huira/geometry/mesh.hpp"
#include "huira/concepts/spectral_concepts.hpp"

namespace huira {
    /**
     * @brief Handle for referencing a Mesh asset in the scene.
     *
     * MeshHandle provides safe, type-checked access to Mesh assets, allowing
     * manipulation and querying of mesh data within the scene. Used by Scene,
     * FrameHandle, and ModelLoader for mesh management.
     *
     * @tparam TSpectral Spectral type for the scene
     */
    template <IsSpectral TSpectral>
    class MeshHandle : public GeometryHandle<TSpectral> {
    public:
        using GeometryHandle<TSpectral>::GeometryHandle;

        MeshHandle() = delete;

        std::size_t get_vertex_count() const {
            return this->get_mesh_()->vertex_buffer().size();
        }

        std::shared_ptr<Mesh<TSpectral>> get_mesh_shared() const {
            return this->get_mesh_();
        }

    private:
        std::shared_ptr<Mesh<TSpectral>> get_mesh_() const {
            auto ptr = this->template get<Mesh<TSpectral>>();
            if (ptr) {
                return ptr;
            }
            else {
                HUIRA_THROW_ERROR("MeshHandle::get_mesh_ - Invalid handle or does not contain a Mesh");
            }
        }
    };
}
