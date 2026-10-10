#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "huira/core/constants.hpp"
#include "huira/util/logger.hpp"
#include "tbb/parallel_for.h"

namespace huira::detail {

namespace psf_profile_detail {

/// Quintic Hermite interpolation between nodes h apart, from the value and first two derivatives
/// at each, at a fraction t of the way: the value and its first two derivatives.
inline std::array<double, 3>
quintic_hermite(double t, double h, const std::array<double, 3>& a, const std::array<double, 3>& b)
{
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double t4 = t3 * t;
    const double t5 = t4 * t;
    // Weights of a's value, slope and curvature, and b's, and their first two derivatives in t.
    const std::array<double, 6> value{1.0 - 10.0 * t3 + 15.0 * t4 - 6.0 * t5,
                                      t - 6.0 * t3 + 8.0 * t4 - 3.0 * t5,
                                      0.5 * (t2 - 3.0 * t3 + 3.0 * t4 - t5),
                                      10.0 * t3 - 15.0 * t4 + 6.0 * t5,
                                      -4.0 * t3 + 7.0 * t4 - 3.0 * t5,
                                      0.5 * (t3 - 2.0 * t4 + t5)};
    const std::array<double, 6> first{-30.0 * t2 + 60.0 * t3 - 30.0 * t4,
                                      1.0 - 18.0 * t2 + 32.0 * t3 - 15.0 * t4,
                                      0.5 * (2.0 * t - 9.0 * t2 + 12.0 * t3 - 5.0 * t4),
                                      30.0 * t2 - 60.0 * t3 + 30.0 * t4,
                                      -12.0 * t2 + 28.0 * t3 - 15.0 * t4,
                                      0.5 * (3.0 * t2 - 8.0 * t3 + 5.0 * t4)};
    const std::array<double, 6> second{-60.0 * t + 180.0 * t2 - 120.0 * t3,
                                       -36.0 * t + 96.0 * t2 - 60.0 * t3,
                                       0.5 * (2.0 - 18.0 * t + 36.0 * t2 - 20.0 * t3),
                                       60.0 * t - 180.0 * t2 + 120.0 * t3,
                                       -24.0 * t + 84.0 * t2 - 60.0 * t3,
                                       0.5 * (6.0 * t - 24.0 * t2 + 20.0 * t3)};
    const std::array<double, 6> data{a[0], h * a[1], h * h * a[2], b[0], h * b[1], h * h * b[2]};
    std::array<double, 3> result{};
    for (std::size_t i = 0; i < data.size(); ++i) {
        result[0] += value[i] * data[i];
        result[1] += first[i] * data[i];
        result[2] += second[i] * data[i];
    }
    result[1] /= h;
    result[2] /= h * h;
    return result;
}

} // namespace psf_profile_detail

/**
 * @brief Tabulates function(r), which returns the value and first two derivatives, every step
 * from start to end (one node more if they do not divide evenly).
 */
template <class Function>
RadialTable RadialTable::linear(const Function& function, double start, double end, double step)
{
    RadialTable table;
    table.start_ = start;
    table.spacing_ = step;
    const auto count = static_cast<std::size_t>(std::ceil((end - start) / step)) + 1;
    table.end_ = start + static_cast<double>(count - 1) * step;
    table.radii_.resize(count);
    table.nodes_.resize(count);
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t k) {
        table.radii_[k] = start + static_cast<double>(k) * step;
        table.nodes_[k] = function(table.radii_[k]);
    });
    return table;
}

/**
 * @brief Tabulates function(r), which returns the value and first two derivatives, at radii
 * growing by ratio from start to end; start must be positive.
 */
template <class Function>
RadialTable
RadialTable::logarithmic(const Function& function, double start, double end, double ratio)
{
    RadialTable table;
    table.logarithmic_ = true;
    table.start_ = start;
    table.spacing_ = std::log(ratio);
    const auto count =
        static_cast<std::size_t>(std::ceil(std::log(end / start) / table.spacing_)) + 2;
    table.radii_.resize(count);
    table.nodes_.resize(count);
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t k) {
        table.radii_[k] = start * std::exp(static_cast<double>(k) * table.spacing_);
        table.nodes_[k] = function(table.radii_[k]);
    });
    table.end_ = table.radii_.back();
    return table;
}

/**
 * @brief The function's value and first two derivatives at r.
 */
inline std::array<double, 3> RadialTable::operator()(double r) const
{
    if (r >= end_) {
        return tail_(r);
    }
    const double u = logarithmic_ ? std::log(std::max(r, start_) / start_) / spacing_
                                  : std::max(r - start_, 0.0) / spacing_;
    const auto k = std::min(static_cast<std::size_t>(u), nodes_.size() - 2);
    const double h = radii_[k + 1] - radii_[k];
    const double t = std::min((r - radii_[k]) / h, 1.0);
    return psf_profile_detail::quintic_hermite(t, h, nodes_[k], nodes_[k + 1]);
}

/**
 * @brief Beyond the last node: the power law r^-n that matches its value and slope there.
 */
inline std::array<double, 3> RadialTable::tail_(double r) const
{
    const std::array<double, 3>& last = nodes_.back();
    if (!(last[0] > 0.0) || !(last[1] < 0.0)) {
        return {0.0, 0.0, 0.0};
    }
    const double falloff = -end_ * last[1] / last[0];
    const double value = last[0] * std::pow(end_ / r, falloff);
    return {value, -falloff * value / r, falloff * (falloff + 1.0) * value / (r * r)};
}

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

/**
 * @brief The profile far out, with its first two derivatives: the diffraction pattern averaged
 * over its rings, kappa / r^3, and the scattered light.
 */
inline std::array<double, 3> OpticsProfile::far_field(double r) const
{
    const double kappa = (1.0 - scatter_fraction_) * diffraction_.far_field_coefficient();
    const double r2 = r * r;
    const double cube = kappa / (r2 * r);
    std::array<double, 3> result{cube, -3.0 * cube / r, 12.0 * cube / r2};
    if (scatter_fraction_ > 0.0) {
        const std::array<double, 3> scattered = scatter_->derivatives(r);
        for (std::size_t i = 0; i < result.size(); ++i) {
            result[i] += scatter_fraction_ * scattered[i];
        }
    }
    return result;
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

namespace psf_profile_detail {

/// Eight-point Gauss-Legendre on panels over [a, b], summing a function that returns several
/// values at once.
template <std::size_t N, class F>
std::array<double, N> gauss_legendre_panels(const F& fn, double a, double b, std::size_t panels)
{
    std::array<double, N> sum{};
    const double width = (b - a) / static_cast<double>(panels);
    for (std::size_t k = 0; k < panels; ++k) {
        const double mid = a + (static_cast<double>(k) + 0.5) * width;
        for (std::size_t i = 0; i < airy_band_detail::GL8_NODES.size(); ++i) {
            const double offset = 0.5 * width * airy_band_detail::GL8_NODES[i];
            const double weight = 0.5 * width * airy_band_detail::GL8_WEIGHTS[i];
            const std::array<double, N> below = fn(mid - offset);
            const std::array<double, N> above = fn(mid + offset);
            for (std::size_t j = 0; j < N; ++j) {
                sum[j] += weight * (below[j] + above[j]);
            }
        }
    }
    return sum;
}

} // namespace psf_profile_detail

/**
 * @brief The profile of focused, blurred by a disc of radius blur.
 *
 * @param focused The in-focus profile.
 * @param blur The blur disc's radius; positive.
 * @param focused_far_radius Where the in-focus profile's rings have washed out (see
 *        PsfTables::airy()); the rings are followed no further.
 * @param reach How far out the blurred profile is tabulated before it is continued as a power
 *        law.
 */
inline DefocusedProfile::DefocusedProfile(const OpticsProfile& focused,
                                          double blur,
                                          double focused_far_radius,
                                          double reach)
    : blur_{blur}
{
    using psf_profile_detail::gauss_legendre_panels;
    if (!(blur > 0.0) || !std::isfinite(blur)) {
        HUIRA_THROW_ERROR("DefocusedProfile - The blur must be positive and finite: " +
                          std::to_string(blur));
    }
    const double pi = PI<double>();
    const double blue = focused.diffraction().cutoff_blue();
    const double red = focused.diffraction().cutoff_red();
    const double period = 1.0 / blue;
    const double step = period / 8.0;

    // How far the rings matter. At the far radius they are 0.1% of the mean once averaged over
    // a pixel, which lets through at most A = 1 / (pi c_red) of them. Averaged over the disc
    // they keep 2 / (k b) sqrt(2 / (pi k b)) of their contrast (the disc's transform, a jinc, far
    // out), with k = 2 pi c_red; three times that, over A, scales the far radius. They are always
    // followed for PROJECTION_PERIODS of the coarsest detail, where the core's edge rings.
    constexpr double SAFETY = 3.0;
    constexpr double PROJECTION_PERIODS = 28.0;
    const double through = std::min(1.0, 1.0 / (pi * red));
    const double kb = 2.0 * pi * red * blur;
    const double kept = std::min(1.0, 2.0 / kb * std::sqrt(2.0 / (pi * kb)));
    ring_radius_ = std::clamp(focused_far_radius * std::min(1.0, SAFETY * kept / through),
                              std::min(PROJECTION_PERIODS / red, focused_far_radius),
                              focused_far_radius);

    // The in-focus profile and its first two derivatives: tabulated with its rings out to the
    // ring radius, and averaged over them beyond. The second derivative is a central difference
    // of the slope, a thousandth of a period wide.
    const double difference = 1e-3 * period;
    const auto slope = [&](double r) { return r < 0.0 ? -focused.slope(-r) : focused.slope(r); };
    const RadialTable rings = RadialTable::linear(
        [&](double r) {
            return std::array<double, 3>{focused(r),
                                         focused.slope(r),
                                         (slope(r + difference) - slope(r - difference)) /
                                             (2.0 * difference)};
        },
        0.0,
        ring_radius_ + 2.0 * step,
        step);
    const auto in_focus = [&](double rho) {
        return rho <= ring_radius_ ? rings(rho) : focused.far_field(rho);
    };

    // The light within rho of the source, E(rho), with E' = 2 pi rho P and
    // E'' = 2 pi (P + rho P'), summed over the nodes up to the disc's radius.
    std::vector<std::array<double, 3>> encircled_nodes;
    {
        const auto count = static_cast<std::size_t>(std::ceil(blur / step)) + 2;
        encircled_nodes.resize(count);
        double light = 0.0;
        for (std::size_t k = 0; k < count; ++k) {
            const double rho = static_cast<double>(k) * step;
            if (k > 0) {
                light += gauss_legendre_panels<1>(
                    [&](double x) { return std::array<double, 1>{2.0 * pi * x * in_focus(x)[0]}; },
                    rho - step,
                    rho,
                    1)[0];
            }
            const std::array<double, 3> p = in_focus(rho);
            encircled_nodes[k] = {light, 2.0 * pi * rho * p[0], 2.0 * pi * (p[0] + rho * p[1])};
        }
    }
    const auto encircled = [&](double rho) {
        const double u = rho / step;
        const auto k = std::min(static_cast<std::size_t>(u), encircled_nodes.size() - 2);
        return psf_profile_detail::quintic_hermite(
            u - static_cast<double>(k), step, encircled_nodes[k], encircled_nodes[k + 1])[0];
    };

    // The blurred value and first two derivatives at r: over the disc, the in-focus profile's
    // whole circles (those within blur - r of the source, whose light is E) and the arcs of the
    // others, of half-angle theta. On an arc, the value takes 2 rho theta P; the slope, the x
    // component of the gradient, 2 rho sin(theta) P'; and the second derivative along x,
    // rho (theta + sin cos) P'' + (theta - sin cos) P'. The arcs' lengths have square-root ends,
    // which rho = lo + (hi - lo) sin^2(pi s / 2) smooths; panels follow the rings out to the
    // ring radius, and are wider beyond.
    const double area = pi * blur * blur;
    const auto blurred = [&](double r, bool smooth) {
        std::array<double, 3> sum{};
        if (r < blur) {
            const double inside = blur - r;
            sum[0] += encircled(inside);
            sum[2] += pi * inside * in_focus(inside)[1];
        }
        const double lo = std::abs(r - blur);
        const double hi = r + blur;
        if (r > 1e-12 * blur && hi > lo) {
            const auto arc = [&](double s) {
                const double sine = std::sin(0.5 * pi * s);
                const double rho = lo + (hi - lo) * sine * sine;
                const double jacobian = 0.5 * pi * (hi - lo) * std::sin(pi * s);
                const double cosine_theta =
                    std::clamp((rho * rho + r * r - blur * blur) / (2.0 * rho * r), -1.0, 1.0);
                const double theta = std::acos(cosine_theta);
                const double sin_theta = std::sqrt(1.0 - cosine_theta * cosine_theta);
                const double sc = sin_theta * cosine_theta;
                const std::array<double, 3> p = smooth ? focused.far_field(rho) : in_focus(rho);
                return std::array<double, 3>{jacobian * 2.0 * rho * theta * p[0],
                                             jacobian * 2.0 * rho * sin_theta * p[1],
                                             jacobian *
                                                 (rho * (theta + sc) * p[2] + (theta - sc) * p[1])};
            };
            const double steepest = 0.5 * pi * (hi - lo);
            double split = 1.0;
            if (!smooth && ring_radius_ < hi) {
                split = ring_radius_ <= lo
                            ? 0.0
                            : 2.0 / pi * std::asin(std::sqrt((ring_radius_ - lo) / (hi - lo)));
            }
            const double coarse_width = std::max(0.5 * period, 0.1 * std::max(lo, ring_radius_));
            if (split > 0.0) {
                const auto panels = static_cast<std::size_t>(
                    std::max(2.0, std::ceil(steepest * split / (0.5 * period))));
                const std::array<double, 3> part =
                    gauss_legendre_panels<3>(arc, 0.0, split, panels);
                for (std::size_t i = 0; i < 3; ++i) {
                    sum[i] += part[i];
                }
            }
            if (split < 1.0) {
                const auto panels = static_cast<std::size_t>(
                    std::max(4.0, std::ceil(steepest * (1.0 - split) / coarse_width)));
                const std::array<double, 3> part =
                    gauss_legendre_panels<3>(arc, split, 1.0, panels);
                for (std::size_t i = 0; i < 3; ++i) {
                    sum[i] += part[i];
                }
            }
        }
        return std::array<double, 3>{sum[0] / area, sum[1] / area, sum[2] / area};
    };

    // Beyond the disc's edge plus the ring radius every arc lies where the rings are averaged,
    // so the blurred profile is smooth there, and the profile is its far field. The far field,
    // the disc over the in-focus profile averaged over its rings, is tabulated from FAR_START
    // beyond the disc's edge, where the in-focus far field holds for every arc, so that a
    // faint source can be drawn from it where the rings do not show (see
    // PsfTables::ring_deviation()).
    constexpr double OVERLAP = 4.0;
    const double rings_end = blur + ring_radius_;
    near_ = RadialTable::linear(
        [&](double r) { return blurred(r, false); }, 0.0, rings_end + OVERLAP, step);
    constexpr double FAR_RATIO = 1.01;
    far_ = RadialTable::logarithmic([&](double r) { return blurred(r, true); },
                                    smooth_from(),
                                    std::max(reach, 2.0 * rings_end),
                                    FAR_RATIO);
}

/// Light per unit area at a distance r from the source.
inline double DefocusedProfile::operator()(double r) const
{
    return r < near_.end() ? near_(r)[0] : far_(r)[0];
}

/// The derivative of the profile with distance.
inline double DefocusedProfile::slope(double r) const
{
    return r < near_.end() ? near_(r)[1] : far_(r)[1];
}

/**
 * @brief The blurred profile averaged over its rings, from smooth_from() out, with its first two
 * derivatives: the profile itself beyond the disc's edge plus ring_radius().
 */
inline std::array<double, 3> DefocusedProfile::far_field(double r) const
{
    return far_(r);
}

} // namespace huira::detail
