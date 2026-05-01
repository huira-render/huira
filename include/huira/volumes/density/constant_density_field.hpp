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
        for (float val : absorption) {
            if (val < 0.f) {
                HUIRA_THROW_ERROR("Absorption coefficients must be non-negative.");
            }
        }
        for (float val : scattering) {
            if (val < 0.f) {
                HUIRA_THROW_ERROR("Scattering coefficients must be non-negative.");
            }
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
