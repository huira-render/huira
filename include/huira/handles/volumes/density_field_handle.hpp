#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/volumes/density/density_field.hpp"

namespace huira {
    /**
     * @brief Handle for manipulating a DensityField in a scene.
     */
    template <IsSpectral TSpectral>
    class DensityFieldHandle : public Handle<DensityField<TSpectral>> {
    public:
        DensityFieldHandle() = delete;
        using Handle<DensityField<TSpectral>>::Handle;

    };

}
