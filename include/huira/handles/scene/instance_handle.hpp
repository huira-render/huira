#pragma once

#include "huira/scene/instance.hpp"
#include "huira/handles/handle.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class FrameHandle;

    template <IsSpectral TSpectral>
    class SceneView;


    /**
     * @brief Handle for referencing an Instance node in the scene graph.
     *
     * InstanceHandle provides safe, type-checked access to Instance nodes, allowing
     * manipulation and querying of asset instances within the scene. Used by FrameHandle
     * and SceneView for instance management.
     *
     * @tparam TSpectral Spectral type for the scene
     */
    template <IsSpectral TSpectral>
    class InstanceHandle : public NodeHandle<TSpectral, Instance<TSpectral>> {
    public:
        InstanceHandle() = delete;
        using NodeHandle<TSpectral, Instance<TSpectral>>::NodeHandle;

        friend class FrameHandle<TSpectral>;
        friend class SceneView<TSpectral>;
    };
}
