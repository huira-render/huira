#pragma once

#include <iostream>
#include <string>

namespace huira {
    /**
     * @brief Represents a wavelength bin with minimum, maximum, and center wavelengths.
     * 
     * Wavelengths are stored in meters (SI units).
     */
    struct Bin {
        // Wavelengths are in meters:
        double min_wavelength = 0;
        double max_wavelength = 0;
        double center_wavelength = 0;

        constexpr Bin() = default;

        constexpr Bin(double new_min, double new_max) :
            min_wavelength{ new_min },
            max_wavelength{ new_max },
            center_wavelength{ (min_wavelength + max_wavelength) / 2.0 }
        {
        }
    };

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4686)  // Possible change in behavior in UDT return calling convention
#endif
    /**
     * @brief A template class representing spectral data distributed across wavelength bins.
     * 
     * SpectralBins provides a container for spectral values (e.g., radiance, irradiance)
     * discretized into wavelength bins. The bins can be configured with:
     * - Uniform spacing (2 args: min, max wavelengths in nm)
     * - Explicit pairs (2N args: min1, max1, min2, max2, ...)
     * - Bin edges (N+1 args: edge0, edge1, ..., edgeN)
     * 
     * The template arguments are to be given in units of nanometers.
     *
     * Supports standard container operations, arithmetic operations (element-wise and scalar),
     * and spectral analysis functions.
     * 
     * @tparam N The number of spectral bins.
     * @tparam Args Variadic template parameters defining bin configuration.
     */
    template <std::size_t N, auto... Args>
    class SpectralBins {
    public:
        using value_type = float;
        using size_type = std::size_t;
        using reference = float&;
        using const_reference = const float&;
        using iterator = typename std::array<float, N>::iterator;
        using const_iterator = typename std::array<float, N>::const_iterator;

        // Constructors
        constexpr SpectralBins();
        explicit constexpr SpectralBins(const float& value);
        constexpr SpectralBins(std::initializer_list<float> init);

        template <typename... Args2>
            requires (sizeof...(Args2) == N && (std::convertible_to<Args2, float> && ...))
        constexpr SpectralBins(Args2&&... args);

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
        static constexpr bool empty() noexcept { return N == 0; }
        static constexpr size_type size() noexcept { return N; }

        // Operations
        constexpr void fill(const float& value) { data_.fill(value); }
        static constexpr SpectralBins from_total(float total);

        // Summary
        float total() const;          ///< Sum of all spectral values.
        float magnitude() const;      ///< Euclidean magnitude (L2 norm) of the spectral vector.
        float max() const;            ///< Maximum value across all bins.
        float min() const;            ///< Minimum value across all bins.
        float integrate() const;      ///< Wavelength-weighted integral over all bins.
        bool valid() const;           ///< Checks if all spectral values are valid (non-negative, not NaN, not infinite).
        bool valid_albedo() const;    ///< Checks if all spectral values are valid albedo (between 0 and 1, not NaN, not infinite).

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
        std::string to_string() const;

        // Access to bin information
        static constexpr const Bin& get_bin(std::size_t index) { return bins_[index]; }
        static constexpr const std::array<Bin, N>& get_all_bins() { return bins_; }
        static constexpr SpectralBins photon_energies();

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

    /**
     * @brief Alias for creating uniformly spaced spectral bins.
     * 
     * @tparam N Number of bins.
     * @tparam min Minimum wavelength in nanometers.
     * @tparam max Maximum wavelength in nanometers.
     */
    template <std::size_t N, int min, int max>
    using UniformSpectralBins = SpectralBins<N, min, max>;

    /**
     * @brief Alias for creating spectral bins from explicit bin edges.
     * 
     * @tparam N Number of bins.
     * @tparam Args N+1 wavelength edges in nanometers.
     */
    template <std::size_t N, auto... Args>
    using SpectralBinEdges = SpectralBins<N, Args...>;

    /// @brief RGB representation with red (600-750nm), green (500-600nm), and blue (380-500nm) bins.
    using RGB = SpectralBins<3, 600, 750, 500, 600, 380, 500>;

    /// @brief 8 uniformly spaced bins covering the visible spectrum (380-750nm).
    using Visible8 = UniformSpectralBins<8, 380, 750>;
}

#include "huira_impl/core/spectral_bins.ipp"
