#pragma once

#include "huira/concepts/spectral_concepts.hpp"

namespace huira {
template <IsSpectral TSpectral>
struct MediumProperties {
    TSpectral absorption = TSpectral{0};
    TSpectral scattering = TSpectral{0};

    [[nodiscard]] TSpectral extinction() const { return absorption + scattering; }
};
} // namespace huira
