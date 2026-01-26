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



    inline float linear_to_srgb(float linear)
    {
        if (linear <= 0.0031308f) {
            return linear * 12.92f;
        }
        return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
    }


}
