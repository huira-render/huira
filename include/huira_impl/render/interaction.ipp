#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "huira/core/types.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"

namespace huira {
    /**
     * @brief Threshold below which direct floating-point offset is used for intersection offsetting.
     * @tparam T Floating-point type
     * @return T Threshold value
     */
    template <IsFloatingPoint T>
    constexpr T origin() {
        if constexpr (std::is_same_v<T, float>) {
            return 1.0f / 32.0f;
        }
        else {
            return 1.0 / 64.0;
        }
    }

    /**
     * @brief Scale factor for direct floating-point offset.
     * @tparam T Floating-point type
     * @return T Scale factor
     */
    template <IsFloatingPoint T>
    constexpr T float_scale() {
        if constexpr (std::is_same_v<T, float>) {
            return 1.0f / 65536.0f;
        }
        else {
            return 1.0 / 4294967296.0;
        }
    }

    /**
     * @brief Scale factor for integer-space offset.
     * @tparam T Floating-point type
     * @return T Scale factor
     */
    template <IsFloatingPoint T>
    constexpr T int_scale() {
        if constexpr (std::is_same_v<T, float>) {
            return 256.0f;
        }
        else {
            return 65536.0;
        }
    }

    /**
     * @brief Bit-casts a float to int32_t for bit-level manipulation.
     * @param val Float value
     * @return std::int32_t Bitwise representation
     */
    inline std::int32_t float_as_int(float val) {
        std::int32_t result;
        std::memcpy(&result, &val, sizeof(float));
        return result;
    }

    /**
     * @brief Bit-casts a double to int64_t for bit-level manipulation.
     * @param val Double value
     * @return std::int64_t Bitwise representation
     */
    inline std::int64_t float_as_int(double val) {
        std::int64_t result;
        std::memcpy(&result, &val, sizeof(double));
        return result;
    }

    /**
     * @brief Bit-casts an int32_t to float for bit-level manipulation.
     * @param val Integer value
     * @return float Bitwise representation as float
     */
    inline float int_as_float(std::int32_t val) {
        float result;
        std::memcpy(&result, &val, sizeof(std::int32_t));
        return result;
    }

    /**
     * @brief Bit-casts an int64_t to double for bit-level manipulation.
     * @param val Integer value
     * @return double Bitwise representation as double
     */
    inline double int_as_float(std::int64_t val) {
        double result;
        std::memcpy(&result, &val, sizeof(std::int64_t));
        return result;
    }

    /**
     * @brief Offsets an intersection point along a normal to prevent self-intersection artifacts.
     *
     * Uses bit-level manipulation to offset the intersection point in floating-point or integer space,
     * depending on the magnitude, to avoid shadow acne and other precision issues in ray tracing.
     * Adapted from Section 6.2.2.4 of "Physically Based Rendering" (Springer).
     *
     * @tparam T Floating-point type
     * @param intersection The intersection point
     * @param N The geometric normal at the intersection
     * @return Vec3<T> Offset intersection point
     */
    template <IsFloatingPoint T>
    inline Vec3<T> offset_intersection_(Vec3<T> intersection, const Vec3<T>& N) {
        using IntType = std::conditional_t<std::is_same_v<T, float>, int32_t, int64_t>;

        const Vec3<IntType> offset_int{
            static_cast<IntType>(int_scale<T>() * N.x),
            static_cast<IntType>(int_scale<T>() * N.y),
            static_cast<IntType>(int_scale<T>() * N.z)
        };

        const Vec3<T> offset_result{
            int_as_float(float_as_int(intersection.x) + (intersection.x < 0 ? -offset_int.x : offset_int.x)),
            int_as_float(float_as_int(intersection.y) + (intersection.y < 0 ? -offset_int.y : offset_int.y)),
            int_as_float(float_as_int(intersection.z) + (intersection.z < 0 ? -offset_int.z : offset_int.z))
        };

        const T threshold = origin<T>();
        const T scale = float_scale<T>();

        return Vec3<T>{
            std::abs(intersection.x) < threshold ? intersection.x + scale * N.x : offset_result.x,
            std::abs(intersection.y) < threshold ? intersection.y + scale * N.y : offset_result.y,
            std::abs(intersection.z) < threshold ? intersection.z + scale * N.z : offset_result.z
        };
    }
}
