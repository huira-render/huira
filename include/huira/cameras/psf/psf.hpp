#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class PSF {
        public:
        PSF() = default;
        virtual ~PSF() = default;

        virtual TSpectral evaluate(float x, float y) = 0;
    };
}
