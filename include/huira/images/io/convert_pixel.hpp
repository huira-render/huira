#pragma once

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/pixel_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsNonSpectralPixel T>
    T convert_float_to_pixel(float value);

    template <IsNonSpectralPixel T>
    T convert_vec3_to_pixel(Vec3<float> value);

    template <IsNonSpectralPixel T>
    float convert_pixel_to_float(T value);

    template <IsNonSpectralPixel T>
    Vec3<float> convert_pixel_to_vec3(T value);
    


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
