#pragma once

#include <concepts>
#include <ratio>
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


    // =============================== //
    // === Concepts For std::ratio === //
    // =============================== //
    template<typename T>
    struct is_std_ratio_impl : std::false_type {};

    template<std::intmax_t Num, std::intmax_t Den>
    struct is_std_ratio_impl<std::ratio<Num, Den>> : std::true_type {};

    /// @concept IsRatio
    template<typename T>
    concept IsRatio = is_std_ratio_impl<T>::value;

    template<typename T>
    struct is_unit_tag : std::false_type {};

    template<typename T>
    concept IsUnitTag = is_unit_tag<T>::value;

    template<typename T>
    concept IsRatioOrTag = IsRatio<T> || IsUnitTag<T>;
}
