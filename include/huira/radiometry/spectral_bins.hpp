#pragma once

#include "huira/math/numeric_array.hpp"

namespace huira {

    template <size_t N>
    class SpectralBins : public NumericArray<float, N> {
    public:
        // Import base class constructors that already work with float
        using NumericArray<float, N>::NumericArray;

        // Universal variadic constructor - accepts any numeric types, converts to float
        template<typename... Args>
            requires (sizeof...(Args) == N && (IsNumeric<Args> && ...))
        constexpr SpectralBins(Args&&... args)
            : NumericArray<float, N>(static_cast<float>(args)...) {}

        // Universal initializer list constructor - accepts any numeric type
        template<IsNumeric U>
        constexpr SpectralBins(std::initializer_list<U> init)
            : NumericArray<float, N>() {
            size_t i = 0;
            for (const auto& value : init) {
                if (i >= N) break;
                (*this)[i] = static_cast<float>(value);
                ++i;
            }
        }

        // Universal fill constructor - accepts any numeric type
        template<IsNumeric U>
        explicit constexpr SpectralBins(const U& value)
            : NumericArray<float, N>(static_cast<float>(value)) {}

        // Copy constructor from any NumericArray with numeric type
        template<IsNumeric U>
        constexpr SpectralBins(const NumericArray<U, N>& other)
            : NumericArray<float, N>() {
            for (size_t i = 0; i < N; ++i) {
                (*this)[i] = static_cast<float>(other[i]);
            }
        }

    private:
    };

    // Deduction guide for SpectralBins
    template<typename T, typename... U>
        requires (IsNumeric<T> && (IsNumeric<U> && ...))
    SpectralBins(T, U...)->SpectralBins<1 + sizeof...(U)>;
}

#include "inline/radiometry/spectral_bins.ipp"