#pragma once

#include <iostream>
#include <string>

#include "huira/detail/numeric_array.hpp"

namespace huira {
    // Bin definitions:
    struct Bin {
        float min;
        float max;
        float center;

        constexpr Bin() : min(0), max(0), center(0) {}

        constexpr Bin(float new_min, float new_max) :
            min{ new_min }, max{ new_max }, center{ (min + max) / 2.0f }
        {
        }
    };

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4686)  // Possible change in behavior in UDT return calling convention
#endif
    template <size_t N, auto... Args>
    class SpectralBins : public NumericArray<float, N> {
    public:
        constexpr SpectralBins() : NumericArray<float, N>() {
            initialize_bins_static_();
        }

        // Single value:
        explicit constexpr SpectralBins(float value) : NumericArray<float, N>(value) {
            initialize_bins_static_();
        }

        // Initializer list:
        constexpr SpectralBins(std::initializer_list<float> init) : NumericArray<float, N>(init) {
            initialize_bins_static_();
        }

        // Direct element initialization
        template <typename... Values>
            requires (sizeof...(Values) == N && (std::convertible_to<Values, float> && ...))
        constexpr SpectralBins(Values&&... values) : NumericArray<float, N>(static_cast<float>(values)...) {
            initialize_bins_static_();
        }

        // Assignment from NumericArray (for arithmetic operation results)
        constexpr SpectralBins& operator=(const NumericArray<float, N>& other) {
            NumericArray<float, N>::operator=(other);
            return *this;
        }

        // Access to bin information
        constexpr const Bin& get_bin(size_t index) const { return bins_[index]; }
        constexpr const std::array<Bin, N>& get_all_bins() const { return bins_; }

    private:
        static std::array<Bin, N> bins_;

        static constexpr std::array<Bin, N> initialize_bins_static_();
        static constexpr std::array<Bin, N> initialize_uniform_static_();
        static constexpr std::array<Bin, N> initialize_pairs_static_();
        static constexpr std::array<Bin, N> initialize_edges_static_();
    };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    // Aliases for faster setting:
    template <size_t N, int min, int max>
    using UniformSpectralBins = SpectralBins<N, min, max>;

    template <size_t N, auto... Args>
    using SpectralBinEdges = SpectralBins<N, Args...>;

    // Helpful types:
    using RGB = SpectralBins<3, 600, 750, 500, 600, 380, 500>;
}

#include "huira_impl/core/spectral_bins.ipp"
