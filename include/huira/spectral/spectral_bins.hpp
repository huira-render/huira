#pragma once

#include <iostream>
#include <string>

#include "huira/spectral/numeric_array.hpp"

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

    template <size_t N, auto... Args>
    class SpectralBins : public NumericArray<float, N> {
    public:
        constexpr SpectralBins() : NumericArray<float, N>() {
            initialize_bins_static();
        }

        // Single value:
        explicit constexpr SpectralBins(float value) : NumericArray<float, N>(value) {
            initialize_bins_static();
        }

        // Initializer list:
        constexpr SpectralBins(std::initializer_list<float> init) : NumericArray<float, N>(init) {
            initialize_bins_static();
        }

        // Direct element initialization
        template <typename... Values>
            requires (sizeof...(Values) == N && (std::convertible_to<Values, float> && ...))
        constexpr SpectralBins(Values&&... values) : NumericArray<float, N>(static_cast<float>(values)...) {
            initialize_bins_static();
        }

        // Access to bin information
        constexpr const Bin& get_bin(size_t index) const { return bins_[index]; }
        constexpr const std::array<Bin, N>& get_all_bins() const { return bins_; }

    private:
        static std::array<Bin, N> bins_;

        static constexpr std::array<Bin, N> initialize_bins_static();
        static constexpr std::array<Bin, N> initialize_uniform_static();
        static constexpr std::array<Bin, N> initialize_pairs_static();
        static constexpr std::array<Bin, N> initialize_edges_static();
    };

    // Aliases for faster setting:
    template <size_t N, int min, int max>
    using UniformSpectralBins = SpectralBins<N, min, max>;

    template <size_t N, auto... Args>
    using SpectralBinEdges = SpectralBins<N, Args...>;

    // Helpful types:
    using RGB = SpectralBins<3, 600, 750, 500, 600, 380, 500>;
}

#include "huira_impl/spectral/spectral_bins.ipp"
