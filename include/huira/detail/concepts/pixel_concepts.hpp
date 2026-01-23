#pragma once

#include <cstdint>
#include <type_traits>
#include <concepts>

#include "huira/core/spectral_bins.hpp"

namespace huira {
    // Type traits for detecting SpectralBins specializations
    template<typename T>
    struct is_spectral_bins : std::false_type {};

    template<size_t N, auto... Args>
    struct is_spectral_bins<SpectralBins<N, Args...>> : std::true_type {};

    template<typename T>
    inline constexpr bool is_spectral_bins_v = is_spectral_bins<T>::value;

    // Type traits for detecting Vec3 specializations
    template<typename T>
    struct is_vec3 : std::false_type {};

    template<IsFloatingPoint T>
    struct is_vec3<Vec3<T>> : std::true_type {};

    template<typename T>
    inline constexpr bool is_vec3_v = is_vec3<T>::value;

    // Valid pixel types for Image<T>
    template<typename T>
    concept IsImagePixel =
        std::same_as<T, std::int8_t> ||
        std::same_as<T, std::int16_t> ||
        std::same_as<T, std::int32_t> ||
        std::same_as<T, std::int64_t> ||
        std::same_as<T, std::uint8_t> ||
        std::same_as<T, std::uint16_t> ||
        std::same_as<T, std::uint32_t> ||
        std::same_as<T, std::uint64_t> ||
        std::same_as<T, std::size_t> ||
        std::same_as<T, float> ||
        std::same_as<T, double> ||
        is_vec3_v<T> ||
        is_spectral_bins_v<T>;
}
