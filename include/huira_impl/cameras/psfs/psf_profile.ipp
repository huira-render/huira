#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>

#include "huira/core/constants.hpp"
#include "huira/util/logger.hpp"

namespace huira::detail {

/**
 * @brief The profile for a shoulder a, slope s and, if given, an outer radius b.
 *
 * Its total is 2 pi times the integral of r S(r), which with r^2 = a^2 (e^t - 1) becomes
 * pi a^2 times the integral over t >= 0 of e^(-s t / 2) / ((1 - h) e^-t + h), with h = a^2 / b^2.
 * Without an outer radius that is pi a^2 2 / (s - 2). With one, it is integrated numerically on
 * eight-point Gauss-Legendre panels a quarter wide, to 40 beyond where the denominator settles
 * on h, and the rest in closed form; this is exact to rounding.
 *
 * @throws std::runtime_error unless the shoulder and slope are positive and finite, and either an
 *         outer radius beyond the shoulder is given or the slope is over 2.
 */
inline ScatterProfile::ScatterProfile(double shoulder, double slope, std::optional<double> outer)
    : shoulder_squared_{shoulder * shoulder}, falloff_{slope}
{
    if (!(shoulder > 0.0) || !std::isfinite(shoulder) || !(slope > 0.0) || !std::isfinite(slope)) {
        HUIRA_THROW_ERROR("ScatterProfile - The shoulder and slope must be positive and finite: " +
                          std::to_string(shoulder) + ", " + std::to_string(slope));
    }
    const double pi = PI<double>();
    if (!outer.has_value()) {
        if (!(slope > 2.0)) {
            HUIRA_THROW_ERROR("ScatterProfile - Without an outer radius the slope must be over 2, "
                              "or the scattered light has no finite total: " +
                              std::to_string(slope));
        }
        peak_ = (slope - 2.0) / (2.0 * pi * shoulder_squared_);
        return;
    }
    if (!(*outer > shoulder) || !std::isfinite(*outer)) {
        HUIRA_THROW_ERROR("ScatterProfile - The outer radius must be finite and beyond the "
                          "shoulder: " +
                          std::to_string(*outer) + ", shoulder " + std::to_string(shoulder));
    }
    outer_squared_ = *outer * *outer;

    const double h = shoulder_squared_ / *outer_squared_;
    const auto integrand = [&](double t) {
        return std::exp(-0.5 * slope * t) / ((1.0 - h) * std::exp(-t) + h);
    };
    constexpr double PANEL = 0.25;
    constexpr double SETTLED = 40.0;
    const double end = std::log(1.0 / h) + SETTLED;
    const auto panels = static_cast<std::size_t>(std::ceil(end / PANEL));
    const double width = end / static_cast<double>(panels);
    double total = 0.0;
    for (std::size_t k = 0; k < panels; ++k) {
        const double start = static_cast<double>(k) * width;
        total += airy_band_detail::gauss_legendre_8(integrand, start, start + width);
    }
    // Beyond the end the denominator is h to a part in e^40.
    total += 2.0 / (slope * h) * std::exp(-0.5 * slope * end);
    peak_ = 1.0 / (pi * shoulder_squared_ * total);
}

/// Light per unit area at a distance r from the source.
inline double ScatterProfile::operator()(double r) const
{
    const double r2 = r * r;
    double value = peak_ * std::pow(1.0 + r2 / shoulder_squared_, -0.5 * falloff_);
    if (outer_squared_.has_value()) {
        value /= 1.0 + r2 / *outer_squared_;
    }
    return value;
}

/// The derivative of the profile with distance.
inline double ScatterProfile::slope(double r) const
{
    return derivatives(r)[1];
}

/**
 * @brief The profile and its first two derivatives with distance, from those of its logarithm:
 * L' = -s r / (a^2 + r^2) - 2 r / (b^2 + r^2), and S'' = S (L'^2 + L'').
 */
inline std::array<double, 3> ScatterProfile::derivatives(double r) const
{
    const double value = (*this)(r);
    const double r2 = r * r;
    const double a2 = shoulder_squared_;
    double log_first = -falloff_ * r / (a2 + r2);
    double log_second = -falloff_ * (a2 - r2) / ((a2 + r2) * (a2 + r2));
    if (outer_squared_.has_value()) {
        const double b2 = *outer_squared_;
        log_first -= 2.0 * r / (b2 + r2);
        log_second -= 2.0 * (b2 - r2) / ((b2 + r2) * (b2 + r2));
    }
    return {value, value * log_first, value * (log_first * log_first + log_second)};
}

/**
 * @brief The profile of a channel's diffraction pattern, with scatter_fraction of its light
 * scattered as the scatter profile says.
 *
 * @throws std::runtime_error if the fraction is outside [0, 1), or positive without a scatter
 *         profile.
 */
inline OpticsProfile::OpticsProfile(const AiryBandProfile& diffraction,
                                    double scatter_fraction,
                                    std::optional<ScatterProfile> scatter)
    : diffraction_{diffraction}, scatter_fraction_{scatter_fraction}, scatter_{scatter}
{
    if (!(scatter_fraction >= 0.0 && scatter_fraction < 1.0) ||
        (scatter_fraction > 0.0 && !scatter.has_value())) {
        HUIRA_THROW_ERROR("OpticsProfile - The scattered fraction must be in [0, 1), with a "
                          "profile when positive: " +
                          std::to_string(scatter_fraction));
    }
}

/// Light per unit area at a distance r from the source.
inline double OpticsProfile::operator()(double r) const
{
    double value = (1.0 - scatter_fraction_) * diffraction_(r);
    if (scatter_fraction_ > 0.0) {
        value += scatter_fraction_ * (*scatter_)(r);
    }
    return value;
}

/// The derivative of the profile with distance.
inline double OpticsProfile::slope(double r) const
{
    double value = (1.0 - scatter_fraction_) * diffraction_.slope(r);
    if (scatter_fraction_ > 0.0) {
        value += scatter_fraction_ * scatter_->slope(r);
    }
    return value;
}

} // namespace huira::detail
