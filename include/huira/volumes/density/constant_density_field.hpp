#pragma once

#include <string>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/util/logger.hpp"
#include "huira/volumes/density/density_field.hpp"
#include "huira/volumes/medium_properties.hpp"

namespace huira {
template <IsSpectral TSpectral>
class ConstantDensityField : public DensityField<TSpectral> {
  public:
    ConstantDensityField() = default;

    explicit ConstantDensityField(TSpectral absorption, TSpectral scattering)
    {
        // valid() is false for NaN, which a test of val < 0 lets through:
        if (!absorption.valid()) {
            HUIRA_THROW_ERROR("Absorption coefficients must be finite and non-negative: " +
                              absorption.to_string());
        }
        if (!scattering.valid()) {
            HUIRA_THROW_ERROR("Scattering coefficients must be finite and non-negative: " +
                              scattering.to_string());
        }
        properties_.absorption = absorption;
        properties_.scattering = scattering;
    }

    ~ConstantDensityField() override = default;

    [[nodiscard]] MediumProperties<TSpectral> evaluate(const Vec3<float>& p) const override
    {
        (void)p;
        return properties_;
    }

    std::string type() const override { return "ConstantDensityField"; }

  private:
    MediumProperties<TSpectral> properties_{};
};

} // namespace huira
