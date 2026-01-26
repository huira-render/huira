#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsImagePixel T>
    T convert_pixel(const float& value);

    template <IsImagePixel T>
    T convert_pixel(const Vec3<float>&value);

    

    template <IsUnsignedInteger T>
    float integer_to_float(T value, float min_range = 0.f, float max_range = 1.f);

    template <IsSignedInteger T>
    float integer_to_float(T value, float min_range = 0.f, float max_range = 1.f);




    template <IsUnsignedInteger T>
    T float_to_integer(float value, float min_range = 0.f, float max_range = 1.f);

    template <IsSignedInteger T>
    T float_to_integer(float value, float min_range = 0.f, float max_range = 1.f);
}

#include "huira_impl/images/io/convert_pixel.ipp"
