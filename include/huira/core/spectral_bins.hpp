#pragma once

#include <iostream>
#include <string>

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
    class SpectralBins {
    public:
        using value_type = float;
        using size_type = std::size_t;
        using reference = float&;
        using const_reference = const float&;
        using iterator = typename std::array<float, N>::iterator;
        using const_iterator = typename std::array<float, N>::const_iterator;

        // Constructors
        constexpr SpectralBins() { data_.fill(0.0f); }

        explicit constexpr SpectralBins(const float& value) { data_.fill(value); }

        constexpr SpectralBins(std::initializer_list<float> init) {
            std::copy(init.begin(),
                std::next(init.begin(), static_cast<std::ptrdiff_t>(std::min(init.size(), N))),
                data_.begin());
        }

        template <typename... Args2>
            requires (sizeof...(Args2) == N && (std::convertible_to<Args2, float> && ...))
        constexpr SpectralBins(Args2&&... args) : data_{ static_cast<float>(args)... } {}

        // Copy and move constructors
        constexpr SpectralBins(const SpectralBins&) = default;
        constexpr SpectralBins(SpectralBins&&) = default;

        // Assignment operators
        constexpr SpectralBins& operator=(const SpectralBins&) = default;
        constexpr SpectralBins& operator=(SpectralBins&&) = default;

        // Element access
        constexpr reference operator[](size_type pos) { return data_[pos]; }
        constexpr const_reference operator[](size_type pos) const { return data_[pos]; }

        constexpr reference at(size_type pos) { return data_.at(pos); }
        constexpr const_reference at(size_type pos) const { return data_.at(pos); }

        constexpr reference front() { return data_.front(); }
        constexpr const_reference front() const { return data_.front(); }

        constexpr reference back() { return data_.back(); }
        constexpr const_reference back() const { return data_.back(); }

        constexpr float* data() noexcept { return data_.data(); }
        constexpr const float* data() const noexcept { return data_.data(); }

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
        constexpr void fill(const float& value) { data_.fill(value); }

        // Summary
        float total() const;
        float magnitude() const;
        float max() const;
        float min() const;

        // Array-Array Arithmetic Operations
        constexpr SpectralBins& operator+=(const SpectralBins& other);
        constexpr SpectralBins& operator-=(const SpectralBins& other);
        constexpr SpectralBins& operator*=(const SpectralBins& other);
        constexpr SpectralBins& operator/=(const SpectralBins& other);

        // Array-Scalar Arithmetic Operations
        template <typename U>
        constexpr SpectralBins& operator+=(const U& scalar);

        template <typename U>
        constexpr SpectralBins& operator-=(const U& scalar);

        template <typename U>
        constexpr SpectralBins& operator*=(const U& scalar);

        template <typename U>
        constexpr SpectralBins& operator/=(const U& scalar);

        // Unary operators
        constexpr SpectralBins operator+() const;
        constexpr SpectralBins operator-() const;

        // Comparison operators
        constexpr bool operator==(const SpectralBins& other) const;
        constexpr bool operator!=(const SpectralBins& other) const;

        // String functions
        std::string toString() const;

        // Access to bin information
        constexpr const Bin& get_bin(size_t index) const { return bins_[index]; }
        constexpr const std::array<Bin, N>& get_all_bins() const { return bins_; }

    private:
        std::array<float, N> data_;
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
