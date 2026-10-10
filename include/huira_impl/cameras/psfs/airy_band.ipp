#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "huira/core/constants.hpp"

namespace huira::detail {

namespace airy_band_detail {

/// Eight-point Gauss-Legendre rule on [-1, 1]: nodes +-NODES[i], each with weight WEIGHTS[i].
inline constexpr std::array<double, 4> GL8_NODES{0.1834346424956498049394761,
                                                 0.5255324099163289858177390,
                                                 0.7966664774136267395915539,
                                                 0.9602898564975362316835609};
inline constexpr std::array<double, 4> GL8_WEIGHTS{0.3626837833783619829651504,
                                                   0.3137066458778872873379622,
                                                   0.2223810344533744705443560,
                                                   0.1012285362903762591525314};

/// Integral of fn over [a, b] by one eight-point Gauss-Legendre panel.
template <class F>
double gauss_legendre_8(const F& fn, double a, double b)
{
    const double mid = 0.5 * (a + b);
    const double half = 0.5 * (b - a);
    double sum = 0.0;
    for (std::size_t i = 0; i < GL8_NODES.size(); ++i) {
        const double offset = half * GL8_NODES[i];
        sum += GL8_WEIGHTS[i] * (fn(mid - offset) + fn(mid + offset));
    }
    return half * sum;
}

/// Below this, Bessel functions come from their power series.
inline constexpr double SERIES_LIMIT = 0.01;

/// From this on, Bessel functions come from Hankel's asymptotic expansion.
inline constexpr double ASYMPTOTIC_LIMIT = 20.0;

/// J0, J1 and J2 by Miller's backward recurrence, for SERIES_LIMIT <= x < ASYMPTOTIC_LIMIT.
///
/// The recurrence J_{k-1} = (2k / x) J_k - J_{k+1} is stable downward. Started well above x
/// from arbitrary values, it settles on the J_k up to a common factor, which the identity
/// J0 + 2 (J2 + J4 + ...) = 1 fixes.
inline std::array<double, 3> bessel_miller(double x)
{
    const int start = 2 * static_cast<int>((x + 30.0 + 4.0 * std::cbrt(x)) / 2.0);
    double above = 0.0;   // J_{k+1}
    double current = 1.0; // J_k, for k = start
    double sum = 0.0;     // 2 (J2 + J4 + ...) so far
    std::array<double, 3> low{};
    for (int k = start; k > 0; --k) {
        const double below = 2.0 * k / x * current - above;
        above = current;
        current = below;
        const int index = k - 1;
        if (index <= 2) {
            low[static_cast<std::size_t>(index)] = current;
        }
        if (index > 0 && index % 2 == 0) {
            sum += 2.0 * current;
        }
        // The values grow downward; keep them in range.
        if (std::abs(current) > 1e200) {
            current *= 1e-200;
            above *= 1e-200;
            sum *= 1e-200;
            for (double& value : low) {
                value *= 1e-200;
            }
        }
    }
    const double norm = sum + current;
    return {low[0] / norm, low[1] / norm, low[2] / norm};
}

/// The sums P and Q of Hankel's asymptotic expansion of J_n and Y_n (DLMF 10.17.3 and 10.17.4),
/// for x >= ASYMPTOTIC_LIMIT, where their terms fall below 1e-17 before they start to grow.
inline std::array<double, 2> hankel_pq(int n, double x)
{
    const double mu = 4.0 * n * n;
    double p = 1.0;
    double q = 0.0;
    double term = 1.0;
    for (int k = 1; k < 64; ++k) {
        const double odd = 2.0 * k - 1.0;
        term *= (mu - odd * odd) / (8.0 * k * x);
        // Terms join P (even k) and Q (odd k) with signs + + - - in turn.
        const double signed_term = (k % 4 == 0 || k % 4 == 1) ? term : -term;
        if (k % 2 == 0) {
            p += signed_term;
        } else {
            q += signed_term;
        }
        if (std::abs(term) < 1e-17) {
            break;
        }
    }
    return {p, q};
}

/// cos and sin of the phase x - (2n + 1) pi / 4 for n = 0, 1, 2, from those of x, so that
/// rounding a large phase adds no error.
inline std::array<double, 2> hankel_phase(int n, double cos_x, double sin_x)
{
    const double half_root = std::sqrt(0.5);
    double cos_phase = 0.0;
    double sin_phase = 0.0;
    switch (n) {
    case 0:
        cos_phase = half_root * (cos_x + sin_x);
        sin_phase = half_root * (sin_x - cos_x);
        break;
    case 1:
        cos_phase = half_root * (sin_x - cos_x);
        sin_phase = -half_root * (sin_x + cos_x);
        break;
    default:
        cos_phase = -half_root * (cos_x + sin_x);
        sin_phase = half_root * (cos_x - sin_x);
        break;
    }
    return {cos_phase, sin_phase};
}

/// J_n(x) for n = 0, 1, 2 from Hankel's asymptotic expansion, for x >= ASYMPTOTIC_LIMIT.
inline double bessel_hankel(int n, double x, double cos_x, double sin_x)
{
    const auto [p, q] = hankel_pq(n, x);
    const auto [cos_phase, sin_phase] = hankel_phase(n, cos_x, sin_x);
    return std::sqrt(2.0 / (PI<double>() * x)) * (p * cos_phase - q * sin_phase);
}

/// Y_n(x) for n = 0, 1 from Hankel's asymptotic expansion, for x >= ASYMPTOTIC_LIMIT.
inline double bessel_y_hankel(int n, double x, double cos_x, double sin_x)
{
    const auto [p, q] = hankel_pq(n, x);
    const auto [cos_phase, sin_phase] = hankel_phase(n, cos_x, sin_x);
    return std::sqrt(2.0 / (PI<double>() * x)) * (p * sin_phase + q * cos_phase);
}

} // namespace airy_band_detail

/**
 * @brief Bessel functions of the first kind J0, J1 and J2 at x >= 0, to about 1e-15.
 *
 * From their power series below 0.01, Miller's backward recurrence below 20 and Hankel's
 * asymptotic expansion above.
 */
inline std::array<double, 3> bessel_j012(double x)
{
    using namespace airy_band_detail;
    if (x < SERIES_LIMIT) {
        const double x2 = x * x;
        return {1.0 - 0.25 * x2 + x2 * x2 / 64.0,
                0.5 * x * (1.0 - 0.125 * x2 + x2 * x2 / 192.0),
                0.125 * x2 * (1.0 - x2 / 12.0 + x2 * x2 / 384.0)};
    }
    if (x < ASYMPTOTIC_LIMIT) {
        return bessel_miller(x);
    }
    const double cos_x = std::cos(x);
    const double sin_x = std::sin(x);
    return {bessel_hankel(0, x, cos_x, sin_x),
            bessel_hankel(1, x, cos_x, sin_x),
            bessel_hankel(2, x, cos_x, sin_x)};
}

/**
 * @brief The shape of the Airy pattern, (J1(x) / x)^2, which is 1/4 at x = 0.
 */
inline double airy_shape(double x)
{
    using namespace airy_band_detail;
    if (x < SERIES_LIMIT) {
        const double x2 = x * x;
        const double j = 0.5 - x2 / 16.0 + x2 * x2 / 384.0;
        return j * j;
    }
    // Far out only J1 is needed, which saves two of the three asymptotic series.
    const double j1 =
        x < ASYMPTOTIC_LIMIT ? bessel_miller(x)[1] : bessel_hankel(1, x, std::cos(x), std::sin(x));
    const double j = j1 / x;
    return j * j;
}

/**
 * @brief The derivative of airy_shape(): -2 J1(x) J2(x) / x^2, since (J1(x) / x)' = -J2(x) / x.
 */
inline double airy_shape_slope(double x)
{
    if (x < airy_band_detail::SERIES_LIMIT) {
        // -2 (x/2 - x^3/16) (x^2/8 - x^4/96) / x^2, to the order of airy_shape()'s series
        return -x / 8.0 + 5.0 * x * x * x / 192.0;
    }
    const std::array<double, 3> j = bessel_j012(x);
    return -2.0 * j[1] * j[2] / (x * x);
}

/**
 * @brief T(x) for large x, from the asymptotic forms of J1's modulus and phase (DLMF 10.18.17
 * and 10.18.18) integrated by parts. Its error oscillates within 0.36 / x^5, 1e-17 at x = 2048.
 */
inline double airy_tail_asymptotic(double x)
{
    const double x2 = x * x;
    const double phase = 2.0 * x + 0.75 / x;
    return (0.5 + (3.0 / 32.0) / x2 - std::cos(phase) / (2.0 * x) - 0.75 * std::sin(phase) / x2) /
           (PI<double>() * x2);
}

/// The table, built the first time it is needed.
inline const AiryTail& AiryTail::instance()
{
    static const AiryTail table;
    return table;
}

inline AiryTail::AiryTail()
{
    const auto nodes = static_cast<std::size_t>(END / STEP) + 1;
    tail_.resize(nodes);
    shape_.resize(nodes);
    slope_.resize(nodes);
    for (std::size_t k = 0; k < nodes; ++k) {
        const double x = static_cast<double>(k) * STEP;
        shape_[k] = airy_shape(x);
        slope_[k] = airy_shape_slope(x);
    }

    // Summed downward, so that the small values far out keep their relative precision, with
    // Neumaier's compensation, so that the large values near 0 keep theirs too.
    double sum = airy_tail_asymptotic(END);
    double compensation = 0.0;
    tail_[nodes - 1] = sum;
    for (std::size_t k = nodes - 1; k-- > 0;) {
        const double x = static_cast<double>(k) * STEP;
        const double panel = airy_band_detail::gauss_legendre_8(airy_shape, x, x + STEP);
        const double next = sum + panel;
        compensation +=
            std::abs(sum) >= std::abs(panel) ? (sum - next) + panel : (panel - next) + sum;
        sum = next;
        tail_[k] = sum + compensation;
    }
}

/**
 * @brief T(x), by quintic Hermite interpolation from the table's values and first two
 * derivatives, T' = -(J1(x) / x)^2 and T'' its slope; beyond the table, its asymptotic form.
 */
inline double AiryTail::operator()(double x) const
{
    if (x >= END) {
        return airy_tail_asymptotic(x);
    }
    const double u = x / STEP;
    const auto k = std::min(static_cast<std::size_t>(u), tail_.size() - 2);
    const double t = u - static_cast<double>(k);
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double value0 = 1.0 - t3 * (10.0 - 15.0 * t + 6.0 * t2);
    const double first0 = t - t3 * (6.0 - 8.0 * t + 3.0 * t2);
    const double second0 = 0.5 * t2 * (1.0 - 3.0 * t + 3.0 * t2 - t3);
    const double first1 = t3 * (-4.0 + 7.0 * t - 3.0 * t2);
    const double second1 = 0.5 * t3 * (1.0 - 2.0 * t + t2);
    return value0 * tail_[k] + (1.0 - value0) * tail_[k + 1] -
           STEP * (first0 * shape_[k] + first1 * shape_[k + 1]) -
           STEP * STEP * (second0 * slope_[k] + second1 * slope_[k + 1]);
}

/**
 * @brief The pattern for a band whose ends have the given cutoff frequencies.
 *
 * @param cutoff_blue Cutoff frequency at the band's short-wavelength end; positive.
 * @param cutoff_red Cutoff frequency at its long-wavelength end; positive, at most cutoff_blue.
 */
inline AiryBandProfile::AiryBandProfile(double cutoff_blue, double cutoff_red)
    : blue_{cutoff_blue}, red_{cutoff_red}
{
}

/**
 * @brief Light per unit area at a distance r from the source.
 */
inline double AiryBandProfile::operator()(double r) const
{
    const double x_blue = PI<double>() * r * blue_;
    const double x_red = PI<double>() * r * red_;
    const double width = x_blue - x_red;
    double mean = 0.0;
    if (width > AiryTail::STEP) {
        const AiryTail& tail = AiryTail::instance();
        mean = (tail(x_red) - tail(x_blue)) / width;
    } else if (width > 0.0) {
        // Near the source, or for a narrow band, the interval is short: integrating directly
        // stays exact as it shrinks, where a difference of T would lose digits.
        mean = airy_band_detail::gauss_legendre_8(airy_shape, x_red, x_blue) / width;
    } else {
        mean = airy_shape(x_blue);
    }
    return PI<double>() * blue_ * red_ * mean;
}

/**
 * @brief The derivative of the pattern with distance from the source.
 */
inline double AiryBandProfile::slope(double r) const
{
    const double x_blue = PI<double>() * r * blue_;
    const double x_red = PI<double>() * r * red_;
    const double width = x_blue - x_red;
    double mean_slope = 0.0;
    if (width > AiryTail::STEP) {
        // d/dr of (T(x_red) - T(x_blue)) / width, with T' = -airy_shape and width = pi r dc
        const AiryTail& tail = AiryTail::instance();
        const double mean = (tail(x_red) - tail(x_blue)) / width;
        mean_slope =
            (blue_ * airy_shape(x_blue) - red_ * airy_shape(x_red)) / (r * (blue_ - red_)) -
            mean / r;
    } else {
        // The mean over the interval of airy_shape at x_red + s (x_blue - x_red), s in [0, 1],
        // differentiated under the integral: each point moves as pi (c_red + s dc).
        const auto integrand = [&](double s) {
            const double cutoff = red_ + s * (blue_ - red_);
            return airy_shape_slope(PI<double>() * r * cutoff) * PI<double>() * cutoff;
        };
        mean_slope = airy_band_detail::gauss_legendre_8(integrand, 0.0, 1.0);
    }
    return PI<double>() * blue_ * red_ * mean_slope;
}

/**
 * @brief The far field averaged over the rings: lambda N / (pi^3 r^3) at one wavelength, in
 * these units 1 / (pi^3 c r^3), averaged evenly over the band's wavelengths.
 */
inline double AiryBandProfile::far_field_coefficient() const
{
    const double pi = PI<double>();
    return 0.5 * (1.0 / blue_ + 1.0 / red_) / (pi * pi * pi);
}

} // namespace huira::detail
