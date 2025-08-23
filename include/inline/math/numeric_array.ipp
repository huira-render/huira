#include <array>
#include <initializer_list>
#include <algorithm>
#include <type_traits>
#include <string>
#include <iostream>

#include "huira/detail/concepts/numeric_concepts.hpp"

namespace huira {
	// ========================================= //
	// === Array-Array Arithmetic operations === //
	// ========================================= //
    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator+=(const NumericArray<T, N>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] += other.data_[i];
        }
        return *this;
    }

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator-=(const NumericArray<T, N>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] -= other.data_[i];
        }
        return *this;
    }

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator*=(const NumericArray<T, N>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] *= other.data_[i];
        }
        return *this;
    }

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator/=(const NumericArray<T, N>& other) {
        for (size_t i = 0; i < N; ++i) {
            data_[i] /= other.data_[i];
        }
        return *this;
    }

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator%=(const NumericArray<T, N>& other)
        requires std::integral<T> {
        for (size_t i = 0; i < N; ++i) {
            data_[i] %= other.data_[i];
        }
        return *this;
    }


    // ========================================== //
    // === Array-Scalar Arithmetic Operations === //
    // ========================================== //
    template <IsNumeric T, size_t N>
    template <IsNumeric U>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator+=(const U& scalar) {
        for (auto& elem : data_) {
            elem += static_cast<T>(scalar);
        }
        return *this;
    }

    template <IsNumeric T, size_t N>
    template <IsNumeric U>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator-=(const U& scalar) {
        for (auto& elem : data_) {
            elem -= static_cast<T>(scalar);
        }
        return *this;
    }

    template <IsNumeric T, size_t N>
    template <IsNumeric U>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator*=(const U& scalar) {
        for (auto& elem : data_) {
            elem *= static_cast<T>(scalar);
        }
        return *this;
    }

    template <IsNumeric T, size_t N>
    template <IsNumeric U>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator/=(const U& scalar) {
        for (auto& elem : data_) {
            elem /= static_cast<T>(scalar);
        }
        return *this;
    }

    template <IsNumeric T, size_t N>
    template <IsNumeric U>
    constexpr NumericArray<T, N>& NumericArray<T, N>::operator%=(const U& scalar)
        requires std::integral<T>&& std::integral<U> {
        for (auto& elem : data_) {
            elem %= static_cast<T>(scalar);
        }
        return *this;
    }


    // ======================= //
    // === Unary Operators === //
    // ======================= //
    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N> NumericArray<T, N>::operator+() const {
        return *this;
    }

    template <IsNumeric T, size_t N>
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
    template <IsNumeric T, size_t N>
    constexpr bool NumericArray<T, N>::operator==(const NumericArray<T, N>& other) const {
        return data_ == other.data_;
    }

    template <IsNumeric T, size_t N>
    constexpr bool NumericArray<T, N>::operator!=(const NumericArray<T, N>& other) const {
        return data_ != other.data_;
    }


    // ======================== //
    // === String Functions === //
    // ======================== //
    template <IsNumeric T, size_t N>
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

    template <IsNumeric T, size_t N>
    std::ostream& operator<<(std::ostream& os, const NumericArray<T, N>& v) {
        os << v.toString();
        return os;
    }


    // ================================== //
    // === Array Arithmetic Operators === //
    // ================================== //
    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N> operator+(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs) {
        NumericArray<T, N> result = lhs;
        result += rhs;
        return result;
    }

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N> operator-(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs) {
        NumericArray<T, N> result = lhs;
        result -= rhs;
        return result;
    }

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N> operator*(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs) {
        NumericArray<T, N> result = lhs;
        result *= rhs;
        return result;
    }

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N> operator/(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs) {
        NumericArray<T, N> result = lhs;
        result /= rhs;
        return result;
    }

    template <IsNumeric T, size_t N>
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