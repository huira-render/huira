#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    struct MediumProperties {
        TSpectral sigma_a; // Absorption coefficient
        TSpectral sigma_s; // Scattering coefficient
        TSpectral sigma_t; // Attenuation (Extinction) coefficient = sigma_a + sigma_s
    };
}
