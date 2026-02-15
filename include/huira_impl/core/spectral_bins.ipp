#include <array>
#include <iostream>
#include <string>

#include "huira/core/physics.hpp"

namespace huira {
    // ==================== //
    // === Constructors === //
    // ==================== //
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>::SpectralBins()
    {
        data_.fill(0.0f);
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>::SpectralBins(const float& value)
    {
        data_.fill(value);
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>::SpectralBins(std::initializer_list<float> init)
    {
        if (init.size() == 1) {
            data_.fill(*init.begin());
        }
        else {
            std::copy(init.begin(),
                std::next(init.begin(), static_cast<std::ptrdiff_t>(std::min(init.size(), N))),
                data_.begin());
        }
    }

    template <std::size_t N, auto... Args>
    template <typename... Args2>
        requires (sizeof...(Args2) == N && (std::convertible_to<Args2, float> && ...))
    constexpr SpectralBins<N, Args...>::SpectralBins(Args2&&... args)
        : data_{ static_cast<float>(args)... }
    {
        
    }

    template <std::size_t N, auto... Args>
    bool SpectralBins<N, Args...>::valid() const
    {
        for (std::size_t i = 0; i < N; ++i) {
            if (data_[i] < 0.f) {
                return false;
            }

            if (std::isnan(data_[i])) {
                return false;
            }

            if (std::isinf(data_[i])) {
                return false;
            }
        }
        return true;
    }

    // ========================= //
    // === Static Definition === //
    // ========================= //
    template <std::size_t N, auto... Args>
    std::array<Bin, N> SpectralBins<N, Args...>::bins_ = SpectralBins<N, Args...>::initialize_bins_static_();


    // ========================== //
    // === Summary Operations === //
    // ========================== //
    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::total() const {
        float sum = 0.0f;
        for (std::size_t i = 0; i < N; ++i) {
            sum += data_[i];
        }
        return sum;
    }

    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::magnitude() const {
        float sum_of_squares = 0.0f;
        for (std::size_t i = 0; i < N; ++i) {
            sum_of_squares += data_[i] * data_[i];
        }
        return std::sqrt(sum_of_squares);
    }

    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::max() const {
        if constexpr (N == 0) {
            return 0.0f;
        }
        float max_val = data_[0];
        for (std::size_t i = 1; i < N; ++i) {
            if (data_[i] > max_val) {
                max_val = data_[i];
            }
        }
        return max_val;
    }

    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::min() const {
        if constexpr (N == 0) {
            return 0.0f;
        }
        float min_val = data_[0];
        for (std::size_t i = 1; i < N; ++i) {
            if (data_[i] < min_val) {
                min_val = data_[i];
            }
        }
        return min_val;
    }

    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::integrate() const {
        float integral = 0.0f;
        for (std::size_t i = 0; i < N; ++i) {
            float bin_width = (bins_[i].max_wavelength - bins_[i].min_wavelength);
            integral += data_[i] * bin_width;
        }
        return integral;

    }


    // ========================================= //
    // === Array-Array Arithmetic Operations === //
    // ========================================= //
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator+=(const SpectralBins<N, Args...>& other) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] += other.data_[i];
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator-=(const SpectralBins<N, Args...>& other) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] -= other.data_[i];
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator*=(const SpectralBins<N, Args...>& other) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] *= other.data_[i];
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator/=(const SpectralBins<N, Args...>& other) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] /= other.data_[i];
        }
        return *this;
    }

    // ========================================== //
    // === Array-Scalar Arithmetic Operations === //
    // ========================================== //
    template <std::size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator+=(const U& scalar) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] += static_cast<float>(scalar);
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator-=(const U& scalar) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] -= static_cast<float>(scalar);
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator*=(const U& scalar) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] *= static_cast<float>(scalar);
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator/=(const U& scalar) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] /= static_cast<float>(scalar);
        }
        return *this;
    }

    // ======================= //
    // === Unary Operators === //
    // ======================= //
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> SpectralBins<N, Args...>::operator+() const {
        return *this;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> SpectralBins<N, Args...>::operator-() const {
        SpectralBins<N, Args...> result;
        for (std::size_t i = 0; i < N; ++i) {
            result.data_[i] = -data_[i];
        }
        return result;
    }

    // ============================ //
    // === Comparison Operators === //
    // ============================ //
    template <std::size_t N, auto... Args>
    constexpr bool SpectralBins<N, Args...>::operator==(const SpectralBins<N, Args...>& other) const {
        return data_ == other.data_;
    }

    template <std::size_t N, auto... Args>
    constexpr bool SpectralBins<N, Args...>::operator!=(const SpectralBins<N, Args...>& other) const {
        return data_ != other.data_;
    }

    // ======================== //
    // === String Functions === //
    // ======================== //
    template <std::size_t N, auto... Args>
    std::string SpectralBins<N, Args...>::to_string() const {
        std::string result = "[";
        for (std::size_t i = 0; i < N; ++i) {
            result += std::to_string(data_[i]);
            if (i < (N - 1)) {
                result += ", ";
            }
        }
        result += "]";
        return result;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> SpectralBins<N, Args...>::photon_energies()
    {
        SpectralBins<N, Args...> result;
        for (std::size_t i = 0; i < N; ++i) {
            double wavelength = static_cast<double>(bins_[i].center_wavelength);
            result[i] = static_cast<float>(photon_energy(wavelength));
        }
        return result;
    }

    // ================================== //
    // === Array Arithmetic Operators === //
    // ================================== //
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator+(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result += rhs;
        return result;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator-(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result -= rhs;
        return result;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator*(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result *= rhs;
        return result;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator/(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result /= rhs;
        return result;
    }

    // ========================================= //
    // === Array-Scalar Arithmetic Operators === //
    // ========================================= //
    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator+(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result += rhs;
        return result;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator-(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result -= rhs;
        return result;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator*(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result *= rhs;
        return result;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator/(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result /= rhs;
        return result;
    }

    // ========================================== //
    // === Scalar-Array Arithmetic Operators ==== //
    // ========================================== //
    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator+(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        return rhs + lhs;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator*(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        return rhs * lhs;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator-(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result;
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = static_cast<float>(lhs) - rhs[i];
        }
        return result;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator/(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result;
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = static_cast<float>(lhs) / rhs[i];
        }
        return result;
    }

    // ======================== //
    // === Stream Operator === //
    // ======================== //
    template <std::size_t N, auto... Args>
    std::ostream& operator<<(std::ostream& os, const SpectralBins<N, Args...>& v) {
        os << v.to_string();
        return os;
    }


    // ======================= //
    // === Static Functions === //
    // ======================= //
    template <std::size_t N, auto... Args>
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

    template <std::size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_uniform_static_() {
        auto args_array = std::array<double, 2>{ static_cast<double>(Args)... };
        double min_val = args_array[0] * 1e-9;
        double max_val = args_array[1] * 1e-9;
        double step = (max_val - min_val) / static_cast<double>(N);

        std::array<Bin, N> result{};
        for (std::size_t i = 0; i < N; ++i) {
            double bin_min = min_val + static_cast<double>(i) * step;
            double bin_max = min_val + static_cast<double>(i + 1) * step;
            result[i] = Bin(bin_min, bin_max);
        }
        return result;
    }

    template <std::size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_pairs_static_() {
        auto args_array = std::array<double, 2 * N>{ static_cast<double>(Args)... };
        std::array<Bin, N> result{};
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = Bin(args_array[2 * i] * 1e-9, args_array[2 * i + 1] * 1e-9);
        }
        return result;
    }

    template <std::size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_edges_static_() {
        auto args_array = std::array<double, N + 1>{ static_cast<double>(Args)... };
        std::array<Bin, N> result{};
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = Bin(args_array[i] * 1e-9, args_array[i + 1] * 1e-9);
        }
        return result;
    }
}
