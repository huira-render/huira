#pragma once

#include "huira/assets/lights/light.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"

namespace huira {
    // Forward Declarations
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class FrameHandle;

    template <IsSpectral TSpectral>
    class LightHandle : public Handle<Light<TSpectral>> {
    public:
        LightHandle() = delete;
        using Handle<Light<TSpectral>>::Handle;

        friend class Scene<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };
}
