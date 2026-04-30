#pragma once

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/materials/bsdfs/bsdf.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class BSDFHandle : public Handle<BSDF<TSpectral>> {
    public:
        BSDFHandle() = delete;
        using Handle<BSDF<TSpectral>>::Handle;

        
    };
}
