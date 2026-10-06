#pragma once

// An independent reference for the optics' point spread function, used only by tests. It shares
// no code with huira, so a test that compares the renderer with it cannot pass because both make
// the same mistake.
//
// It provides Bessel functions of the first kind, the Airy pattern of a circular aperture
// averaged evenly over a band of wavelengths, and the exact light that a radially symmetric
// pattern puts into a rectangle such as a pixel. Lengths are in pixels. The optics enter only
// through the cutoff frequency c = p / (lambda N), in cycles per pixel, for pixel pitch p,
// wavelength lambda and f-number N: r pixels from the source, the pattern's argument is
// x = pi r c.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace reference {

constexpr double PI = 3.141592653589793238462643383279502884;

/// Integral of fn over [a, b] by eight-point Gauss-Legendre on `panels` equal panels.
template <class F>
double gauss_legendre(const F& fn, double a, double b, int panels)
{
    // The rule's nodes on [-1, 1] are +-NODES[i], each with weight WEIGHTS[i].
    constexpr std::array<double, 4> NODES{0.1834346424956498049394761,
                                          0.5255324099163289858177390,
                                          0.7966664774136267395915539,
                                          0.9602898564975362316835609};
    constexpr std::array<double, 4> WEIGHTS{0.3626837833783619829651504,
                                            0.3137066458778872873379622,
                                            0.2223810344533744705443560,
                                            0.1012285362903762591525314};
    const double width = (b - a) / panels;
    double sum = 0.0;
    for (int k = 0; k < panels; ++k) {
        const double mid = a + (k + 0.5) * width;
        double panel = 0.0;
        for (std::size_t i = 0; i < NODES.size(); ++i) {
            const double offset = 0.5 * width * NODES[i];
            panel += WEIGHTS[i] * (fn(mid - offset) + fn(mid + offset));
        }
        sum += 0.5 * width * panel;
    }
    return sum;
}

namespace detail {

/// J_n(x) from Bessel's integral, J_n(x) = (1/pi) int_0^pi cos(n t - x sin t) dt. Reflected onto
/// [0, 2 pi] the integrand is smooth and periodic, so the midpoint sum converges faster than any
/// power of its step, and with ceil(x) + 48 points only round-off remains. Its cost grows with x,
/// so it serves small x.
inline double bessel_integral(int n, double x)
{
    const int points = static_cast<int>(std::ceil(x)) + 48;
    double sum = 0.0;
    for (int k = 0; k < points; ++k) {
        const double t = (k + 0.5) * PI / points;
        sum += std::cos(n * t - x * std::sin(t));
    }
    return sum / points;
}

/// J_n(x) from Hankel's asymptotic expansion (DLMF 10.17.3), for large x:
/// J_n(x) = sqrt(2 / (pi x)) (P cos w - Q sin w) with w = x - (2n + 1) pi / 4. The series for P
/// and Q are summed until their terms fall below 1e-17; for x >= 25 and n <= 2 the terms are
/// still shrinking there, so what is left out is smaller still. cos w and sin w are formed from
/// cos x and sin x, so that rounding the phase adds no error at large x.
inline double bessel_hankel(int n, double x)
{
    const double mu = 4.0 * n * n;
    double p = 1.0;
    double q = 0.0;
    double term = 1.0;
    for (int k = 1; k < 100; ++k) {
        const double odd = 2.0 * k - 1.0;
        term *= (mu - odd * odd) / (8.0 * k * x);
        // a_k(n) / x^k joins P for even k and Q for odd k, with signs + + - - repeating.
        const double signed_term = k % 4 < 2 ? term : -term;
        if (k % 2 == 0) {
            p += signed_term;
        } else {
            q += signed_term;
        }
        if (std::abs(term) < 1e-17) {
            break;
        }
    }
    // cos and sin of (2n + 1) pi / 4 are +-sqrt(1/2), with signs repeating every four orders.
    constexpr std::array<double, 4> COS_SIGN{1.0, -1.0, -1.0, 1.0};
    constexpr std::array<double, 4> SIN_SIGN{1.0, 1.0, -1.0, -1.0};
    const double half = std::sqrt(0.5);
    const double cos_phase = half * COS_SIGN[static_cast<std::size_t>(n % 4)];
    const double sin_phase = half * SIN_SIGN[static_cast<std::size_t>(n % 4)];
    const double c = std::cos(x);
    const double s = std::sin(x);
    const double cos_w = c * cos_phase + s * sin_phase;
    const double sin_w = s * cos_phase - c * sin_phase;
    return std::sqrt(2.0 / (PI * x)) * (p * cos_w - q * sin_w);
}

} // namespace detail

/// Bessel function of the first kind J_n(x), for n = 0, 1 or 2 and x >= 0, within 1e-15 of
/// mpmath's values. It is Bessel's integral below x = 25 and Hankel's expansion above; the two
/// agree to 6e-15 where both apply.
inline double bessel_j(int n, double x)
{
    return x < 25.0 ? detail::bessel_integral(n, x) : detail::bessel_hankel(n, x);
}

/// The shape of the Airy pattern, (J1(x) / x)^2, which is 1/4 at x = 0.
inline double airy_shape(double x)
{
    // Below 0.01 the series avoids the cancellation in J1(x) / x; the terms it leaves out are
    // below 1e-16.
    if (x < 0.01) {
        const double x2 = x * x;
        const double j = 0.5 - x2 / 16.0 + x2 * x2 / 384.0;
        return j * j;
    }
    const double j = bessel_j(1, x) / x;
    return j * j;
}

/// T(x) = int_x^inf (J1(t) / t)^2 dt for large x, from the asymptotic forms of J1's modulus and
/// phase (DLMF 10.18.17 and 10.18.18) integrated by parts. Its error oscillates within
/// 0.36 / x^5: 4e-11 at x = 100, 3e-19 at x = 4096.
inline double airy_tail_asymptotic(double x)
{
    const double x2 = x * x;
    const double phase = 2.0 * x + 0.75 / x;
    return (0.5 + (3.0 / 32.0) / x2 - std::cos(phase) / (2.0 * x) - 0.75 * std::sin(phase) / x2) /
           (PI * x2);
}

/// T(x) = int_x^inf (J1(t) / t)^2 dt, which falls from 4 / (3 pi) at x = 0 to about
/// 1 / (2 pi x^2). The band-averaged Airy pattern is a difference of two of its values, so it is
/// kept to full relative precision at every x: tabulated with its first two derivatives every
/// 1/32 up to x = 4096, summed downward from the asymptotic form there, and read by quintic
/// Hermite interpolation, whose error is below 4e-15. Beyond 4096 the asymptotic form is used.
class AiryTail {
  public:
    static constexpr double STEP = 1.0 / 32.0;
    static constexpr double END = 4096.0;

    AiryTail()
    {
        const auto nodes = static_cast<std::size_t>(END / STEP) + 1;
        tail_.resize(nodes);
        shape_.resize(nodes);
        slope_.resize(nodes);
        shape_[0] = 0.25;
        slope_[0] = 0.0;
        for (std::size_t k = 1; k < nodes; ++k) {
            const double x = static_cast<double>(k) * STEP;
            const double j1 = bessel_j(1, x) / x;
            const double j2 = bessel_j(2, x) / x;
            shape_[k] = j1 * j1;
            // (J1(x) / x)' = -J2(x) / x
            slope_[k] = -2.0 * j1 * j2;
        }

        // Summed downward, so that the small values at large x keep their relative precision,
        // with Neumaier's compensation, so that the large values near x = 0 keep theirs too.
        double sum = airy_tail_asymptotic(END);
        double compensation = 0.0;
        tail_[nodes - 1] = sum;
        for (std::size_t k = nodes - 1; k-- > 0;) {
            const double x = static_cast<double>(k) * STEP;
            const double panel = gauss_legendre(airy_shape, x, x + STEP, 1);
            const double next = sum + panel;
            compensation +=
                std::abs(sum) >= std::abs(panel) ? (sum - next) + panel : (panel - next) + sum;
            sum = next;
            tail_[k] = sum + compensation;
        }
    }

    double operator()(double x) const
    {
        if (x >= END) {
            return airy_tail_asymptotic(x);
        }
        const double u = x / STEP;
        const auto k = std::min(static_cast<std::size_t>(u), tail_.size() - 2);
        const double t = u - static_cast<double>(k);
        const double t2 = t * t;
        const double t3 = t2 * t;
        // Quintic Hermite basis: weights of the value, first and second derivative at each end.
        const double value0 = 1.0 - t3 * (10.0 - 15.0 * t + 6.0 * t2);
        const double first0 = t - t3 * (6.0 - 8.0 * t + 3.0 * t2);
        const double second0 = 0.5 * t2 * (1.0 - 3.0 * t + 3.0 * t2 - t3);
        const double value1 = 1.0 - value0;
        const double first1 = t3 * (-4.0 + 7.0 * t - 3.0 * t2);
        const double second1 = 0.5 * t3 * (1.0 - 2.0 * t + t2);
        // T' = -(J1(x) / x)^2 and T'' is minus its slope.
        return value0 * tail_[k] + value1 * tail_[k + 1] -
               STEP * (first0 * shape_[k] + first1 * shape_[k + 1]) -
               STEP * STEP * (second0 * slope_[k] + second1 * slope_[k + 1]);
    }

  private:
    std::vector<double> tail_;
    std::vector<double> shape_;
    std::vector<double> slope_;
};

/// The tail table, built on first use (131,073 nodes, 3 MB).
inline const AiryTail& airy_tail()
{
    static const AiryTail table;
    return table;
}

/// The Airy pattern of a circular aperture averaged evenly over the wavelengths of a band, as
/// light per square pixel r pixels from the source, for a total of 1. The band runs between the
/// cutoff frequencies of its two ends, in cycles per pixel; equal cutoffs give one wavelength.
///
/// At one wavelength the pattern is pi c^2 (J1(x) / x)^2 with x = pi r c. Averaged evenly over
/// wavelength, which is 1/c in these units, it becomes pi c_blue c_red times the mean of
/// (J1(x) / x)^2 over x from pi r c_red to pi r c_blue, and that mean is a difference of the
/// tail T over the interval's width.
class AiryBand {
  public:
    AiryBand(double cutoff_blue, double cutoff_red) : blue_{cutoff_blue}, red_{cutoff_red} {}

    double operator()(double r) const
    {
        const double x_blue = PI * r * blue_;
        const double x_red = PI * r * red_;
        const double width = x_blue - x_red;
        double mean = 0.0;
        if (width > AiryTail::STEP) {
            const AiryTail& tail = airy_tail();
            mean = (tail(x_red) - tail(x_blue)) / width;
        } else if (width > 0.0) {
            // A short interval, near the source or in a narrow band, is integrated directly,
            // which stays exact as its width shrinks to nothing.
            mean = gauss_legendre(airy_shape, x_red, x_blue, 1) / width;
        } else {
            mean = airy_shape(x_blue);
        }
        return PI * blue_ * red_ * mean;
    }

    /// Light per square pixel at the source.
    double peak() const { return 0.25 * PI * blue_ * red_; }

    /// Light beyond r pixels from the source. At one wavelength it is J0(x)^2 + J1(x)^2
    /// (Rayleigh's encircled energy), averaged here over the band's wavelengths.
    double light_beyond(double r) const
    {
        const auto beyond = [r](double cutoff) {
            const double j0 = bessel_j(0, PI * r * cutoff);
            const double j1 = bessel_j(1, PI * r * cutoff);
            return j0 * j0 + j1 * j1;
        };
        if (blue_ == red_) {
            return beyond(blue_);
        }
        // Each panel spans at most a quarter of a period of the oscillation in x, which is
        // fastest in wavelength at the blue end.
        const int panels = 4 + static_cast<int>(std::ceil(4.0 * r * blue_ * (blue_ - red_) / red_));
        const double u_blue = 1.0 / blue_;
        const double u_red = 1.0 / red_;
        return gauss_legendre([&](double u) { return beyond(1.0 / u); }, u_blue, u_red, panels) /
               (u_red - u_blue);
    }

  private:
    double blue_;
    double red_;
};

/// Length of the arc of the circle of radius r about the origin that lies in x >= a, y >= b, for
/// a, b >= 0.
inline double corner_arc(double a, double b, double r)
{
    if (a * a + b * b >= r * r) {
        return 0.0;
    }
    return r * (std::acos(b / r) - std::asin(a / r));
}

namespace detail {

/// Integral of fn over [a, b] by eight-point Gauss-Legendre panels that start `first` wide at a
/// and grow by half each time, for an integrand with a singularity close to a, until they are
/// `widest` wide.
template <class F>
double graded_gauss_legendre(const F& fn, double a, double b, double first, double widest)
{
    double sum = 0.0;
    double start = a;
    double width = first;
    while (width < widest && start + width < b) {
        sum += gauss_legendre(fn, start, start + width, 1);
        start += width;
        width *= 1.5;
    }
    const int panels = std::max(1, static_cast<int>(std::ceil((b - start) / widest)));
    return sum + gauss_legendre(fn, start, b, panels);
}

/// rectangle_light() for a rectangle [a0, a1] x [b0, b1] in the first quadrant.
template <class Density>
double
quadrant_light(const Density& density, double a0, double a1, double b0, double b1, double max_step)
{
    if (a1 <= a0 || b1 <= b0) {
        return 0.0;
    }
    struct Corner {
        double a;
        double b;
        double radius;
    };
    std::array<Corner, 4> corners{{{a0, b0, std::hypot(a0, b0)},
                                   {a1, b0, std::hypot(a1, b0)},
                                   {a0, b1, std::hypot(a0, b1)},
                                   {a1, b1, std::hypot(a1, b1)}}};
    std::sort(corners.begin(), corners.end(), [](const Corner& p, const Corner& q) {
        return p.radius < q.radius;
    });
    const auto arc = [&](double r) {
        return corner_arc(a0, b0, r) - corner_arc(a1, b0, r) - corner_arc(a0, b1, r) +
               corner_arc(a1, b1, r);
    };
    // Points on the real line where the arcs in the rectangle are singular: each corner's arc,
    // r (acos(b / r) - asin(a / r)), has square-root branch points at r = a and r = b, at or below
    // the corner's radius, and a logarithmic one at r = 0.
    std::vector<double> singular{0.0};
    double light = 0.0;
    for (std::size_t k = 0; k + 1 < corners.size(); ++k) {
        // Between the radii of corners k and k + 1, the arcs of corners 0 to k are in the
        // rectangle, and all their singular points lie at or below this interval. The nearest
        // can lie just below it when the source is close to an edge. The substitution
        // r = branch + (hi - branch) t^2, centred on it, makes its square root smooth in t.
        singular.push_back(corners[k].a);
        singular.push_back(corners[k].b);
        const double lo = corners[k].radius;
        const double hi = corners[k + 1].radius;
        if (hi <= lo) {
            continue;
        }
        const double branch = *std::max_element(singular.begin(), singular.end());
        double next = 0.0;
        for (double point : singular) {
            if (point < branch) {
                next = std::max(next, point);
            }
        }
        const double span = hi - branch;
        const double t_lo = std::sqrt(std::max(0.0, lo - branch) / span);
        // The next singular point below sits at i sqrt((branch - next) / span) in t. Panels
        // that start at half its distance from t_lo, and grow from there, keep Gauss-Legendre
        // converging however close it is.
        const double gap = branch > 0.0 ? std::hypot(t_lo, std::sqrt((branch - next) / span)) : 1.0;
        // dr/dt is at most 2 span, which bounds the panels' length in r by max_step.
        const double steepest = 2.0 * span;
        const auto integrand = [&](double t) {
            const double r = branch + span * t * t;
            return density(r) * arc(r) * steepest * t;
        };
        light += graded_gauss_legendre(integrand, t_lo, 1.0, 0.5 * gap, max_step / steepest);
    }
    return light;
}

} // namespace detail

/// Light that a radially symmetric density puts into the rectangle [x0, x1] x [y0, y1], with the
/// source at the origin. density(r) is light per unit area r from the source.
///
/// The light is the integral over r of density(r) times the length of the circle of radius r
/// inside the rectangle, which is exact: that length is the derivative of the area the disc of
/// radius r shares with the rectangle. With the rectangle's parts in each quadrant reflected into
/// the first, the length is a sum of four corner arcs, each starting at its corner's radius, so
/// the integral is split at those radii. An arc has square-root branch points at the distances of
/// its corner's two edges from the source. When an edge passes through the source, one sits where
/// its piece of the integral starts, and when an edge passes close by, just short of it. Each
/// piece is therefore integrated in a variable that makes the nearest branch point smooth, on
/// Gauss-Legendre panels that start short near it and are at most max_step long in r, so that
/// they follow the pattern's rings. The Airy pattern's rings repeat every 1 / c pixels, and the
/// default of 0.02 px puts at least five panels in each down to c = 10.
template <class Density>
double rectangle_light(
    const Density& density, double x0, double x1, double y0, double y1, double max_step = 0.02)
{
    struct Span {
        double lo;
        double hi;
    };
    // The parts of [u0, u1] on each side of zero, reflected onto the positive side.
    const auto fold = [](double u0, double u1) {
        std::vector<Span> spans;
        if (u1 <= 0.0) {
            spans.push_back({-u1, -u0});
        } else if (u0 >= 0.0) {
            spans.push_back({u0, u1});
        } else {
            spans.push_back({0.0, -u0});
            spans.push_back({0.0, u1});
        }
        return spans;
    };
    double light = 0.0;
    for (const Span& x : fold(x0, x1)) {
        for (const Span& y : fold(y0, y1)) {
            light += detail::quadrant_light(density, x.lo, x.hi, y.lo, y.hi, max_step);
        }
    }
    return light;
}

/// Light that a radially symmetric density puts into the pixel whose centre is dx, dy pixels from
/// the source.
template <class Density>
double pixel_light(const Density& density, double dx, double dy, double max_step = 0.02)
{
    return rectangle_light(density, dx - 0.5, dx + 0.5, dy - 0.5, dy + 0.5, max_step);
}

} // namespace reference
