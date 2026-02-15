#pragma once

#include <ratio>
#include <string>
#include <type_traits>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/units/dimensionality.hpp"
#include "huira/core/units/quantity.hpp"

namespace huira::units {

    /**
     * @brief A spectral data container with associated physical unit information.
     *
     * SpectralQuantity wraps a spectral type (e.g. RGB, Visible8) and associates it
     * with a physical dimensionality and scale, providing compile-time unit safety
     * for spectral data.
     *
     * @section spectral_quantity_usage_examples Usage Examples
     *
     * @code{.cpp}
     * using TSpectral = huira::RGB;
     *
     * // Construct from spectral data (values are in the specified unit)
     * SpectralWatts<TSpectral> power(TSpectral{10.0f, 20.0f, 30.0f});
     *
     * // Convert to SI representation
     * TSpectral si_values = power.to_si();
     *
     * // Convert between compatible scales
     * SpectralKilowatts<TSpectral> kw = power.as<std::kilo>();
     *
     * // Access the underlying spectral data
     * const TSpectral& raw = power.value();
     * @endcode
     *
     * @tparam Dim    The dimensionality type (e.g. Power, Irradiance).
     * @tparam Scale  The scale ratio relative to SI (e.g. std::ratio<1,1> for base, std::kilo for kilo-).
     * @tparam TSpectral The spectral representation type (must satisfy IsSpectral).
     */
    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    class SpectralQuantity {
    public:
        using dimension_type = Dim;
        using scale_type = Scale;
        using spectral_type = TSpectral;

        /// Default constructor. Initializes spectral data to zero.
        constexpr SpectralQuantity();

        /// Construct from spectral data. Values are interpreted in this quantity's unit scale.
        explicit constexpr SpectralQuantity(const TSpectral& spectral_value);

        /// Convert from another SpectralQuantity with the same dimensionality but different scale.
        template <typename OtherScale>
        constexpr SpectralQuantity(const SpectralQuantity<Dim, OtherScale, TSpectral>& other);

        // Default copy/move
        constexpr SpectralQuantity(const SpectralQuantity&) = default;
        constexpr SpectralQuantity(SpectralQuantity&&) = default;
        constexpr SpectralQuantity& operator=(const SpectralQuantity&) = default;
        constexpr SpectralQuantity& operator=(SpectralQuantity&&) = default;

        /// Get the underlying spectral data in the current unit's scale.
        constexpr const TSpectral& value() const;

        /// Convert the spectral data to SI base units.
        TSpectral to_si() const;

        /// Convert to a SpectralQuantity with a different scale of the same dimensionality.
        template <typename NewScale>
        SpectralQuantity<Dim, NewScale, TSpectral> as() const;

        /// String representation.
        std::string to_string() const;

        // Comparison operators
        constexpr bool operator==(const SpectralQuantity& other) const;
        constexpr bool operator!=(const SpectralQuantity& other) const;

    private:
        TSpectral value_{ 0 };

        static constexpr double get_ratio() {
            return static_cast<double>(Scale::num) / static_cast<double>(Scale::den);
        }
    };

    // Concept for detecting SpectralQuantity types
    template <typename>
    struct is_spectral_quantity : std::false_type {};

    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    struct is_spectral_quantity<SpectralQuantity<Dim, Scale, TSpectral>> : std::true_type {};

    template <typename T>
    concept IsSpectralQuantity = is_spectral_quantity<T>::value;

}

#include "huira_impl/core/units/spectral_quantity.ipp"
