#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/volumes/medium.hpp"

namespace huira {
    // Forward Declarations
    template <IsSpectral TSpectral>
    class Scene;

    /**
     * @brief Handle for manipulating a Medium in a scene.
     */
    template <IsSpectral TSpectral>
    class MediumHandle : public Handle<Medium<TSpectral>> {
    public:
        MediumHandle() = delete;
        using Handle<Medium<TSpectral>>::Handle;

    };

}
