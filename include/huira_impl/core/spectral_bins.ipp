#include <array>
#include <iostream>
#include <string>

#include "huira/core/physics.hpp"

namespace huira {
    // ======================== //
    // === Static Definition === //
    // ======================== //
    template <size_t N, auto... Args>
    std::array<Bin, N> SpectralBins<N, Args...>::bins_ = SpectralBins<N, Args...>::initialize_bins_static_();


    // ========================== //
    // === Summary Operations === //
    // ========================== //
    template <size_t N, auto... Args>
    float SpectralBins<N, Args...>::total() const {
        float sum = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            sum += data_[i];
        }
        return sum;
    }

    template <size_t N, auto... Args>
    float SpectralBins<N, Args...>::magnitude() const {
        float sum_of_squares = 0.0f;
        for (size_t i = 0; i < N; ++i) {
            sum_of_squares += data_[i] * data_[i];
        }
        return std::sqrt(sum_of_squares);
    }

    template <size_t N, auto... Args>
    float SpectralBins<N, Args...>::max() const {
        if constexpr (N == 0) {
            return 0.0f;
        }
        float max_val = data_[0];
        for (size_t i = 1; i < N; ++i) {
            if (data_[i] > max_val) {
                max_val = data_[i];
            }
        }
        return max_val;
    }

    template <size_t N, auto... Args>
    float SpectralBins<N, Args...>::min() const {
        if constexpr (N == 0) {
            return 0.0f;
        }
        float min_val = data_[0];
        for (size_t i = 1; i < N; ++i) {
            if (data_[i] < min_val) {
                min_val = data_[i];
            }
        }
        return min_val;
    }

    // ========================================= //
    // === Array-Array Arithmetic Operations === //
    // ========================================= //
    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator+=(const SpectralBins<N, Args...>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] += other.data_[i];
        }
        return *this;
    }

    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator-=(const SpectralBins<N, Args...>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] -= other.data_[i];
        }
        return *this;
    }

    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator*=(const SpectralBins<N, Args...>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] *= other.data_[i];
        }
        return *this;
    }

    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator/=(const SpectralBins<N, Args...>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] /= other.data_[i];
        }
        return *this;
    }

    // ========================================== //
    // === Array-Scalar Arithmetic Operations === //
    // ========================================== //
    template <size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator+=(const U& scalar) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] += static_cast<float>(scalar);
        }
        return *this;
    }

    template <size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator-=(const U& scalar) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] -= static_cast<float>(scalar);
        }
        return *this;
    }

    template <size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator*=(const U& scalar) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] *= static_cast<float>(scalar);
        }
        return *this;
    }

    template <size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator/=(const U& scalar) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] /= static_cast<float>(scalar);
        }
        return *this;
    }

    // ======================= //
    // === Unary Operators === //
    // ======================= //
    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> SpectralBins<N, Args...>::operator+() const {
        return *this;
    }

    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> SpectralBins<N, Args...>::operator-() const {
        SpectralBins<N, Args...> result;
        for (size_t i = 0; i < N; ++i) {
            result.data_[i] = -data_[i];
        }
        return result;
    }

    // ============================ //
    // === Comparison Operators === //
    // ============================ //
    template <size_t N, auto... Args>
    constexpr bool SpectralBins<N, Args...>::operator==(const SpectralBins<N, Args...>& other) const {
        return data_ == other.data_;
    }

    template <size_t N, auto... Args>
    constexpr bool SpectralBins<N, Args...>::operator!=(const SpectralBins<N, Args...>& other) const {
        return data_ != other.data_;
    }

    // ======================== //
    // === String Functions === //
    // ======================== //
    template <size_t N, auto... Args>
    std::string SpectralBins<N, Args...>::to_string() const {
        std::string result = "[";
        for (size_t i = 0; i < N; ++i) {
            result += std::to_string(data_[i]);
            if (i < (N - 1)) {
                result += ", ";
            }
        }
        result += "]";
        return result;
    }

    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> SpectralBins<N, Args...>::photon_energies()
    {
        SpectralBins<N, Args...> result;
        for (size_t i = 0; i < N; ++i) {
            double wavelength = static_cast<double>(bins_[i].center) * 1e-9;
            result[i] = static_cast<float>(photon_energy(wavelength));
        }
        return result;
    }

    // ================================== //
    // === Array Arithmetic Operators === //
    // ================================== //
    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator+(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result += rhs;
        return result;
    }

    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator-(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result -= rhs;
        return result;
    }

    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator*(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result *= rhs;
        return result;
    }

    template <size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator/(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result /= rhs;
        return result;
    }

    // ========================================= //
    // === Array-Scalar Arithmetic Operators === //
    // ========================================= //
    template <size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator+(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result += rhs;
        return result;
    }

    template <size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator-(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result -= rhs;
        return result;
    }

    template <size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator*(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result *= rhs;
        return result;
    }

    template <size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator/(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result /= rhs;
        return result;
    }

    // ========================================== //
    // === Scalar-Array Arithmetic Operators ==== //
    // ========================================== //
    template <size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator+(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        return rhs + lhs;
    }

    template <size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator*(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        return rhs * lhs;
    }

    template <size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator-(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result;
        for (size_t i = 0; i < N; ++i) {
            result[i] = static_cast<float>(lhs) - rhs[i];
        }
        return result;
    }

    template <size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator/(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result;
        for (size_t i = 0; i < N; ++i) {
            result[i] = static_cast<float>(lhs) / rhs[i];
        }
        return result;
    }

    // ======================== //
    // === Stream Operator === //
    // ======================== //
    template <size_t N, auto... Args>
    std::ostream& operator<<(std::ostream& os, const SpectralBins<N, Args...>& v) {
        os << v.to_string();
        return os;
    }


    // ======================= //
    // === Static Functions === //
    // ======================= //
    template <size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_bins_static_() {
        constexpr size_t num_args = sizeof...(Args);
        if constexpr (num_args == 2) {
            static_assert(N > 0, "Must have at least 1 bin");
            return initialize_uniform_static_();
        }
        else if constexpr (num_args == 2 * N) {
            return initialize_pairs_static_();
        }
        else if constexpr (num_args == N + 1) {
            return initialize_edges_static_();
        }
        else {
            static_assert(num_args == 2 || num_args == 2 * N || num_args == N + 1,
                "Must provide either 2 args (uniform), 2*N args (pairs), or N+1 args (edges)");
            return {};
        }
    }

    template <size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_uniform_static_() {
        auto args_array = std::array<float, 2>{ static_cast<float>(Args)... };
        float min_val = args_array[0];
        float max_val = args_array[1];
        float step = (max_val - min_val) / N;

        std::array<Bin, N> result{};
        for (size_t i = 0; i < N; ++i) {
            float bin_min = min_val + static_cast<float>(i) * step;
            float bin_max = min_val + static_cast<float>(i + 1) * step;
            result[i] = Bin(bin_min, bin_max);
        }
        return result;
    }

    template <size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_pairs_static_() {
        auto args_array = std::array<float, 2 * N>{ static_cast<float>(Args)... };
        std::array<Bin, N> result{};
        for (size_t i = 0; i < N; ++i) {
            result[i] = Bin(args_array[2 * i], args_array[2 * i + 1]);
        }
        return result;
    }

    template <size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_edges_static_() {
        auto args_array = std::array<float, N + 1>{ static_cast<float>(Args)... };
        std::array<Bin, N> result{};
        for (size_t i = 0; i < N; ++i) {
            result[i] = Bin(args_array[i], args_array[i + 1]);
        }
        return result;
    }
}
