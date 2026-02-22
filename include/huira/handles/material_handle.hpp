#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/materials/material.hpp"

namespace huira {
    /**
     * @brief Handle for manipulating a Material in a scene.
     *
     * Provides a safe, reference-like interface for configuring and querying a Material
     * instance.  All operations are forwarded to the underlying Material.
     *
     * @tparam TSpectral The spectral type
     */
    template <IsSpectral TSpectral>
    class MaterialHandle : public Handle<Material<TSpectral>> {
    public:
        MaterialHandle() = delete;
        using Handle<Material<TSpectral>>::Handle;


    };
}
