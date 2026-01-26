#pragma once

#include <cmath>

namespace huira {
    inline float srgb_to_linear(float s)
    {
        if (s <= 0.04045f) {
            return s / 12.92f;
        }
        return std::pow((s + 0.055f) / 1.055f, 2.4f);
    }

    // Simple gamma to linear
    inline float gamma_to_linear(float encoded, float decoding_gamma)
    {
        return std::pow(encoded, decoding_gamma);
    }
}
