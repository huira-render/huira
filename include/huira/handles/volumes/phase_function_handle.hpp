#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/volumes/scattering/phase_function.hpp"

namespace huira {
    /**
     * @brief Handle for manipulating a PhaseFunction in a scene.
     */
    template <IsSpectral TSpectral>
    class PhaseFunctionHandle : public Handle<PhaseFunction<TSpectral>> {
    public:
        PhaseFunctionHandle() = delete;
        using Handle<PhaseFunction<TSpectral>>::Handle;

    };

}
