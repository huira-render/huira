#pragma once

#include <type_traits>

namespace huira {
    // Forward declaration
    template <size_t N, auto... Args>
    class SpectralBins;

    // Detect SpectralBins specialization
    template<typename T>
    struct is_spectral_bins_impl : std::false_type {};

    template<size_t N, auto... Args>
    struct is_spectral_bins_impl<SpectralBins<N, Args...>> : std::true_type {};

    /// @concept IsSpectral
    /// @brief Checks if a type is a specialization of SpectralBins
    template<typename T>
    concept IsSpectral = is_spectral_bins_impl<T>::value;
}
