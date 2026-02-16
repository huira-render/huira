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

    /**
     * @brief Handle for referencing a Light asset in the scene.
     *
     * LightHandle provides safe, type-checked access to Light assets, allowing
     * manipulation and querying of lights within the scene. Used by Scene and FrameHandle.
     *
     * @tparam TSpectral Spectral type for the scene
     */
    template <IsSpectral TSpectral>
    class LightHandle : public Handle<Light<TSpectral>> {
    public:
        LightHandle() = delete;
        using Handle<Light<TSpectral>>::Handle;

        friend class Scene<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };
}
