#include <array>
#include <initializer_list>
#include <algorithm>
#include <type_traits>
#include <string>
#include <iostream>

#include "huira/detail/concepts/numeric_concepts.hpp"

namespace huira {
    // ========================== //
    // === Summary Operations === //
    // ========================== //
    template <IsFloatingPoint T, size_t N>
    T NumericArray<T, N>::total() const {
        T sum = T{ 0 };
        // Simple loop that compilers can auto-vectorize with reduction
        for (size_t i = 0; i < N; ++i) {
            sum += data_[i];
        }
        return sum;
    }

    template <IsFloatingPoint T, size_t N>
    T NumericArray<T, N>::magnitude() const {
        T sum_of_squares = T{ 0 };
        // Dot product - highly SIMD-optimizable
        for (size_t i = 0; i < N; ++i) {
            sum_of_squares += data_[i] * data_[i];
        }
        return std::sqrt(sum_of_squares);
    }

    template <IsFloatingPoint T, size_t N>
    T NumericArray<T, N>::max() const {
        if constexpr (N == 0) {
            return T{};
        }
        T max_val = data_[0];
        // Max reduction - SIMD-friendly
        for (size_t i = 1; i < N; ++i) {
            if (data_[i] > max_val) {
                max_val = data_[i];
            }
        }
        return max_val;
    }

    template <IsFloatingPoint T, size_t N>
    T NumericArray<T, N>::min() const {
        if constexpr (N == 0) {
            return T{};
        }
        T min_val = data_[0];
        // Min reduction - SIMD-friendly
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
    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator+=(const NumericArray<T, N>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] += other.data_[i];
        }
        return *this;
    }

    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator-=(const NumericArray<T, N>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] -= other.data_[i];
        }
        return *this;
    }

    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator*=(const NumericArray<T, N>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] *= other.data_[i];
        }
        return *this;
    }

    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator/=(const NumericArray<T, N>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] /= other.data_[i];
        }
        return *this;
    }


    // ========================================== //
    // === Array-Scalar Arithmetic Operations === //
    // ========================================== //
    template <IsFloatingPoint T, size_t N>
    template <IsNumeric U>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator+=(const U& scalar) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] += static_cast<T>(scalar);
        }
        return *this;
    }

    template <IsFloatingPoint T, size_t N>
    template <IsNumeric U>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator-=(const U& scalar) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] -= static_cast<T>(scalar);
        }
        return *this;
    }

    template <IsFloatingPoint T, size_t N>
    template <IsNumeric U>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator*=(const U& scalar) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] *= static_cast<T>(scalar);
        }
        return *this;
    }

    template <IsFloatingPoint T, size_t N>
    template <IsNumeric U>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator/=(const U& scalar) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] /= static_cast<T>(scalar);
        }
        return *this;
    }


    // ======================= //
    // === Unary Operators === //
    // ======================= //
    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N> NumericArray<T, N>::operator+() const {
        return *this;
    }

    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N> NumericArray<T, N>::operator-() const {
        NumericArray<T, N> result;
        for (size_t i = 0; i < N; ++i) {
            result.data_[i] = -data_[i];
        }
        return result;
    }


    // ============================ //
    // === Comparison Operators === //
    // ============================ //
    template <IsFloatingPoint T, size_t N>
    constexpr bool NumericArray<T, N>::operator==(const NumericArray<T, N>& other) const {
        return data_ == other.data_;
    }

    template <IsFloatingPoint T, size_t N>
    constexpr bool NumericArray<T, N>::operator!=(const NumericArray<T, N>& other) const {
        return data_ != other.data_;
    }


    // ======================== //
    // === String Functions === //
    // ======================== //
    template <IsFloatingPoint T, size_t N>
    std::string NumericArray<T, N>::toString() const {
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

    template <IsFloatingPoint T, size_t N>
    std::ostream& operator<<(std::ostream& os, const NumericArray<T, N>& v) {
        os << v.toString();
        return os;
    }


    // ================================== //
    // === Array Arithmetic Operators === //
    // ================================== //
    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N> operator+(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs) {
        NumericArray<T, N> result = lhs;
        result += rhs;
        return result;
    }

    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N> operator-(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs) {
        NumericArray<T, N> result = lhs;
        result -= rhs;
        return result;
    }

    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N> operator*(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs) {
        NumericArray<T, N> result = lhs;
        result *= rhs;
        return result;
    }

    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N> operator/(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs) {
        NumericArray<T, N> result = lhs;
        result /= rhs;
        return result;
    }

    template <IsFloatingPoint T, size_t N>
    constexpr NumericArray<T, N> operator%(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs)
        requires std::integral<T> {
        NumericArray<T, N> result = lhs;
        result %= rhs;
        return result;
    }

    
    // ========================================= //
    // === Array-Scalar Arithmetic Operators === //
    // ========================================= //
    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator+(const NumericArray<T, N>& lhs, const U& rhs) {
        NumericArray<T, N> result = lhs;
        result += rhs;
        return result;
    }

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator-(const NumericArray<T, N>& lhs, const U& rhs) {
        NumericArray<T, N> result = lhs;
        result -= rhs;
        return result;
    }

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator*(const NumericArray<T, N>& lhs, const U& rhs) {
        NumericArray<T, N> result = lhs;
        result *= rhs;
        return result;
    }

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator/(const NumericArray<T, N>& lhs, const U& rhs) {
        NumericArray<T, N> result = lhs;
        result /= rhs;
        return result;
    }

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator%(const NumericArray<T, N>& lhs, const U& rhs)
        requires std::integral<T>&& std::integral<U> {
        NumericArray<T, N> result = lhs;
        result %= rhs;
        return result;
    }


    // ========================================== //
    // === Scalar-Array Arithmetic Operators ==== //
    // ========================================== //
    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator+(const U& lhs, const NumericArray<T, N>& rhs) {
        return rhs + lhs;
    }

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator*(const U& lhs, const NumericArray<T, N>& rhs) {
        return rhs * lhs;
    }

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator-(const U& lhs, const NumericArray<T, N>& rhs) {
        NumericArray<T, N> result;
        for (size_t i = 0; i < N; ++i) {
            result[i] = static_cast<T>(lhs) - rhs[i];
        }
        return result;
    }

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator/(const U& lhs, const NumericArray<T, N>& rhs) {
        NumericArray<T, N> result;
        for (size_t i = 0; i < N; ++i) {
            result[i] = static_cast<T>(lhs) / rhs[i];
        }
        return result;
    }
}