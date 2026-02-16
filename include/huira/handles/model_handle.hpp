#pragma once

#include "huira/assets/model.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"

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

    private:
        friend class Scene<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };

}
