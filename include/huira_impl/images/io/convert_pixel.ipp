#include <limits>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/detail/logger.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsImagePixel T>
    T convert_pixel(const float& value)
    {
        if constexpr (IsFloatingPoint<T>) {
            return static_cast<T>(value);
        }
        else if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(value * static_cast<float>(std::numeric_limits<T>::max()));
        }
        else if constexpr (is_vec3<T>::value) {
            if constexpr (std::is_same_v<T, Vec3<float>>) {
                return Vec3<float>(value, value, value);
            }
            else if constexpr (std::is_same_v<T, Vec3<double>>) {
                return Vec3<double>(static_cast<double>(value), static_cast<double>(value), static_cast<double>(value));
            }
        }
        else if constexpr (is_spectral_bins<T>::value) {
            T result;
            for (std::size_t i = 0; i < T::size(); ++i) {
                result[i] = value;
            }
            return result;
        }
        else {
            static_assert(std::is_arithmetic_v<T>, "convert_pixel: Unsupported pixel type for single float conversion.");
        }
    }

    template <IsImagePixel T>
    T convert_pixel(const Vec3<float>& value)
    {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
            float gray = 0.2126f * value.x + 0.7152f * value.y + 0.0722f * value.z;
            if constexpr (std::is_same_v<T, float>) {
                return gray;
            }
            else {
                return static_cast<double>(gray);
            }
        }
        else if constexpr (std::is_integral_v<T>) {
            float gray = 0.2126f * value.x + 0.7152f * value.y + 0.0722f * value.z;
            return static_cast<T>(gray * static_cast<float>(std::numeric_limits<T>::max()));
        }
        else if constexpr (is_vec3<T>::value) {
            if constexpr (std::is_same_v<T, Vec3<float>>) {
                return value;
            }
            else if constexpr (std::is_same_v<T, Vec3<double>>) {
                return Vec3<double>(static_cast<double>(value.x), static_cast<double>(value.y), static_cast<double>(value.z));
            }
        }
        else if constexpr (is_spectral_bins<T>::value) {
            // TODO Implement
            HUIRA_THROW_ERROR("NOT YET IMPLEMENTED: convert_pixel from Vec3<float> to SpectralBins.");
        }
        else {
            static_assert(std::is_arithmetic_v<T>, "convert_pixel: Unsupported pixel type for Vec3<float> conversion.");
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
