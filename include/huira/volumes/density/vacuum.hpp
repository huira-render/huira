#pragma once

#include <string>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/volumes/medium_properties.hpp"

namespace huira {

    template <IsSpectral TSpectral>
    class VacuumDensityField : public DensityField<TSpectral> {
    public:
        VacuumDensityField() = default;
        ~VacuumDensityField() override = default;
        
        [[nodiscard]] MediumProperties<TSpectral> evaluate(const Vec3<float>& p) const override {
            (void)p;
            return { TSpectral{0}, TSpectral{0}, TSpectral{0} };
        }

        std::string type() const override { return "VacuumDensityField"; }
    };

}
