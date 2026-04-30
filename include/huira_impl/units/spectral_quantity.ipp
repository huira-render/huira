#pragma once

namespace huira::units {

    // Default constructor
    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    constexpr SpectralQuantity<Dim, Scale, TSpectral>::SpectralQuantity()
        : value_{ 0 }
    {
    }

    // Construct from spectral data
    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    constexpr SpectralQuantity<Dim, Scale, TSpectral>::SpectralQuantity(const TSpectral& spectral_value)
        : value_(spectral_value)
    {
    }

    // Convert from another scale of the same dimensionality
    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    template <typename OtherScale>
    constexpr SpectralQuantity<Dim, Scale, TSpectral>::SpectralQuantity(
        const SpectralQuantity<Dim, OtherScale, TSpectral>& other)
    {
        constexpr double other_ratio = static_cast<double>(OtherScale::num) / static_cast<double>(OtherScale::den);
        constexpr double our_ratio = get_ratio();
        constexpr double conversion = other_ratio / our_ratio;

        const auto& other_value = other.value();
        for (std::size_t i = 0; i < TSpectral::size(); ++i) {
            value_[i] = static_cast<float>(static_cast<double>(other_value[i]) * conversion);
        }
    }

    // Access underlying spectral data
    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    constexpr const TSpectral& SpectralQuantity<Dim, Scale, TSpectral>::value() const {
        return value_;
    }

    // Convert to SI
    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    TSpectral SpectralQuantity<Dim, Scale, TSpectral>::to_si() const {
        if constexpr (Scale::num == 1 && Scale::den == 1) {
            return value_;
        }
        else {
            TSpectral result;
            constexpr double ratio = get_ratio();
            for (std::size_t i = 0; i < TSpectral::size(); ++i) {
                result[i] = static_cast<float>(static_cast<double>(value_[i]) * ratio);
            }
            return result;
        }
    }

    // Convert to a different scale
    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    template <typename NewScale>
    SpectralQuantity<Dim, NewScale, TSpectral> SpectralQuantity<Dim, Scale, TSpectral>::as() const {
        return SpectralQuantity<Dim, NewScale, TSpectral>(*this);
    }

    // String representation
    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    std::string SpectralQuantity<Dim, Scale, TSpectral>::to_string() const {
        std::string result = "SpectralQuantity[" + Dim::to_si_string() + "](";
        for (std::size_t i = 0; i < TSpectral::size(); ++i) {
            if (i > 0) result += ", ";
            result += std::to_string(value_[i]);
        }
        result += ")";
        return result;
    }

    // Comparison
    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    constexpr bool SpectralQuantity<Dim, Scale, TSpectral>::operator==(const SpectralQuantity& other) const {
        return value_ == other.value_;
    }

    template <IsDimensionality Dim, typename Scale, IsSpectral TSpectral>
    constexpr bool SpectralQuantity<Dim, Scale, TSpectral>::operator!=(const SpectralQuantity& other) const {
        return value_ != other.value_;
    }

}
