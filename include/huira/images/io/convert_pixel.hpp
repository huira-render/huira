#pragma once

#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsImagePixel T>
    T convert_pixel(const float& value);

    template <IsImagePixel T>
    T convert_pixel(const Vec3<float>&value);
}

#include "huira_impl/images/io/convert_pixel.ipp"
