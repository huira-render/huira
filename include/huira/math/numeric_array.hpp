#pragma once

#include <array>
#include <initializer_list>
#include <algorithm>
#include <type_traits>

#include "huira/detail/concepts/numeric_concepts.hpp"

namespace huira {
    template <IsNumeric T, size_t N>
    class NumericArray {
    public:
        // Type aliases
        using value_type = T;
        using size_type = std::size_t;
        using reference = T&;
        using const_reference = const T&;
        using iterator = typename std::array<T, N>::iterator;
        using const_iterator = typename std::array<T, N>::const_iterator;

        // Constructors
        constexpr NumericArray() = default;

        explicit constexpr NumericArray(const T& value) {
            data_.fill(value);
        }

        constexpr NumericArray(std::initializer_list<T> init) {
            std::copy(init.begin(),
                init.begin() + std::min(init.size(), N),
                data_.begin());
        }

        template <typename... Args>
            requires (sizeof...(Args) == N && (std::convertible_to<Args, T> && ...))
        constexpr NumericArray(Args&&... args) : data_{ static_cast<T>(args)... } {}

        // Copy and move constructors
        constexpr NumericArray(const NumericArray&) = default;
        constexpr NumericArray(NumericArray&&) = default;

        // Assignment operators
        constexpr NumericArray& operator=(const NumericArray&) = default;
        constexpr NumericArray& operator=(NumericArray&&) = default;

        // Element access
        constexpr reference operator[](size_type pos) { return data_[pos]; }
        constexpr const_reference operator[](size_type pos) const { return data_[pos]; }

        constexpr reference at(size_type pos) { return data_.at(pos); }
        constexpr const_reference at(size_type pos) const { return data_.at(pos); }

        constexpr reference front() { return data_.front(); }
        constexpr const_reference front() const { return data_.front(); }

        constexpr reference back() { return data_.back(); }
        constexpr const_reference back() const { return data_.back(); }

        constexpr T* data() noexcept { return data_.data(); }
        constexpr const T* data() const noexcept { return data_.data(); }

        // Iterators
        constexpr iterator begin() noexcept { return data_.begin(); }
        constexpr const_iterator begin() const noexcept { return data_.begin(); }
        constexpr const_iterator cbegin() const noexcept { return data_.cbegin(); }

        constexpr iterator end() noexcept { return data_.end(); }
        constexpr const_iterator end() const noexcept { return data_.end(); }
        constexpr const_iterator cend() const noexcept { return data_.cend(); }

        // Capacity
        constexpr bool empty() const noexcept { return N == 0; }
        constexpr size_type size() const noexcept { return N; }
        constexpr size_type max_size() const noexcept { return N; }

        // Operations
        constexpr void fill(const T& value) { data_.fill(value); }

        constexpr void swap(NumericArray& other) noexcept(std::is_nothrow_swappable_v<T>) {
            data_.swap(other.data_);
        }

        // Arithmetic operations with other NumericArray
        constexpr NumericArray& operator+=(const NumericArray& other);
        constexpr NumericArray& operator-=(const NumericArray& other);
        constexpr NumericArray& operator*=(const NumericArray& other);
        constexpr NumericArray& operator/=(const NumericArray& other);

        constexpr NumericArray& operator%=(const NumericArray& other) 
            requires std::integral<T>;

        // Arithmetic operations with scalars
        template <IsNumeric U>
        constexpr NumericArray& operator+=(const U& scalar);

        template <IsNumeric U>
        constexpr NumericArray& operator-=(const U& scalar);

        template <IsNumeric U>
        constexpr NumericArray& operator*=(const U& scalar);

        template <IsNumeric U>
        constexpr NumericArray& operator/=(const U& scalar);

        template <IsNumeric U>
        constexpr NumericArray& operator%=(const U& scalar)
            requires std::integral<T>&& std::integral<U>;

        // Unary operators
        constexpr NumericArray operator+() const;
        constexpr NumericArray operator-() const;

        // Comparison operators
        constexpr bool operator==(const NumericArray& other) const;
        constexpr bool operator!=(const NumericArray& other) const;

    private:
        std::array<T, N> data_;
    };

    // Binary arithmetic operators (array + array)
    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N> operator+(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs);

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N> operator-(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs);

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N> operator*(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs);

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N> operator/(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs);

    template <IsNumeric T, size_t N>
    constexpr NumericArray<T, N> operator%(const NumericArray<T, N>& lhs, const NumericArray<T, N>& rhs)
        requires std::integral<T>;

    // Binary arithmetic operators (array + scalar)
    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator+(const NumericArray<T, N>& lhs, const U& rhs);

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator-(const NumericArray<T, N>& lhs, const U& rhs);

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator*(const NumericArray<T, N>& lhs, const U& rhs);

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator/(const NumericArray<T, N>& lhs, const U& rhs);

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator%(const NumericArray<T, N>& lhs, const U& rhs)
        requires std::integral<T>&& std::integral<U>;

    // Binary arithmetic operators (scalar + array)
    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator+(const U& lhs, const NumericArray<T, N>& rhs);

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator*(const U& lhs, const NumericArray<T, N>& rhs);

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator-(const U& lhs, const NumericArray<T, N>& rhs);

    template <IsNumeric T, size_t N, IsNumeric U>
    constexpr NumericArray<T, N> operator/(const U& lhs, const NumericArray<T, N>& rhs);

    // Utility function for swapping
    template <IsNumeric T, size_t N>
    constexpr void swap(NumericArray<T, N>& lhs, NumericArray<T, N>& rhs)
        noexcept(noexcept(lhs.swap(rhs))) {
        lhs.swap(rhs);
    }

    // Deduction guides
    template <typename T, typename... U>
    NumericArray(T, U...) -> NumericArray<T, 1 + sizeof...(U)>;
}

#include "inline/math/numeric_array.ipp"