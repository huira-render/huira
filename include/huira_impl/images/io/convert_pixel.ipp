#include <limits>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/detail/logger.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsNonSpectralPixel T>
    T convert_float_to_pixel(float value)
    {
        // This function is used to convert a linear float value in [0.0, 1.0] to the target pixel type T
        if constexpr (IsFloatingPoint<T>) {
            return static_cast<T>(value);
        }
        else if constexpr (IsInteger<T>) {
            return float_to_integer<T>(value, 0.f, 1.f);
        }
        else if constexpr (std::is_same_v<T, Vec3<float>>) {
            return Vec3<float>(value, value, value);
        }
        else if constexpr (std::is_same_v<T, Vec3<double>>) {
            return Vec3<double>(static_cast<double>(value), static_cast<double>(value), static_cast<double>(value));
        }
    }

    template <IsNonSpectralPixel T>
    T convert_vec3_to_pixel(Vec3<float> value)
    {
        // This function is used to convert a linear RGB Vec3<float> value to the target pixel type T
        float gray = 0.2126f * value.x + 0.7152f * value.y + 0.0722f * value.z;
        if constexpr (IsFloatingPoint<T>) {
            return static_cast<T>(gray);
        }
        else if constexpr (IsInteger<T>) {
            return float_to_integer<T>(gray, 0.f, 1.f);
        }
        else if constexpr (std::is_same_v<T, Vec3<float>>) {
            return value;
        }
        else if constexpr (std::is_same_v<T, Vec3<double>>) {
            return Vec3<double>(static_cast<double>(value.x), static_cast<double>(value.y), static_cast<double>(value.z));
        }
    }

    template <IsNonSpectralPixel T>
    float convert_pixel_to_float(T value)
    {
        // This function is used to convert a pixel of type T to a linear float value in [0.0, 1.0]
        if constexpr (IsFloatingPoint<T>) {
            return static_cast<float>(value);
        }
        else if constexpr (IsInteger<T>) {
            return integer_to_float<T>(value, 0.f, 1.f);
        }
        else if constexpr (std::is_same_v<T, Vec3<float>>) {
            float gray = 0.2126f * value.x + 0.7152f * value.y + 0.0722f * value.z;
            return gray;
        }
        else if constexpr (std::is_same_v<T, Vec3<double>>) {
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
                static_cast<float>(value),
                static_cast<float>(value),
                static_cast<float>(value)
            );
        }
        else if constexpr (IsInteger<T>) {
            return Vec3<float>(
                integer_to_float<T>(value, 0.f, 1.f),
                integer_to_float<T>(value, 0.f, 1.f),
                integer_to_float<T>(value, 0.f, 1.f)
            );
        }
        else if constexpr (std::is_same_v<T, Vec3<float>>) {
            return value;
        }
        else if constexpr (std::is_same_v<T, Vec3<double>>) {
            return Vec3<float>(
                static_cast<float>(value.x),
                static_cast<float>(value.y),
                static_cast<float>(value.z)
            );
        }
    }


    template <IsUnsignedInteger T>
    float integer_to_float(T value, float min_range, float max_range)
    {
        float normalized = static_cast<float>(value) / static_cast<float>(std::numeric_limits<T>::max());
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



    template <IsUnsignedInteger T>
    T float_to_integer(float value, float min_range, float max_range)
    {
        float normalized = (value - min_range) / (max_range - min_range);
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        return static_cast<T>(normalized * static_cast<float>(std::numeric_limits<T>::max()) + 0.5f);
    }

    template <IsSignedInteger T>
    T float_to_integer(float value, float min_range, float max_range)
    {
        float type_min = static_cast<float>(std::numeric_limits<T>::min());
        float type_max = static_cast<float>(std::numeric_limits<T>::max());
        
        float normalized = (value - min_range) / (max_range - min_range);
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        
        return static_cast<T>(type_min + normalized * (type_max - type_min) + 0.5f);
    }
}
