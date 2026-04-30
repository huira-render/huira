#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/volumes/medium_properties.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class DensityField {
    public:
        DensityField() = default;
        virtual ~DensityField() = default;

        [[nodiscard]] virtual MediumProperties<TSpectral> evaluate(const Vec3<float>& p) const = 0;
    };
}
