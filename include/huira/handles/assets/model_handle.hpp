#pragma once

#include "huira/assets/model.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/handles/materials/material_handle.hpp"
#include "huira/handles/materials/bsdf_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class FrameHandle;

    /**
     * @brief Handle for referencing a Model asset in the scene.
     *
     * ModelHandle provides safe, type-checked access to Model assets, allowing
     * manipulation and querying of models within the scene. Used by Scene and FrameHandle.
     *
     * @tparam TSpectral Spectral type for the scene
     */
    template <IsSpectral TSpectral>
    class ModelHandle : public Handle<Model<TSpectral>> {
    public:
        ModelHandle() = delete;
        using Handle<Model<TSpectral>>::Handle;

        void print_graph() const {
            this->get_()->print_graph();
        }

        MaterialHandle<TSpectral> get_material_by_id(std::uint64_t material_id) const {
            return this->get_()->get_material_by_id(material_id);
        }

        void set_all_bsdfs(const BSDFHandle<TSpectral>& bsdf_handle) const {
            return this->get_()->set_all_bsdfs(bsdf_handle);
        }

    private:
        friend class Scene<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };

}
