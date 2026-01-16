#pragma once

#include <concepts>
#include <type_traits>

#include "glm/glm.hpp"

namespace huira {
    /// @concept IsFloatingPoint
    template<typename T>
    concept IsFloatingPoint = std::same_as<T, float> || std::same_as<T, double>;

    /// @concept IsInteger
    template <typename T>
    concept IsInteger = std::is_integral_v<T>;

    /// @concept IsNumeric
    template<typename T>
    concept IsNumeric = std::is_arithmetic_v<T>;


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
