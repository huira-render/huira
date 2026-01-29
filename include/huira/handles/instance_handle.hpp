#pragma once

#include "huira/scene/instance.hpp"
#include "huira/handles/handle.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class FrameHandle;

    template <IsSpectral TSpectral>
    class SceneView;


    template <IsSpectral TSpectral>
    class InstanceHandle : public NodeHandle<TSpectral, Instance<TSpectral>> {
    public:
        InstanceHandle() = delete;
        using NodeHandle<TSpectral, Instance<TSpectral>>::NodeHandle;

        friend class FrameHandle<TSpectral>;
        friend class SceneView<TSpectral>;
    };
}
