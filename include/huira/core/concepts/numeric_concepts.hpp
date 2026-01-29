#pragma once

#include <concepts>
#include <type_traits>

#include "glm/glm.hpp"

namespace huira {
    /// @concept IsFloatingPoint
    template<typename T>
    concept IsFloatingPoint =
        std::same_as<T, float> ||
        std::same_as<T, double>;

    /// @concept IsUnsignedInteger
    template <typename T>
    concept IsUnsignedInteger =
        std::same_as<T, std::uint8_t> ||
        std::same_as<T, std::uint16_t> ||
        std::same_as<T, std::uint32_t> ||
        std::same_as<T, std::uint64_t>;

    // @concept IsSignedInteger
    template <typename T>
    concept IsSignedInteger =
        std::same_as<T, std::int8_t> ||
        std::same_as<T, std::int16_t> ||
        std::same_as<T, std::int32_t> ||
        std::same_as<T, std::int64_t>;

    /// @concept IsInteger
    template <typename T>
    concept IsInteger =
        IsUnsignedInteger<T> ||
        IsSignedInteger<T>;

    /// @concept IsNumeric
    template<typename T>
    concept IsNumeric =
        IsFloatingPoint<T> ||
        IsInteger<T>;


    // Detect glm::vec
    template<typename T>
    struct is_glm_vec_impl : std::false_type {};

    template<int N, typename T, glm::qualifier Q>
    struct is_glm_vec_impl<glm::vec<N, T, Q>> : std::true_type {};

    /// @concept IsVec
    template<typename T>
    concept IsVec = is_glm_vec_impl<T>::value;


    // Detect glm::mat
    template<typename T>
    struct is_glm_mat_impl : std::false_type {};

    template<int C, int R, typename T, glm::qualifier Q>
    struct is_glm_mat_impl<glm::mat<C, R, T, Q>> : std::true_type {};

    /// @concept IsMat
    template<typename T>
    concept IsMat = is_glm_mat_impl<T>::value;
}
