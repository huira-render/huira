#pragma once

#include <cstddef>

#include "huira/handles/handle.hpp"
#include "huira/assets/atmosphere.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class AtmosphereHandle : public Handle<Atmosphere<TSpectral>> {
    public:
        AtmosphereHandle() = delete;
        using Handle<Atmosphere<TSpectral>>::Handle;

        std::shared_ptr<Atmosphere<TSpectral>> get_shared() const {
            return this->get_();
        }
    };
}
