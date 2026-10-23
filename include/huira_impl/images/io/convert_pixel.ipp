#include <algorithm>
#include <cmath>
#include <limits>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/pixel_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/util/logger.hpp"

namespace huira {
template <IsNonSpectralPixel T>
T convert_float_to_pixel(float value)
{
    // This function is used to convert a linear float value in [0.0, 1.0] to the target pixel type
    // T
    if constexpr (IsFloatingPoint<T>) {
        return static_cast<T>(value);
    } else if constexpr (IsInteger<T>) {
        return float_to_integer<T>(value, 0.f, 1.f);
    } else if constexpr (std::is_same_v<T, Vec3<float>>) {
        return Vec3<float>(value, value, value);
    } else if constexpr (std::is_same_v<T, Vec3<double>>) {
        return Vec3<double>(
            static_cast<double>(value), static_cast<double>(value), static_cast<double>(value));
    }
}

template <IsNonSpectralPixel T>
T convert_vec3_to_pixel(Vec3<float> value)
{
    // This function is used to convert a linear RGB Vec3<float> value to the target pixel type T
    float gray = 0.2126f * value.x + 0.7152f * value.y + 0.0722f * value.z;
    if constexpr (IsFloatingPoint<T>) {
        return static_cast<T>(gray);
    } else if constexpr (IsInteger<T>) {
        return float_to_integer<T>(gray, 0.f, 1.f);
    } else if constexpr (std::is_same_v<T, Vec3<float>>) {
        return value;
    } else if constexpr (std::is_same_v<T, Vec3<double>>) {
        return Vec3<double>(static_cast<double>(value.x),
                            static_cast<double>(value.y),
                            static_cast<double>(value.z));
    }
}

template <IsNonSpectralPixel T>
float convert_pixel_to_float(T value)
{
    // This function is used to convert a pixel of type T to a linear float value in [0.0, 1.0]
    if constexpr (IsFloatingPoint<T>) {
        return static_cast<float>(value);
    } else if constexpr (IsInteger<T>) {
        return integer_to_float<T>(value, 0.f, 1.f);
    } else if constexpr (std::is_same_v<T, Vec3<float>>) {
        float gray = 0.2126f * value.x + 0.7152f * value.y + 0.0722f * value.z;
        return gray;
    } else if constexpr (std::is_same_v<T, Vec3<double>>) {
        double gray = 0.2126 * value.x + 0.7152 * value.y + 0.0722 * value.z;
        return static_cast<float>(gray);
    }
}

template <IsNonSpectralPixel T>
Vec3<float> convert_pixel_to_vec3(T value)
{
    // This function is used to convert a pixel of type T to a linear RGB Vec3<float>
    if constexpr (IsFloatingPoint<T>) {
        return Vec3<float>(
            static_cast<float>(value), static_cast<float>(value), static_cast<float>(value));
    } else if constexpr (IsInteger<T>) {
        return Vec3<float>(integer_to_float<T>(value, 0.f, 1.f),
                           integer_to_float<T>(value, 0.f, 1.f),
                           integer_to_float<T>(value, 0.f, 1.f));
    } else if constexpr (std::is_same_v<T, Vec3<float>>) {
        return value;
    } else if constexpr (std::is_same_v<T, Vec3<double>>) {
        return Vec3<float>(
            static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z));
    }
}

template <IsUnsignedInteger T>
float integer_to_float(T value, float min_range, float max_range)
{
    float normalized =
        static_cast<float>(value) / static_cast<float>(std::numeric_limits<T>::max());
    return min_range + normalized * (max_range - min_range);
}

template <IsSignedInteger T>
float integer_to_float(T value, float min_range, float max_range)
{
    // Map signed integer from [min, max] to [min_range, max_range]
    float true_min = static_cast<float>(std::numeric_limits<T>::min());
    float true_max = static_cast<float>(std::numeric_limits<T>::max());
    float normalized = (static_cast<float>(value) - true_min) / (true_max - true_min);
    return min_range + normalized * (max_range - min_range);
}

namespace detail {
/**
 * @brief The integer a value in [min_range, max_range] maps to, over T's whole range: T's lowest
 * value plus the value's fraction of the range of T, rounded to the nearest integer.
 *
 * In double, and kept within T's range: the largest values of 32- and 64-bit types round up in
 * a float (2^32 - 1 to 2^32), past what T holds, and converting such a value to T is undefined.
 * A range that is empty (or not finite) has no fraction to give, and gives T's lowest value.
 */
template <IsInteger T>
T scale_to_integer(float value, float min_range, float max_range)
{
    const double fraction = std::clamp((static_cast<double>(value) - min_range) /
                                           (static_cast<double>(max_range) - min_range),
                                       0.0,
                                       1.0);
    if (std::isnan(fraction)) {
        return std::numeric_limits<T>::min();
    }
    const auto lowest = static_cast<double>(std::numeric_limits<T>::min());
    const auto highest = static_cast<double>(std::numeric_limits<T>::max());
    const double scaled = lowest + std::round(fraction * (highest - lowest));
    if (scaled >= highest) {
        return std::numeric_limits<T>::max();
    }
    return static_cast<T>(scaled);
}
} // namespace detail

template <IsUnsignedInteger T>
T float_to_integer(float value, float min_range, float max_range)
{
    if (!std::isfinite(value)) {
        return T{0}; // no integer stands for a value that is not finite
    }
    return detail::scale_to_integer<T>(value, min_range, max_range);
}

template <IsSignedInteger T>
T float_to_integer(float value, float min_range, float max_range)
{
    if (!std::isfinite(value)) {
        return std::numeric_limits<T>::min(); // as min_range would be
    }
    return detail::scale_to_integer<T>(value, min_range, max_range);
}
} // namespace huira
