#pragma once

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/volumes/medium.hpp"

namespace huira {
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
