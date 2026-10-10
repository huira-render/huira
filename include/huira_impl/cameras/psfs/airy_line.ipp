#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "huira/cameras/psfs/airy_band.hpp"
#include "huira/core/constants.hpp"

namespace huira::detail {

namespace airy_line_detail {

/// Below this, V' and V'' come from their power series, where their closed forms cancel.
inline constexpr double SERIES_LIMIT = 1.0;

/**
 * @brief V'(x) = 2 / (3 pi x) - H1(x) / x^3 and V''(x), from H1's power series, for x below
 * SERIES_LIMIT: the series of H1(x) / x^3 without its first term, which V takes out.
 */
inline std::array<double, 2> line_tail_derivatives_series(double x)
{
    // H1(x) = (2 / pi) sum (-1)^k x^(2k + 2) / ((2k + 1)!! (2k + 3)!!).
    const double x2 = x * x;
    double term = x2 / 3.0; // k = 0
    double first = 0.0;
    double second = 0.0;
    for (int k = 1; k < 20; ++k) {
        term *= -x2 / ((2.0 * k + 1.0) * (2.0 * k + 3.0));
        // term is (-1)^k x^(2k + 2) / (...), and enters H1 / x^3 as x^(2k - 1).
        first -= term / (x2 * x);
        second -= (2.0 * k - 1.0) * term / (x2 * x2);
        if (std::abs(term) < 1e-18 * x2) {
            break;
        }
    }
    const double scale = 2.0 / PI<double>();
    return {scale * first, scale * second};
}

/// V' and V'' at x > 0.
inline std::array<double, 2> line_tail_derivatives(double x)
{
    if (x < SERIES_LIMIT) {
        return line_tail_derivatives_series(x);
    }
    const auto [h0, h1] = struve_h01(x);
    const double pi = PI<double>();
    const double x3 = x * x * x;
    return {2.0 / (3.0 * pi * x) - h1 / x3, -(h0 - 4.0 * h1 / x) / x3 - 2.0 / (3.0 * pi * x * x)};
}

} // namespace airy_line_detail

/**
 * @brief Struve's functions H0 and H1 at x >= 0, to about 1e-10 of their size.
 *
 * From their power series below 20, where it loses at most 1e-10 to cancellation, and above
 * from Y0 and Y1 and the asymptotic series of H - Y (DLMF 11.6.1), whose terms fall below 1e-10
 * before they start to grow.
 */
inline std::array<double, 2> struve_h01(double x)
{
    const double pi = PI<double>();
    if (x < airy_band_detail::ASYMPTOTIC_LIMIT) {
        // H0 = (2 / pi) sum (-1)^k x^(2k + 1) / ((2k + 1)!!)^2 and
        // H1 = (2 / pi) sum (-1)^k x^(2k + 2) / ((2k + 1)!! (2k + 3)!!).
        const double x2 = x * x;
        double term0 = x;
        double term1 = x2 / 3.0;
        double sum0 = term0;
        double sum1 = term1;
        for (int k = 0; k < 200; ++k) {
            term0 *= -x2 / ((2.0 * k + 3.0) * (2.0 * k + 3.0));
            term1 *= -x2 / ((2.0 * k + 3.0) * (2.0 * k + 5.0));
            sum0 += term0;
            sum1 += term1;
            if (k > x && std::abs(term0) < 1e-17 * std::abs(sum0) + 1e-300 &&
                std::abs(term1) < 1e-17 * std::abs(sum1) + 1e-300) {
                break;
            }
        }
        return {2.0 / pi * sum0, 2.0 / pi * sum1};
    }
    // H_n - Y_n = (1 / pi) sum_k Gamma(k + 1/2) / Gamma(n + 1/2 - k) (x / 2)^(n - 2k - 1): for
    // n = 0 the terms go 2 / x times (-(2k - 1)^2 / x^2) in turn, for n = 1, 2 times
    // ((2k - 1) (3 - 2k) / x^2).
    const double x2 = x * x;
    double term0 = 2.0 / x;
    double term1 = 2.0;
    double sum0 = term0;
    double sum1 = term1;
    for (int k = 1; k < 40; ++k) {
        const double next0 = term0 * -((2.0 * k - 1.0) * (2.0 * k - 1.0)) / x2;
        const double next1 = term1 * ((2.0 * k - 1.0) * (3.0 - 2.0 * k)) / x2;
        if (std::abs(next0) >= std::abs(term0) || (std::abs(next1) >= std::abs(term1) && k > 1)) {
            break;
        }
        term0 = next0;
        term1 = next1;
        sum0 += term0;
        sum1 += term1;
        if (std::abs(term0) < 1e-18 && std::abs(term1) < 1e-18) {
            break;
        }
    }
    const double cos_x = std::cos(x);
    const double sin_x = std::sin(x);
    return {airy_band_detail::bessel_y_hankel(0, x, cos_x, sin_x) + sum0 / pi,
            airy_band_detail::bessel_y_hankel(1, x, cos_x, sin_x) + sum1 / pi};
}

/**
 * @brief V(x) for large x: W from H1 = Y1 + (2 + 2 / x^2 - 6 / x^4 + 90 / x^6) / pi, its
 * non-oscillating part integrated term by term, and the integral of Y1 / t^3, Y0 / x^3 + 3 Y1 /
 * x^4, by parts. At x = 2048 its error is about 1e-17 of W.
 */
inline double airy_line_tail_asymptotic(double x)
{
    const double pi = PI<double>();
    const double x2 = x * x;
    const double x4 = x2 * x2;
    const double cos_x = std::cos(x);
    const double sin_x = std::sin(x);
    const double y0 = airy_band_detail::bessel_y_hankel(0, x, cos_x, sin_x);
    const double y1 = airy_band_detail::bessel_y_hankel(1, x, cos_x, sin_x);
    const double smooth = (1.0 / x2 + 0.5 / x4 - 1.0 / (x4 * x2) + 11.25 / (x4 * x4)) / pi;
    const double oscillating = y0 / (x2 * x) + 3.0 * y1 / x4;
    return smooth + oscillating + 2.0 * std::log(x) / (3.0 * pi);
}

/// The table, built the first time it is needed.
inline const AiryLineTail& AiryLineTail::instance()
{
    static const AiryLineTail table;
    return table;
}

inline AiryLineTail::AiryLineTail()
{
    using namespace airy_line_detail;
    const auto nodes = static_cast<std::size_t>(END / STEP) + 1;
    value_.resize(nodes);
    slope_.resize(nodes);
    curvature_.resize(nodes);
    for (std::size_t k = 0; k < nodes; ++k) {
        const double x = static_cast<double>(k) * STEP;
        const std::array<double, 2> derivatives =
            k == 0 ? std::array<double, 2>{0.0, 2.0 / (45.0 * PI<double>())}
                   : line_tail_derivatives(x);
        slope_[k] = derivatives[0];
        curvature_[k] = derivatives[1];
    }

    // Summed downward from the asymptotic form, with Neumaier's compensation.
    double sum = airy_line_tail_asymptotic(END);
    double compensation = 0.0;
    value_[nodes - 1] = sum;
    const auto slope = [](double x) { return line_tail_derivatives(x)[0]; };
    for (std::size_t k = nodes - 1; k-- > 0;) {
        const double x = static_cast<double>(k) * STEP;
        const double panel = -airy_band_detail::gauss_legendre_8(slope, x, x + STEP);
        const double next = sum + panel;
        compensation +=
            std::abs(sum) >= std::abs(panel) ? (sum - next) + panel : (panel - next) + sum;
        sum = next;
        value_[k] = sum + compensation;
    }
}

/// V(x), by quintic Hermite interpolation; beyond the table, its asymptotic form.
inline double AiryLineTail::operator()(double x) const
{
    if (x >= END) {
        return airy_line_tail_asymptotic(x);
    }
    const double u = x / STEP;
    const auto k = std::min(static_cast<std::size_t>(u), value_.size() - 2);
    const double t = u - static_cast<double>(k);
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double value0 = 1.0 - t3 * (10.0 - 15.0 * t + 6.0 * t2);
    const double first0 = t - t3 * (6.0 - 8.0 * t + 3.0 * t2);
    const double second0 = 0.5 * t2 * (1.0 - 3.0 * t + 3.0 * t2 - t3);
    const double first1 = t3 * (-4.0 + 7.0 * t - 3.0 * t2);
    const double second1 = 0.5 * t3 * (1.0 - 2.0 * t + t2);
    return value0 * value_[k] + (1.0 - value0) * value_[k + 1] +
           STEP * (first0 * slope_[k] + first1 * slope_[k + 1]) +
           STEP * STEP * (second0 * curvature_[k] + second1 * curvature_[k + 1]);
}

/**
 * @brief The line spread for a band whose ends have the given cutoff frequencies.
 *
 * @param cutoff_blue Cutoff frequency at the band's short-wavelength end; positive.
 * @param cutoff_red Cutoff frequency at its long-wavelength end; positive, at most cutoff_blue.
 */
inline AiryBandLine::AiryBandLine(double cutoff_blue, double cutoff_red)
    : blue_{cutoff_blue}, red_{cutoff_red}
{
}

/// Light per unit length across the line, at a distance d from it.
inline double AiryBandLine::operator()(double d) const
{
    const double pi = PI<double>();
    const double a_blue = 2.0 * pi * blue_ * d;
    const double a_red = 2.0 * pi * red_ * d;
    const auto h1_over_a2 = [](double a) {
        if (a < 1e-3) {
            return (2.0 / PI<double>()) * (1.0 / 3.0 - a * a / 45.0);
        }
        return struve_h01(a)[1] / (a * a);
    };
    if (blue_ == red_) {
        return 4.0 * blue_ * h1_over_a2(a_blue);
    }
    const double scale = 4.0 / (1.0 / red_ - 1.0 / blue_);
    const double width = a_blue - a_red;
    if (width > AiryLineTail::STEP) {
        const AiryLineTail& tail = AiryLineTail::instance();
        return scale * (tail(a_red) - tail(a_blue) + 2.0 / (3.0 * pi) * std::log(blue_ / red_));
    }
    // A short interval: H1(t) / t^3 directly, which near the line is (2 / (3 pi)) / t.
    if (a_blue <= 0.0) {
        return scale * 2.0 / (3.0 * pi) * std::log(blue_ / red_);
    }
    const auto integrand = [&](double t) { return h1_over_a2(t) / t; };
    return scale * airy_band_detail::gauss_legendre_8(integrand, a_red, a_blue);
}

namespace airy_line_detail {

/// h(a) = H1(a) / a^2 and its derivative (H0(a) - 3 H1(a) / a) / a^2; from H1's series near 0.
inline std::array<double, 2> struve_ratio(double a)
{
    const double pi = PI<double>();
    if (a < 0.05) {
        const double a2 = a * a;
        return {(2.0 / pi) * (1.0 / 3.0 - a2 / 45.0 + a2 * a2 / 1575.0),
                (2.0 / pi) * (-2.0 * a / 45.0 + 4.0 * a2 * a / 1575.0)};
    }
    const auto [h0, h1] = struve_h01(a);
    return {h1 / (a * a), (h0 - 3.0 * h1 / a) / (a * a)};
}

} // namespace airy_line_detail

/**
 * @brief The derivative of the line spread with distance, and its second derivative. With
 * h(a) = H1(a) / a^2, the first is 4 c h'(a) 2 pi c at one wavelength, and
 * 4 (h(a_blue) - h(a_red)) / ((1 / c_red - 1 / c_blue) d) over a band.
 */
inline std::array<double, 2> AiryBandLine::derivatives(double d) const
{
    using airy_line_detail::struve_ratio;
    const double pi = PI<double>();
    const double two_pi = 2.0 * pi;
    if (blue_ == red_) {
        const double rate = two_pi * blue_;
        const double a = rate * d;
        const auto [h, slope] = struve_ratio(a);
        // h''(a), from h' = (H0 - 3 H1 / a) / a^2 with H0' = 2 / pi - H1 and
        // H1' = H0 - H1 / a; near 0, from the series.
        double curvature = 0.0;
        if (a < 0.05) {
            curvature = (2.0 / pi) * (-2.0 / 45.0 + 12.0 * a * a / 1575.0);
        } else {
            const auto [h0, h1] = struve_h01(a);
            const double h1_prime = h0 - h1 / a;
            const double h0_prime = 2.0 / pi - h1;
            curvature =
                (h0_prime - 3.0 * h1_prime / a + 3.0 * h1 / (a * a)) / (a * a) - 2.0 * slope / a;
        }
        return {4.0 * blue_ * slope * rate, 4.0 * blue_ * curvature * rate * rate};
    }
    const double scale = 4.0 / (1.0 / red_ - 1.0 / blue_);
    const double a_blue = two_pi * blue_ * d;
    const double a_red = two_pi * red_ * d;
    if (a_blue < 0.05) {
        // From the series of h: the slope is odd in d and the second derivative even.
        const double b2 = blue_ * blue_ - red_ * red_;
        const double b4 = blue_ * blue_ * blue_ * blue_ - red_ * red_ * red_ * red_;
        const double pi2 = pi * pi;
        return {scale * (2.0 / pi) *
                    (-4.0 * pi2 * b2 * d / 45.0 + 16.0 * pi2 * pi2 * b4 * d * d * d / 1575.0),
                scale * (2.0 / pi) *
                    (-4.0 * pi2 * b2 / 45.0 + 48.0 * pi2 * pi2 * b4 * d * d / 1575.0)};
    }
    const auto [h_blue, slope_blue] = struve_ratio(a_blue);
    const auto [h_red, slope_red] = struve_ratio(a_red);
    const double difference = h_blue - h_red;
    return {scale * difference / d,
            scale * ((slope_blue * two_pi * blue_ - slope_red * two_pi * red_) / d -
                     difference / (d * d))};
}

/// The derivative of the line spread with distance.
inline double AiryBandLine::slope(double d) const
{
    return derivatives(d)[0];
}

} // namespace huira::detail
