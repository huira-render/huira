#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

#include "huira/core/constants.hpp"
#include "tbb/parallel_for.h"

namespace huira {

namespace grid_psf_detail {

constexpr std::array<double, 4> GL4_NODES{
    -0.8611363115940526, -0.3399810435848563, 0.3399810435848563, 0.8611363115940526};
constexpr std::array<double, 4> GL4_WEIGHTS{
    0.3478548451374538, 0.6521451548625461, 0.6521451548625461, 0.3478548451374538};
constexpr std::array<double, 3> GL3_NODES{-0.7745966692414834, 0.0, 0.7745966692414834};
constexpr std::array<double, 3> GL3_WEIGHTS{
    0.5555555555555556, 0.8888888888888888, 0.5555555555555556};

/// The area of the rectangle [x0, x1] x [y0, y1] within the circle of radius r about the origin:
/// across each row, the rectangle's width within the circle's chord, integrated exactly between
/// the heights where either edge meets the circle, with the circle's half chord s(y) and its
/// integral (y s(y) + r^2 asin(y / r)) / 2.
inline double circle_rectangle_area(double x0, double x1, double y0, double y1, double r)
{
    y0 = std::max(y0, -r);
    y1 = std::min(y1, r);
    if (!(x1 > x0) || !(y1 > y0)) {
        return 0.0;
    }
    const double r2 = r * r;
    const auto half_chord = [&](double y) { return std::sqrt(std::max(0.0, r2 - y * y)); };
    const auto integral = [&](double y) {
        const double t = std::clamp(y / r, -1.0, 1.0);
        return 0.5 * (y * half_chord(y) + r2 * std::asin(t));
    };
    std::array<double, 8> breaks{};
    std::size_t count = 0;
    breaks[count++] = y0;
    breaks[count++] = y1;
    // Where either side meets the circle, or just touches it (at a height of 0, which splits
    // the piece the width is tested at the middle of, where the chord's and the side's widths
    // would otherwise compare equal):
    for (const double x : {x0, x1}) {
        if (std::abs(x) <= r) {
            const double y = std::sqrt(std::max(0.0, r2 - x * x));
            for (const double b : {-y, y}) {
                if (b > y0 && b < y1) {
                    breaks[count++] = b;
                }
            }
        }
    }
    std::sort(breaks.begin(), breaks.begin() + static_cast<std::ptrdiff_t>(count));
    double area = 0.0;
    for (std::size_t k = 0; k + 1 < count; ++k) {
        const double a = breaks[k];
        const double b = breaks[k + 1];
        if (!(b > a)) {
            continue;
        }
        // On each piece, each edge of the width is either the rectangle's or the chord's:
        const double s = half_chord(0.5 * (a + b));
        const bool right_is_chord = s < x1;
        const bool left_is_chord = -s > x0;
        if ((right_is_chord ? s : x1) <= (left_is_chord ? -s : x0)) {
            continue;
        }
        const double chord = integral(b) - integral(a);
        area += (right_is_chord ? chord : x1 * (b - a)) - (left_is_chord ? -chord : x0 * (b - a));
    }
    return area;
}

} // namespace grid_psf_detail

/**
 * @brief The defocus blur alone, for a camera without a PSF: a uniform disc of radius blur pixel
 * widths, the aperture's image out of focus, or in focus (a blur of 0) a point, which puts all of
 * its light in the pixel it falls in. Either way each pixel's light is the disc's area within
 * it, as a share of the whole, which over the pixels sums to all of the light wherever the
 * source is, so scale() is 1.
 *
 * A disc under EXACT_DISC, or a point, is drawn exactly where it is drawn: still, from the
 * disc's area within each pixel, and moving, from that integrated along the line, by
 * Gauss-Legendre quadrature on four points between the places the area bends sharply, within
 * about 6e-5 of the source's light (see exact_line_light_()). A larger disc is put on a grid of
 * pixel-integrated samples every half or quarter SAMPLE_STEP (or as finely as MEMORY_BUDGET
 * allows), each the disc's area within a pixel centered there, exactly.
 *
 * @param blur The disc's radius, in pixel widths; 0 for a point, as is any under 1e-6.
 * @param aspect The pixel's height over its width.
 * @throws std::runtime_error if the blur is negative or not finite, or the aspect is not
 *         positive and finite.
 */
template <IsSpectral TSpectral>
GridPsf<TSpectral> GridPsf<TSpectral>::disc(double blur, double aspect)
{
    if (!(blur >= 0.0) || !std::isfinite(blur) || !(aspect > 0.0) || !std::isfinite(aspect)) {
        HUIRA_THROW_ERROR("GridPsf::disc - The blur must be non-negative and finite, and the "
                          "aspect positive and finite: " +
                          std::to_string(blur) + ", " + std::to_string(aspect));
    }
    // A disc this small lights a pixel differently from a point only for a source within its
    // radius of the pixel's edge, and its area, the square of its radius, would lose precision:
    constexpr double SMALLEST = 1e-6;
    if (blur < SMALLEST) {
        blur = 0.0;
    }
    GridPsf result;
    result.aspect_ = aspect;
    result.pixel_integrated_ = true;
    result.sums_to_one_ = true;
    // A pixel meets the disc out to its radius and half a pixel more, in pixels along each axis:
    const double half_x = blur + 0.5;
    const double half_y = blur / aspect + 0.5;
    if (blur < EXACT_DISC) {
        result.exact_blur_ = blur;
        result.step_ = 0.0;
        result.left_ = -half_x;
        result.right_ = half_x;
        result.top_ = -half_y;
        result.bottom_ = half_y;
        result.reach_ = std::hypot(half_x, aspect * half_y);
        // A pixel holds at most all of the light, and all of it is within reach:
        Light all;
        all.fill(1.0);
        result.envelope_.assign(
            static_cast<std::size_t>(std::floor(result.reach_ * ENVELOPE_BINS)) + 2, all);
        result.radius_90_.fill(result.reach_);
        result.build_line_envelope_();
        return result;
    }
    const double bytes = static_cast<double>(N * sizeof(double) * 2);
    const double most = std::sqrt(static_cast<double>(MEMORY_BUDGET) / bytes) - 2.0 * MARGIN - 1.0;
    // Where a pixel's corners cross the disc's edge, the area bends sharply; a finer step than
    // a PSF's keeps the spline within about 1e-4 of the peak there:
    const double finest = blur < 1.0 ? SAMPLE_STEP / 4.0 : SAMPLE_STEP / 2.0;
    const double step = std::max(finest, 2.0 * std::max(half_x, half_y) / std::max(most, 4.0));
    const int count_x = 2 * static_cast<int>(std::ceil(half_x / step)) + 1;
    const int count_y = 2 * static_cast<int>(std::ceil(half_y / step)) + 1;
    result.step_ = step;
    result.left_ = -static_cast<double>(count_x / 2) * step;
    result.top_ = -static_cast<double>(count_y / 2) * step;
    // In heights scaled by the aspect, the disc is round:
    const double area = PI<double>() * blur * blur;
    std::vector<double> samples(static_cast<std::size_t>(count_x) *
                                static_cast<std::size_t>(count_y) * N);
    tbb::parallel_for(0, count_y, [&](int j) {
        const double y = result.top_ + static_cast<double>(j) * step;
        for (int i = 0; i < count_x; ++i) {
            const double x = result.left_ + static_cast<double>(i) * step;
            const double inside =
                grid_psf_detail::circle_rectangle_area(
                    x - 0.5, x + 0.5, aspect * (y - 0.5), aspect * (y + 0.5), blur) /
                area;
            double* out =
                &samples[(static_cast<std::size_t>(j) * static_cast<std::size_t>(count_x) +
                          static_cast<std::size_t>(i)) *
                         N];
            std::fill(out, out + N, inside);
        }
    });
    result.build_from_samples_(samples, count_x, count_y);
    return result;
}

/**
 * @brief For a disc drawn exactly (see disc()), the light a pixel whose center is (dx, dy) pixels
 * from the source receives, as a share of the source's: the disc's area within the pixel, or for
 * a point, all of it if it falls in the pixel.
 */
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::exact_light_(double dx, double dy) const
{
    const double blur = exact_blur_;
    Light light;
    if (blur == 0.0) {
        // Pixel (x, y) covers [x, x + 1) by [y, y + 1):
        light.fill(dx > -0.5 && dx <= 0.5 && dy > -0.5 && dy <= 0.5 ? 1.0 : 0.0);
        return light;
    }
    // In pixel widths, a pixel being aspect widths high, where the disc is round:
    light.fill(grid_psf_detail::circle_rectangle_area(
                   dx - 0.5, dx + 0.5, aspect_ * (dy - 0.5), aspect_ * (dy + 0.5), blur) /
               (PI<double>() * blur * blur));
    return light;
}

/**
 * @brief For a disc drawn exactly (see disc()), its light along a piece of a path from its start
 * in the direction (ex, ey) for length pixels, integrated along it, that a pixel whose center is
 * (qx, qy) from the start receives (see draw_line_direct()).
 *
 * The disc's area within the pixel bends sharply where the disc's edge touches one of the
 * pixel's edges or passes through one of its corners: where the disc's center, in pixel widths,
 * is its radius from an edge, either side, or from a corner. Between those places along the
 * piece, the area is smooth, and Gauss-Legendre quadrature on four points integrates it to within
 * about 6e-5 of the source's light. A point's light is all or nothing between the places it
 * crosses the pixel's edges.
 */
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::exact_line_light_(
    double qx, double qy, double ex, double ey, double length) const
{
    Light light{};
    const auto [from, to] = clip_(qx, qy, ex, ey, length).range;
    if (!(to > from)) {
        return light;
    }
    const double blur = exact_blur_;
    const double half_x = 0.5;
    const double half_y = 0.5 * aspect_;
    // The disc's center relative to the pixel's, in pixel widths, at v along the piece, is
    // v d - c:
    const double d_x = ex;
    const double d_y = aspect_ * ey;
    const double c_x = qx;
    const double c_y = aspect_ * qy;
    std::array<double, 18> breaks{};
    std::size_t count = 0;
    breaks[count++] = from;
    breaks[count++] = to;
    const auto add = [&](double v) {
        if (v > from && v < to) {
            breaks[count++] = v;
        }
    };
    for (const double edge : {-half_x - blur, -half_x + blur, half_x - blur, half_x + blur}) {
        if (d_x != 0.0) {
            add((c_x + edge) / d_x);
        }
    }
    for (const double edge : {-half_y - blur, -half_y + blur, half_y - blur, half_y + blur}) {
        if (d_y != 0.0) {
            add((c_y + edge) / d_y);
        }
    }
    if (blur > 0.0) {
        // Where the center is the radius from a corner, |v d - (c + corner)| = blur:
        const double d2 = d_x * d_x + d_y * d_y;
        for (const double corner_x : {-half_x, half_x}) {
            for (const double corner_y : {-half_y, half_y}) {
                const double px = c_x + corner_x;
                const double py = c_y + corner_y;
                const double b = d_x * px + d_y * py;
                const double discriminant = b * b - d2 * (px * px + py * py - blur * blur);
                if (discriminant > 0.0) {
                    const double root = std::sqrt(discriminant);
                    add((b - root) / d2);
                    add((b + root) / d2);
                }
            }
        }
    }
    std::sort(breaks.begin(), breaks.begin() + static_cast<std::ptrdiff_t>(count));

    double inside = 0.0;
    double area = 0.0;
    for (std::size_t k = 0; k + 1 < count; ++k) {
        const double a = breaks[k];
        const double b = breaks[k + 1];
        if (!(b > a)) {
            continue;
        }
        const double mid = 0.5 * (a + b);
        const double half = 0.5 * (b - a);
        // Wholly within the pixel, which is most of a piece longer than the pixel. A point is in
        // the pixel from its lower edges up to, not including, its upper ones, as when still
        // (see exact_light_()), so that one moving exactly along an edge is in the pixels on one
        // side of it, not both:
        const double x = mid * d_x - c_x;
        const double y = mid * d_y - c_y;
        if (blur == 0.0) {
            if (x >= -half_x && x < half_x && y >= -half_y && y < half_y) {
                inside += b - a;
            }
            continue;
        }
        if (std::abs(x) <= half_x - blur && std::abs(y) <= half_y - blur) {
            inside += b - a;
            continue;
        }
        for (std::size_t i = 0; i < grid_psf_detail::GL4_NODES.size(); ++i) {
            const double v = mid + half * grid_psf_detail::GL4_NODES[i];
            const double dx = qx - v * ex;
            const double dy = aspect_ * (qy - v * ey);
            area += half * grid_psf_detail::GL4_WEIGHTS[i] *
                    grid_psf_detail::circle_rectangle_area(
                        dx - half_x, dx + half_x, dy - half_y, dy + half_y, blur);
        }
    }
    light.fill(blur > 0.0 ? inside + area / (PI<double>() * blur * blur) : inside);
    return light;
}

/**
 * @brief The PSF as pixels collect it, which lines are drawn from: itself if pixel-integrated,
 * and otherwise integrated over a pixel at each point (see integrated_()), made the first time
 * it is needed.
 */
template <IsSpectral TSpectral>
const GridPsf<TSpectral>& GridPsf<TSpectral>::collected_() const
{
    if (pixel_integrated_) {
        return *this;
    }
    std::call_once(line_cache_->collected_once, [this] {
        line_cache_->integrated = std::make_shared<const GridPsf>(integrated_(*this));
    });
    return *line_cache_->integrated;
}

/**
 * @brief The PSF as pixels collect it, as lines are drawn from it: itself if pixel-integrated,
 * and otherwise integrated over a pixel at each point (see integrated_()) every LINE_TABLE_STEP,
 * or as finely as it was sampled, made the first time it is needed. A line's light is the PSF
 * integrated over the pixel and along the line, smooth enough that a spline through it this
 * finely is within about 1e-4 of its peak, and the coarser samples keep its lookups in the
 * processor's caches.
 */
template <IsSpectral TSpectral>
const GridPsf<TSpectral>& GridPsf<TSpectral>::line_source_() const
{
    if (pixel_integrated_) {
        return *this;
    }
    std::call_once(line_cache_->source_once, [this] {
        line_cache_->source = std::make_shared<const GridPsf>(integrated_(*this, LINE_TABLE_STEP));
    });
    return *line_cache_->source;
}

/// The collected PSF's scale() at a pixel's center, which lines are drawn with.
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::line_scale() const
{
    std::call_once(line_cache_->scale_once,
                   [this] { line_cache_->scale = line_source_().scale(0.5, 0.5); });
    return line_cache_->scale;
}

/// The directions either side of a piece's (dx, dy), in pixels along the axes.
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::LineDirections GridPsf<TSpectral>::line_directions(double dx,
                                                                                double dy)
{
    const double turn = std::atan2(dy, dx) / (2.0 * PI<double>()) * LINE_DIRECTIONS;
    const double below = std::floor(turn);
    const auto wrap = [](long index) {
        index %= LINE_DIRECTIONS;
        return static_cast<int>(index < 0 ? index + LINE_DIRECTIONS : index);
    };
    LineDirections directions;
    directions.below = wrap(static_cast<long>(below));
    directions.above = wrap(static_cast<long>(below) + 1);
    directions.fraction = turn - below;
    return directions;
}

/// Where a line's tables are made: every step of the collected PSF's samples, or coarser if they
/// would take more than MEMORY_BUDGET, out to where it reaches and two steps beyond.
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::LineLayout GridPsf<TSpectral>::line_layout_() const
{
    const GridPsf& q = line_source_();
    LineLayout layout;
    layout.support = std::hypot(std::max(std::abs(q.left_), std::abs(q.right_)),
                                std::max(std::abs(q.top_), std::abs(q.bottom_)));
    const double most =
        std::sqrt(static_cast<double>(MEMORY_BUDGET) / static_cast<double>(N * sizeof(double)));
    layout.panels = std::max(1, static_cast<int>(std::floor(LINE_TABLE_STEP / q.step_ + 1e-9)));
    layout.step =
        std::max(q.step_ * layout.panels, 2.0 * layout.support / std::max(most - 5.0, 4.0));
    layout.half = static_cast<int>(std::ceil(layout.support / layout.step)) + 2;
    return layout;
}

/// The memory one direction's tables take, as they are kept (in single precision; see Line).
template <IsSpectral TSpectral>
std::size_t GridPsf<TSpectral>::line_table_bytes() const
{
    if (exact()) {
        return 0;
    }
    const auto count = static_cast<std::size_t>(2 * line_layout_().half + 1);
    return (count * count + count) * N * sizeof(float);
}

/// About how many times the collected PSF is evaluated to make one direction's tables; a disc
/// drawn exactly has none, being drawn directly.
template <IsSpectral TSpectral>
double GridPsf<TSpectral>::line_table_work() const
{
    if (exact()) {
        return std::numeric_limits<double>::infinity();
    }
    const auto count = static_cast<double>(2 * line_layout_().half + 1);
    return static_cast<double>(grid_psf_detail::GL4_NODES.size()) * count * count;
}

/**
 * @brief How a line is drawn directly (see draw_line_direct()), found the first time it is: by
 * distance from the source, the longest panel along it that may start there and reach no further
 * than the collected PSF's roughness allows, and the panels a line takes for a pixel there.
 *
 * Gauss-Legendre quadrature on three points integrates each of the spline's cubic pieces exactly,
 * and is off only where the line crosses from one to the next, by at most about 2e-4 of the jump
 * in its third derivative there times the panel's length to the fourth: its coefficients' fourth
 * difference over the step cubed. A panel L long crosses about L / step of them, and so is off by
 * about 2e-4 D L^5 / step^4 for the largest fourth difference D around it, which is no more than
 * DIRECT_TOLERANCE of the brightest coefficient for each unit of its length when L is at most
 * step (DIRECT_TOLERANCE peak / (2e-4 D))^(1/4). (A coefficient's difference bends the spline
 * within two steps of it.)
 */
template <IsSpectral TSpectral>
const typename GridPsf<TSpectral>::LineCache& GridPsf<TSpectral>::direct_setup_() const
{
    std::call_once(line_cache_->direct_once, [this] {
        const GridPsf& q = line_source_();
        const double h = q.step_;
        const double extent = std::hypot(std::max(std::abs(q.left_), std::abs(q.right_)),
                                         std::max(std::abs(q.top_), std::abs(q.bottom_)));
        const auto bins = static_cast<std::size_t>(std::ceil(extent * PANEL_BINS)) + 2;
        Light peak{};
        std::vector<Light> rough(bins, Light{});
        for (int j = 0; j < q.ny_; ++j) {
            for (int i = 0; i < q.nx_; ++i) {
                const double* at = &q.coefficients_[q.at_(i, j)];
                for (std::size_t c = 0; c < N; ++c) {
                    peak[c] = std::max(peak[c], std::abs(at[c]));
                }
                if (i < 2 || i + 2 >= q.nx_ || j < 2 || j + 2 >= q.ny_) {
                    continue;
                }
                const double r = std::hypot(q.x0_ + static_cast<double>(i) * h,
                                            q.y0_ + static_cast<double>(j) * h);
                auto& bin = rough[std::min(static_cast<std::size_t>(r * PANEL_BINS), bins - 1)];
                for (std::size_t c = 0; c < N; ++c) {
                    const auto along = [&](int di, int dj) {
                        return q.coefficients_[q.at_(i + di, j + dj) + c];
                    };
                    const double dx = along(-2, 0) - 4.0 * along(-1, 0) + 6.0 * at[c] -
                                      4.0 * along(1, 0) + along(2, 0);
                    const double dy = along(0, -2) - 4.0 * along(0, -1) + 6.0 * at[c] -
                                      4.0 * along(0, 1) + along(0, 2);
                    bin[c] = std::max({bin[c], std::abs(dx), std::abs(dy)});
                }
            }
        }
        line_cache_->coefficients.assign(q.coefficients_.begin(), q.coefficients_.end());
        const auto spread = static_cast<std::size_t>(std::ceil(3.0 * h * PANEL_BINS));
        auto& limit = line_cache_->panel_limit;
        limit.assign(bins, 2.0 * extent);
        for (std::size_t b = 0; b < bins; ++b) {
            for (std::size_t k = b > spread ? b - spread : 0; k <= std::min(bins - 1, b + spread);
                 ++k) {
                for (std::size_t c = 0; c < N; ++c) {
                    if (peak[c] > 0.0 && rough[k][c] > 0.0) {
                        limit[b] = std::min(
                            limit[b],
                            h * std::pow(DIRECT_TOLERANCE * peak[c] / (2e-4 * rough[k][c]), 0.25));
                    }
                }
            }
            limit[b] = std::max(limit[b], 2.0 * h);
        }
        // The panels a line takes for a pixel at each distance, from its foot to the PSF's edge
        // both ways:
        auto& panels = line_cache_->panels_from;
        panels.assign(bins, 0.0);
        for (std::size_t b = 0; b < bins; ++b) {
            const double p = static_cast<double>(b) / PANEL_BINS;
            const double reach = std::sqrt(std::max(0.0, extent * extent - p * p));
            double count = 0.0;
            for (double u = 0.0; u < reach; ++count) {
                u += panel_length_(*line_cache_, std::hypot(p, u), 2.0 * extent);
            }
            panels[b] = 2.0 * count;
        }
    });
    return *line_cache_;
}

/// The longest panel that may start at a distance near from the source, going away from it:
/// within the limit of every distance it covers.
template <IsSpectral TSpectral>
double
GridPsf<TSpectral>::panel_length_(const LineCache& cache, double near, double far_limit) const
{
    const auto& limit = cache.panel_limit;
    auto b = static_cast<std::size_t>(near * PANEL_BINS);
    if (b >= limit.size()) {
        return far_limit;
    }
    double length = limit[b];
    for (++b; b < limit.size() && static_cast<double>(b) / PANEL_BINS < near + length; ++b) {
        length = std::min(length, limit[b]);
    }
    return length;
}

/**
 * @brief Adds the collected PSF at (x, y), relative to the source in pixels, times a weight, to
 * a sum: the spline within its rectangle, from its coefficients in single precision, which are
 * within 1e-7 of its light; nothing beyond. Within the rectangle, the four coefficients around a
 * place on either axis are always among them, MARGIN beyond it.
 */
template <IsSpectral TSpectral>
void GridPsf<TSpectral>::add_collected_(
    const LineCache& cache, double x, double y, double weight, Light& sum) const
{
    const GridPsf& q = line_source_();
    if (!(x >= q.left_ && x <= q.right_ && y >= q.top_ && y <= q.bottom_)) {
        return;
    }
    const auto weights = [&](double offset, double origin, long& first) {
        const double u = (offset - origin) / q.step_;
        const double floor_u = std::floor(u);
        const double t = u - floor_u;
        const double s = 1.0 - t;
        first = static_cast<long>(floor_u) - 1;
        return std::array<double, 4>{s * s * s / 6.0,
                                     (4.0 - 6.0 * t * t + 3.0 * t * t * t) / 6.0,
                                     (4.0 - 6.0 * s * s + 3.0 * s * s * s) / 6.0,
                                     t * t * t / 6.0};
    };
    long first_x = 0;
    long first_y = 0;
    const std::array<double, 4> wx = weights(x, q.x0_, first_x);
    const std::array<double, 4> wy = weights(y, q.y0_, first_y);
    for (std::size_t b = 0; b < 4; ++b) {
        const float* row = &cache.coefficients[q.at_(
            static_cast<int>(first_x), static_cast<int>(first_y) + static_cast<int>(b))];
        const double wb = weight * wy[b];
        for (std::size_t a = 0; a < 4; ++a) {
            const double w = wb * wx[a];
            const float* at = row + a * N;
            for (std::size_t c = 0; c < N; ++c) {
                sum[c] += w * static_cast<double>(at[c]);
            }
        }
    }
}

/**
 * @brief About how many times the collected PSF is evaluated to draw a piece of a path of the
 * given length (in pixels along the axes) directly (see draw_line_direct()), out to reach pixel
 * widths from it: for the pixels at each distance from it, three for each panel a line takes
 * there, or for a shorter piece, those its length takes.
 */
template <IsSpectral TSpectral>
double GridPsf<TSpectral>::line_direct_work(double length, double reach) const
{
    if (exact()) {
        // The pixels it reaches, each with a few pieces where the disc meets the pixel's edges:
        return 16.0 * 2.0 * reach_ * (length + 2.0 * reach_);
    }
    const LineCache& cache = direct_setup_();
    const auto& panels = cache.panels_from;
    const double band = 1.0 / PANEL_BINS;
    double work = 0.0;
    for (std::size_t b = 0; b < panels.size(); ++b) {
        const double p = static_cast<double>(b) * band;
        if (p > reach) {
            break;
        }
        const double pixels = 2.0 * band * (length + 2.0 * reach);
        const double taken = std::min(panels[b], 2.0 + length / cache.panel_limit[b]);
        work += pixels * 3.0 * taken;
    }
    return work;
}

/// Makes the tables for a direction, as an index LINE_DIRECTIONS-ths of a turn round, unless they
/// are kept already.
template <IsSpectral TSpectral>
void GridPsf<TSpectral>::prepare_line(int direction) const
{
    if (exact()) {
        return;
    }
    static_cast<void>(line_at_(direction));
}

/// The memory the tables lines are drawn from take now.
template <IsSpectral TSpectral>
std::size_t GridPsf<TSpectral>::line_cache_bytes() const
{
    std::lock_guard<std::mutex> lock(line_cache_->mutex);
    return line_cache_->bytes;
}

/**
 * @brief The tables for the direction index LINE_DIRECTIONS-ths of a turn round, made the first
 * time they are needed and kept with the PSF, but for those used least recently once they take
 * more than LINE_CACHE_BUDGET. (They are the same however often they are made.)
 */
template <IsSpectral TSpectral>
std::shared_ptr<const typename GridPsf<TSpectral>::Line>
GridPsf<TSpectral>::line_at_(int index) const
{
    index %= LINE_DIRECTIONS;
    if (index < 0) {
        index += LINE_DIRECTIONS;
    }
    {
        std::lock_guard<std::mutex> lock(line_cache_->mutex);
        const auto found = line_cache_->lines.find(index);
        if (found != line_cache_->lines.end()) {
            found->second.used = ++line_cache_->clock;
            return found->second.line;
        }
    }
    const double angle = 2.0 * PI<double>() * static_cast<double>(index) / LINE_DIRECTIONS;
    auto made = std::make_shared<const Line>(build_line_(std::cos(angle), std::sin(angle)));
    std::lock_guard<std::mutex> lock(line_cache_->mutex);
    auto& lines = line_cache_->lines;
    const auto [kept, added] = lines.try_emplace(index);
    kept->second.used = ++line_cache_->clock;
    if (!added) {
        return kept->second.line;
    }
    kept->second.line = std::move(made);
    line_cache_->bytes += kept->second.line->memory_bytes();
    while (line_cache_->bytes > LINE_CACHE_BUDGET && lines.size() > 1) {
        auto oldest = lines.end();
        for (auto it = lines.begin(); it != lines.end(); ++it) {
            if (it != kept && (oldest == lines.end() || it->second.used < oldest->second.used)) {
                oldest = it;
            }
        }
        line_cache_->bytes -= oldest->second.line->memory_bytes();
        lines.erase(oldest);
    }
    return kept->second.line;
}

/**
 * @brief Tabulates a line in the direction (cos, sin) (see Line): at every step of the collected
 * PSF's samples across the line, p, its integral along the line up to every step, q, by
 * four-point Gauss-Legendre quadrature on each step, which the spline's pieces make all but
 * exact; then the splines through those values. They reach two steps beyond the PSF, where they
 * are flat. Coarser steps are taken if the integrals would take more than MEMORY_BUDGET.
 */
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Line GridPsf<TSpectral>::build_line_(double cos, double sin) const
{
    const GridPsf& q = line_source_();
    const Light scale = line_scale();
    const LineLayout layout = line_layout_();
    Line line;
    line.cos = cos;
    line.sin = sin;
    line.support = layout.support;
    line.step = layout.step;
    line.count = 2 * layout.half + 1;
    line.first = -static_cast<double>(layout.half) * line.step;
    const auto count = static_cast<std::size_t>(line.count);
    std::vector<double> cumulative(count * count * N, 0.0);
    std::vector<double> spread(count * N, 0.0);

    const double nx = -sin;
    const double ny = cos;
    const double panel = line.step / static_cast<double>(layout.panels);
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t row) {
        const double p = line.first + static_cast<double>(row) * line.step;
        double* out = &cumulative[row * count * N];
        Light sum{};
        for (std::size_t k = 1; k < count; ++k) {
            for (int j = 0; j < layout.panels; ++j) {
                const double v0 = line.first + static_cast<double>(k - 1) * line.step +
                                  static_cast<double>(j) * panel;
                for (std::size_t i = 0; i < grid_psf_detail::GL4_NODES.size(); ++i) {
                    const double v = v0 + 0.5 * panel * (1.0 + grid_psf_detail::GL4_NODES[i]);
                    const Light value = q.interpolate_(p * nx + v * cos, p * ny + v * sin);
                    for (std::size_t c = 0; c < N; ++c) {
                        sum[c] += 0.5 * panel * grid_psf_detail::GL4_WEIGHTS[i] * value[c];
                    }
                }
            }
            for (std::size_t c = 0; c < N; ++c) {
                out[k * N + c] = sum[c] * scale[c];
            }
        }
        for (std::size_t c = 0; c < N; ++c) {
            spread[row * N + c] = sum[c] * scale[c];
        }
    });

    // The splines' coefficients, along q and then along p, kept in single precision, which
    // takes half the memory and time to look up, and is within 1e-7 of a line's light:
    for (std::size_t c = 0; c < N; ++c) {
        psf_tables_detail::bspline_coefficients(&spread[c], count, N);
    }
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t row) {
        for (std::size_t c = 0; c < N; ++c) {
            psf_tables_detail::bspline_coefficients(&cumulative[row * count * N + c], count, N);
        }
    });
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t column) {
        for (std::size_t c = 0; c < N; ++c) {
            psf_tables_detail::bspline_coefficients(&cumulative[column * N + c], count, count * N);
        }
    });
    line.spread.assign(spread.begin(), spread.end());
    line.cumulative.assign(cumulative.begin(), cumulative.end());

    return line;
}

/// The line spread at a signed distance p across the line, in pixels along the axes.
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::line_spread_(const Line& line, double p)
{
    Light value{};
    const double u = (p - line.first) / line.step;
    if (!(u > -2.0) || !(u < static_cast<double>(line.count) + 1.0)) {
        return value;
    }
    const long first = static_cast<long>(std::floor(u)) - 1;
    for (long k = first; k < first + 4; ++k) {
        const long at = std::clamp(k, 0L, static_cast<long>(line.count) - 1);
        const double w = grid_psf_detail::spline(u - static_cast<double>(k));
        const float* coefficient = &line.spread[static_cast<std::size_t>(at) * N];
        for (std::size_t c = 0; c < N; ++c) {
            value[c] += w * coefficient[c];
        }
    }
    return value;
}

/**
 * @brief C(p, q) (see Line): nothing before the table, the line spread beyond it, and in it the
 * spline, which is flat at either end in q, so that the coefficients are taken as the last
 * beyond them.
 */
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light
GridPsf<TSpectral>::line_cumulative_(const Line& line, double p, double q)
{
    const double v = (q - line.first) / line.step;
    if (!(v > -2.0)) {
        return Light{};
    }
    if (!(v < static_cast<double>(line.count) + 1.0)) {
        return line_spread_(line, p);
    }
    Light value{};
    const double u = (p - line.first) / line.step;
    if (!(u > -2.0) || !(u < static_cast<double>(line.count) + 1.0)) {
        return value;
    }
    const long count = line.count;
    const long first_p = static_cast<long>(std::floor(u)) - 1;
    const long first_q = static_cast<long>(std::floor(v)) - 1;
    std::array<double, 4> wq{};
    std::array<long, 4> at_q{};
    for (long b = 0; b < 4; ++b) {
        wq[static_cast<std::size_t>(b)] =
            grid_psf_detail::spline(v - static_cast<double>(first_q + b));
        at_q[static_cast<std::size_t>(b)] = std::clamp(first_q + b, 0L, count - 1);
    }
    for (long a = 0; a < 4; ++a) {
        const long row = first_p + a;
        if (row < 0 || row >= count) {
            continue;
        }
        const double wp = grid_psf_detail::spline(u - static_cast<double>(row));
        const float* coefficients =
            &line.cumulative[static_cast<std::size_t>(row) * static_cast<std::size_t>(count) * N];
        for (std::size_t b = 0; b < 4; ++b) {
            const double w = wp * wq[b];
            const float* at = coefficients + static_cast<std::size_t>(at_q[b]) * N;
            for (std::size_t c = 0; c < N; ++c) {
                value[c] += w * at[c];
            }
        }
    }
    return value;
}

/**
 * @brief At least the light per unit length, as a fraction of the source's, that a line in any
 * direction puts in any pixel whose center is r pixel widths or more from it; 0 beyond reach():
 * envelope() integrated along the line (see build_bounds_()).
 */
template <IsSpectral TSpectral>
double GridPsf<TSpectral>::line_envelope(std::size_t channel, double r) const
{
    const auto bin = static_cast<std::size_t>(std::max(0.0, r) * ENVELOPE_BINS);
    if (bin >= line_envelope_.size()) {
        return 0.0;
    }
    return line_envelope_[bin][channel];
}

/**
 * @brief Adds a moving source's light to the pixels of a block of an image: its path's straight
 * piece from start to end (sensor coordinates), with power spread evenly along it, out to reach
 * pixel widths from the piece, times weight(r) at a distance r from it.
 *
 * A pixel's light is the piece's line integral of the pixels' light from a still source (as
 * pixel() gives it), per unit length: the integral along the line up to the pixel's place along
 * it from the start, less that from the end, C(p, q) - C(p, q - length) (see Line), from the
 * tables for the directions either side of the piece's, in proportion. Where the tolerance
 * allows, its faint wings are interpolated (see draw_line_pixels_()).
 *
 * @return The light drawn, per channel, as a fraction of the power.
 */
template <IsSpectral TSpectral>
template <class Weight>
typename GridPsf<TSpectral>::Light
GridPsf<TSpectral>::draw_line(Image<TSpectral>& target,
                              int x_begin,
                              int x_end,
                              int y_begin,
                              int y_end,
                              const std::array<double, 2>& start,
                              const std::array<double, 2>& end,
                              const TSpectral& power,
                              double reach,
                              const Weight& weight,
                              const LineTolerance& tolerance) const
{
    if (exact()) {
        return draw_line_direct(
            target, x_begin, x_end, y_begin, y_end, start, end, power, reach, weight, tolerance);
    }
    // In pixels along the axes:
    const double span_x = end[0] - start[0];
    const double span_y = end[1] - start[1];
    const double length = std::hypot(span_x, span_y);
    if (!(length > 0.0) || x_end <= x_begin || y_end <= y_begin) {
        return Light{};
    }
    const double ex = span_x / length;
    const double ey = span_y / length;
    const double nx = -ey;
    const double ny = ex;
    // The tables either side of the direction, in proportion:
    const LineDirections directions = line_directions(span_x, span_y);
    const double fraction = directions.fraction;
    const auto below_line = line_at_(directions.below);
    const auto above_line = fraction > 0.0 ? line_at_(directions.above) : below_line;

    // A pixel's place among the tables' coefficients, which the two directions share: along p,
    // the four rows around it and their weights, and along q, for C at q and at q less the
    // piece's length, the same, or nothing before the tables, or the line spread beyond them.
    const Line& layout = *below_line;
    const long count = layout.count;
    const double inverse_step = 1.0 / layout.step;
    struct Place {
        long first = 0;
        std::array<double, 4> weights{};
    };
    const auto place = [&](double offset) {
        const double u = (offset - layout.first) * inverse_step;
        const double floor_u = std::floor(u);
        const double t = u - floor_u;
        const double s = 1.0 - t;
        Place axis;
        axis.first = static_cast<long>(floor_u) - 1;
        axis.weights = {s * s * s / 6.0,
                        (4.0 - 6.0 * t * t + 3.0 * t * t * t) / 6.0,
                        (4.0 - 6.0 * s * s + 3.0 * s * s * s) / 6.0,
                        t * t * t / 6.0};
        return axis;
    };
    enum class Reach { Before, Within, Beyond };
    const auto reach_of = [&](double offset) {
        const double v = (offset - layout.first) * inverse_step;
        return !(v > -2.0)                               ? Reach::Before
               : !(v < static_cast<double>(count) + 1.0) ? Reach::Beyond
                                                         : Reach::Within;
    };
    // C at a place along q, less C at another, for both directions in proportion:
    const auto light_between = [&](const Place& across,
                                   Reach near_reach,
                                   const Place& near,
                                   Reach far_reach,
                                   const Place& far) {
        Light light{};
        const auto add = [&](const Line& line,
                             double share,
                             Reach reach_along,
                             const Place& along,
                             double sign) {
            if (reach_along == Reach::Before || !(share > 0.0)) {
                return;
            }
            for (long a = 0; a < 4; ++a) {
                const long row = across.first + a;
                if (row < 0 || row >= count) {
                    continue;
                }
                const double wp = sign * share * across.weights[static_cast<std::size_t>(a)];
                if (reach_along == Reach::Beyond) {
                    const float* at = &line.spread[static_cast<std::size_t>(row) * N];
                    for (std::size_t c = 0; c < N; ++c) {
                        light[c] += wp * at[c];
                    }
                    continue;
                }
                const float* coefficients = &line.cumulative[static_cast<std::size_t>(row) *
                                                             static_cast<std::size_t>(count) * N];
                for (long b = 0; b < 4; ++b) {
                    const long column = std::clamp(along.first + b, 0L, count - 1);
                    const double w = wp * along.weights[static_cast<std::size_t>(b)];
                    const float* at = coefficients + static_cast<std::size_t>(column) * N;
                    for (std::size_t c = 0; c < N; ++c) {
                        light[c] += w * at[c];
                    }
                }
            }
        };
        add(*below_line, 1.0 - fraction, near_reach, near, 1.0);
        add(*above_line, fraction, near_reach, near, 1.0);
        add(*below_line, 1.0 - fraction, far_reach, far, -1.0);
        add(*above_line, fraction, far_reach, far, -1.0);
        return light;
    };
    const auto light_at = [&](double qx, double qy) {
        const double p = qx * nx + qy * ny;
        const double u = (p - layout.first) * inverse_step;
        if (!(u > -2.0) || !(u < static_cast<double>(count) + 1.0)) {
            return Light{};
        }
        const double q = qx * ex + qy * ey;
        return light_between(
            place(p), reach_of(q), place(q), reach_of(q - length), place(q - length));
    };
    return draw_line_pixels_(target,
                             x_begin,
                             x_end,
                             y_begin,
                             y_end,
                             start,
                             end,
                             power,
                             reach,
                             weight,
                             tolerance,
                             light_at);
}

/**
 * @brief Adds a moving source's light to the pixels of a block of an image as draw_line() does,
 * but from the collected PSF directly, without tables: a pixel's light is the PSF integrated
 * along the piece, where it reaches the pixel, by three-point Gauss-Legendre quadrature on panels
 * as long as the PSF's roughness allows where they are (see direct_setup_()), which for most
 * PSFs grow with the distance from the source. It costs more for each pixel than the tables,
 * but nothing to make, and so suits pieces whose direction few others share. A disc drawn exactly,
 * or a point, is integrated exactly instead (see exact_line_light_()), and draw_line() draws it so
 * too. Where the tolerance allows, its faint wings are interpolated (see draw_line_pixels_()).
 *
 * @return The light drawn, per channel, as a fraction of the power.
 */
template <IsSpectral TSpectral>
template <class Weight>
typename GridPsf<TSpectral>::Light
GridPsf<TSpectral>::draw_line_direct(Image<TSpectral>& target,
                                     int x_begin,
                                     int x_end,
                                     int y_begin,
                                     int y_end,
                                     const std::array<double, 2>& start,
                                     const std::array<double, 2>& end,
                                     const TSpectral& power,
                                     double reach,
                                     const Weight& weight,
                                     const LineTolerance& tolerance) const
{
    // In pixels along the axes:
    const double span_x = end[0] - start[0];
    const double span_y = end[1] - start[1];
    const double length = std::hypot(span_x, span_y);
    if (!(length > 0.0) || x_end <= x_begin || y_end <= y_begin) {
        return Light{};
    }
    const double ex = span_x / length;
    const double ey = span_y / length;
    if (exact()) {
        const auto light_at = [&](double qx, double qy) {
            return exact_line_light_(qx, qy, ex, ey, length);
        };
        return draw_line_pixels_(target,
                                 x_begin,
                                 x_end,
                                 y_begin,
                                 y_end,
                                 start,
                                 end,
                                 power,
                                 reach,
                                 weight,
                                 tolerance,
                                 light_at);
    }
    const LineCache& cache = direct_setup_();
    const double nx = -ey;
    const double ny = ex;
    const Light scale = line_scale();
    const auto light_at = [&](double qx, double qy) {
        Light sum{};
        // The part of the piece within the PSF's rectangle of the pixel, where its light is:
        const auto [from, to] = clip_(qx, qy, ex, ey, length).range;
        if (!(to > from)) {
            return sum;
        }
        const double p = qx * nx + qy * ny;
        const double foot = qx * ex + qy * ey;
        const double nearest = std::clamp(foot, from, to);
        const auto panel = [&](double a, double b) {
            const double mid = 0.5 * (a + b);
            const double half = 0.5 * (b - a);
            for (std::size_t i = 0; i < grid_psf_detail::GL3_NODES.size(); ++i) {
                const double v = mid + half * grid_psf_detail::GL3_NODES[i];
                add_collected_(
                    cache, qx - v * ex, qy - v * ey, half * grid_psf_detail::GL3_WEIGHTS[i], sum);
            }
        };
        // Outward from the point nearest the pixel, each panel as long as its near end allows:
        for (double v = nearest; v < to;) {
            const double next =
                std::min(to, v + panel_length_(cache, std::hypot(p, v - foot), to - v));
            panel(v, next);
            v = next;
        }
        for (double v = nearest; v > from;) {
            const double next =
                std::max(from, v - panel_length_(cache, std::hypot(p, v - foot), v - from));
            panel(next, v);
            v = next;
        }
        for (std::size_t c = 0; c < N; ++c) {
            sum[c] *= scale[c];
        }
        return sum;
    };
    return draw_line_pixels_(target,
                             x_begin,
                             x_end,
                             y_begin,
                             y_end,
                             start,
                             end,
                             power,
                             reach,
                             weight,
                             tolerance,
                             light_at);
}

/**
 * @brief Where along a piece, from its start, a pixel's offset from it (qx - v ex, qy - v ey) is
 * within the PSF's rectangle, where its light is, and what cuts that off at either end: the
 * piece's start or end, or the rectangle's edges across x or y. Where the same things cut it off,
 * a pixel's light changes smoothly from pixel to pixel; where they change, it can fall off
 * sharply, at the edge of the PSF swept along the piece.
 */
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Clip
GridPsf<TSpectral>::clip_(double qx, double qy, double ex, double ey, double length) const
{
    const GridPsf& q = line_source_();
    Clip clip;
    clip.range = {0.0, length};
    const auto within = [&](double c, double e, double lo, double hi, int edge) {
        if (std::abs(e) < 1e-300) {
            if (c < lo || c > hi) {
                clip.range[1] = clip.range[0] - 1.0;
            }
            return;
        }
        const double a = (c - hi) / e;
        const double b = (c - lo) / e;
        if (std::min(a, b) > clip.range[0]) {
            clip.range[0] = std::min(a, b);
            clip.cut[0] = edge;
        }
        if (std::max(a, b) < clip.range[1]) {
            clip.range[1] = std::max(a, b);
            clip.cut[1] = edge;
        }
    };
    within(qx, ex, q.left_, q.right_, 1);
    within(qy, ey, q.top_, q.bottom_, 2);
    if (!(clip.range[1] > clip.range[0])) {
        clip.cut = {-1, -1};
    }
    return clip;
}

/**
 * @brief Adds a line's light to the pixels of a block of an image, out to reach pixel widths from
 * it, times weight(r) at a distance r from it: each pixel's from light_at(qx, qy), its offset
 * from the line's start, in pixels along the axes, the light integrated along the line, which is
 * divided by its length.
 *
 * A faint line's pixels are mostly in its wings, where its light changes slowly, and there it is
 * interpolated: the pixels are taken in cells of WING_CELL on a side, from multiples of it, and
 * where a cell's light comes to no more than WING_LIGHT times the tolerance, by the line's
 * envelope, it is computed at the cell's corners, and at its center and the middles of its
 * sides, which are pixels too. If the same things cut the line's light off at all nine (see
 * clip_()), so that the PSF's edge swept along the line does not cross the cell, and bilinear
 * interpolation between the corners gives the other five to within a quarter of the tolerance,
 * it gives the cell's other pixels; otherwise they are computed. Its error is greatest near those
 * five, and in the wings, where the light is the PSF integrated along the line and over the pixel,
 * nothing changes faster than over a few pixels. Every block shares the cells, so the choice does
 * not depend on where blocks fall.
 *
 * @return The light drawn, per channel, as a fraction of the power.
 */
template <IsSpectral TSpectral>
template <class Weight, class LightAt>
typename GridPsf<TSpectral>::Light
GridPsf<TSpectral>::draw_line_pixels_(Image<TSpectral>& target,
                                      int x_begin,
                                      int x_end,
                                      int y_begin,
                                      int y_end,
                                      const std::array<double, 2>& start,
                                      const std::array<double, 2>& end,
                                      const TSpectral& power,
                                      double reach,
                                      const Weight& weight,
                                      const LineTolerance& tolerance,
                                      const LightAt& light_at) const
{
    Light drawn{};
    const double span_x = end[0] - start[0];
    const double span_y = end[1] - start[1];
    const double length = std::hypot(span_x, span_y);
    const double nx = -span_y / length;
    const double ny = span_x / length;
    const GridPsf& q = line_source_();
    const double support = std::hypot(std::max(std::abs(q.left_), std::abs(q.right_)),
                                      std::max(std::abs(q.top_), std::abs(q.bottom_)));
    const double aspect2 = aspect_ * aspect_;
    const double physical2 = span_x * span_x + aspect2 * span_y * span_y;
    const double reach2 = reach * reach;

    // A pixel's distance from the line, in pixel widths, if it is drawn, and how much:
    const auto distance_of = [&](double qx, double qy) {
        const double t = std::clamp((qx * span_x + aspect2 * qy * span_y) / physical2, 0.0, 1.0);
        const double fx = qx - t * span_x;
        const double fy = qy - t * span_y;
        return std::sqrt(fx * fx + aspect2 * fy * fy);
    };
    const auto add = [&](int x, int y, double share, const Light& light) {
        TSpectral& out = target(x, y);
        for (std::size_t c = 0; c < N; ++c) {
            const double part = share * light[c] / length;
            drawn[c] += part;
            out[c] += static_cast<float>(part * static_cast<double>(power[c]));
        }
    };
    // The electrons a pixel's readout takes from light, as light_at() gives it:
    const auto electrons = [&](const Light& light) {
        double sum = 0.0;
        for (std::size_t c = 0; c < N; ++c) {
            const double value = std::abs(light[c]) / length * tolerance.electrons[c];
            sum = tolerance.per_channel ? std::max(sum, value) : sum + value;
        }
        return sum;
    };
    const auto exact_pixel = [&](int x, int y) {
        const double qx = static_cast<double>(x) + 0.5 - start[0];
        const double qy = static_cast<double>(y) + 0.5 - start[1];
        if (std::abs(qx * nx + qy * ny) > support) {
            return;
        }
        const double distance = distance_of(qx, qy);
        if (distance * distance > reach2) {
            return;
        }
        const double share = weight(distance);
        if (share > 0.0) {
            add(x, y, share, light_at(qx, qy));
        }
    };
    if (!(tolerance.electrons_off > 0.0)) {
        for (int y = y_begin; y < y_end; ++y) {
            for (int x = x_begin; x < x_end; ++x) {
                exact_pixel(x, y);
            }
        }
        return drawn;
    }

    // The light at the pixels on even rows and columns, which are the cells' corners, centers
    // and sides' middles, computed as needed:
    constexpr int CELL = WING_CELL;
    constexpr int HALF = CELL / 2;
    const int first_i = (x_begin / CELL) * CELL / HALF;
    const int first_j = (y_begin / CELL) * CELL / HALF;
    const int columns = ((x_end - 1) / CELL + 1) * CELL / HALF + 1 - first_i;
    const int rows = ((y_end - 1) / CELL + 1) * CELL / HALF + 1 - first_j;
    struct Node {
        bool known = false;
        std::array<int, 2> cut{};
        Light light{};
    };
    std::vector<Node> nodes(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows));
    const double ex = span_x / length;
    const double ey = span_y / length;
    const auto node_at = [&](int i, int j) -> const Node& {
        Node& at = nodes[static_cast<std::size_t>(j - first_j) * static_cast<std::size_t>(columns) +
                         static_cast<std::size_t>(i - first_i)];
        if (!at.known) {
            const double qx = static_cast<double>(i * HALF) + 0.5 - start[0];
            const double qy = static_cast<double>(j * HALF) + 0.5 - start[1];
            at.cut = clip_(qx, qy, ex, ey, length).cut;
            at.light = light_at(qx, qy);
            at.known = true;
        }
        return at;
    };
    const auto node = [&](int i, int j) -> const Light& { return node_at(i, j).light; };
    const double faint = WING_LIGHT * tolerance.electrons_off;
    const double cell_reach = HALF * std::hypot(1.0, aspect_);
    Light envelope{};
    for (int cy = (y_begin / CELL) * CELL; cy < y_end; cy += CELL) {
        for (int cx = (x_begin / CELL) * CELL; cx < x_end; cx += CELL) {
            const int x0 = std::max(x_begin, cx);
            const int x1 = std::min(x_end, cx + CELL);
            const int y0 = std::max(y_begin, cy);
            const int y1 = std::min(y_end, cy + CELL);
            // Faint enough everywhere in the cell, by the envelope from its center?
            const double center_distance =
                distance_of(static_cast<double>(cx + HALF) + 0.5 - start[0],
                            static_cast<double>(cy + HALF) + 0.5 - start[1]);
            const double nearest = std::max(0.0, center_distance - cell_reach);
            for (std::size_t c = 0; c < N; ++c) {
                envelope[c] = line_envelope(c, nearest);
            }
            // ...and drawn everywhere, so that none of its corners is computed for nothing:
            bool interpolate = center_distance + cell_reach <= reach && x1 - x0 == CELL &&
                               y1 - y0 == CELL && electrons(envelope) <= faint;
            const int i = cx / HALF;
            const int j = cy / HALF;
            if (interpolate) {
                const Light& c00 = node(i, j);
                const Light& c20 = node(i + 2, j);
                const Light& c02 = node(i, j + 2);
                const Light& c22 = node(i + 2, j + 2);
                const auto off = [&](const Light& exact, double wx, double wy) {
                    Light diff;
                    for (std::size_t c = 0; c < N; ++c) {
                        diff[c] = exact[c] - ((1.0 - wy) * ((1.0 - wx) * c00[c] + wx * c20[c]) +
                                              wy * ((1.0 - wx) * c02[c] + wx * c22[c]));
                    }
                    return electrons(diff);
                };
                // The same things cut the line's light off at all nine, so that the cell holds
                // none of the PSF's edge swept along the line, where the light falls off between
                // pixels:
                const std::array<int, 2> cut = node_at(i, j).cut;
                for (int b = 0; b <= 2 && interpolate; ++b) {
                    for (int a = 0; a <= 2 && interpolate; ++a) {
                        interpolate = node_at(i + a, j + b).cut == cut;
                    }
                }
                const double check = 0.25 * tolerance.electrons_off;
                interpolate = interpolate && off(node(i + 1, j + 1), 0.5, 0.5) <= check &&
                              off(node(i + 1, j), 0.5, 0.0) <= check &&
                              off(node(i + 1, j + 2), 0.5, 1.0) <= check &&
                              off(node(i, j + 1), 0.0, 0.5) <= check &&
                              off(node(i + 2, j + 1), 1.0, 0.5) <= check;
                if (interpolate) {
                    for (int y = y0; y < y1; ++y) {
                        for (int x = x0; x < x1; ++x) {
                            const double qx = static_cast<double>(x) + 0.5 - start[0];
                            const double qy = static_cast<double>(y) + 0.5 - start[1];
                            if (std::abs(qx * nx + qy * ny) > support) {
                                continue;
                            }
                            const double distance = distance_of(qx, qy);
                            if (distance * distance > reach2) {
                                continue;
                            }
                            const double share = weight(distance);
                            if (!(share > 0.0)) {
                                continue;
                            }
                            Light light;
                            if ((x - cx) % HALF == 0 && (y - cy) % HALF == 0) {
                                light = node(x / HALF, y / HALF);
                            } else {
                                const double wx = static_cast<double>(x - cx) / CELL;
                                const double wy = static_cast<double>(y - cy) / CELL;
                                for (std::size_t c = 0; c < N; ++c) {
                                    light[c] = (1.0 - wy) * ((1.0 - wx) * c00[c] + wx * c20[c]) +
                                               wy * ((1.0 - wx) * c02[c] + wx * c22[c]);
                                }
                            }
                            add(x, y, share, light);
                        }
                    }
                    continue;
                }
            }
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    if ((x - cx) % HALF == 0 && (y - cy) % HALF == 0) {
                        // Computed already for the cell's check, or as cheaply now:
                        const double qx = static_cast<double>(x) + 0.5 - start[0];
                        const double qy = static_cast<double>(y) + 0.5 - start[1];
                        if (std::abs(qx * nx + qy * ny) > support) {
                            continue;
                        }
                        const double distance = distance_of(qx, qy);
                        if (distance * distance > reach2) {
                            continue;
                        }
                        const double share = weight(distance);
                        if (share > 0.0) {
                            add(x, y, share, node(x / HALF, y / HALF));
                        }
                    } else {
                        exact_pixel(x, y);
                    }
                }
            }
        }
    }
    return drawn;
}

} // namespace huira
