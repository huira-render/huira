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
    for (const double x : {x0, x1}) {
        if (std::abs(x) < r) {
            const double y = std::sqrt(r2 - x * x);
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
 * widths, the aperture's image out of focus, as pixel-integrated samples every half or quarter
 * SAMPLE_STEP (or as finely as MEMORY_BUDGET allows): each the disc's area within a pixel centered
 * there, exactly, over the whole area. They sum over the pixels to all of the light wherever the
 * source is, so scale() is 1.
 *
 * @param blur The disc's radius, in pixel widths; positive.
 * @param aspect The pixel's height over its width.
 * @throws std::runtime_error if the blur or aspect is not positive and finite.
 */
template <IsSpectral TSpectral>
GridPsf<TSpectral> GridPsf<TSpectral>::disc(double blur, double aspect)
{
    if (!(blur > 0.0) || !std::isfinite(blur) || !(aspect > 0.0) || !std::isfinite(aspect)) {
        HUIRA_THROW_ERROR("GridPsf::disc - The blur and aspect must be positive and finite: " +
                          std::to_string(blur) + ", " + std::to_string(aspect));
    }
    GridPsf result;
    result.aspect_ = aspect;
    result.pixel_integrated_ = true;
    result.sums_to_one_ = true;
    // A pixel meets the disc out to its radius and half a pixel more, in pixels along each axis:
    const double half_x = blur + 0.5;
    const double half_y = blur / aspect + 0.5;
    const double bytes = static_cast<double>(N * sizeof(double) * 2);
    const double most = std::sqrt(static_cast<double>(MEMORY_BUDGET) / bytes) - 2.0 * MARGIN - 1.0;
    // Where a pixel's corners cross the disc's edge, the area bends sharply; a finer step than
    // a PSF's keeps the spline within about 2e-4 of the peak there (1e-4 for discs under a pixel):
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

/// The collected PSF's scale() at a pixel's center, which lines are drawn with.
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::line_scale() const
{
    std::call_once(line_cache_->scale_once,
                   [this] { line_cache_->scale = collected_().scale(0.5, 0.5); });
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
    const GridPsf& q = collected_();
    LineLayout layout;
    layout.support = std::hypot(std::max(std::abs(q.left_), std::abs(q.right_)),
                                std::max(std::abs(q.top_), std::abs(q.bottom_)));
    const double most =
        std::sqrt(static_cast<double>(MEMORY_BUDGET) / static_cast<double>(N * sizeof(double)));
    layout.step = std::max(q.step_, 2.0 * layout.support / std::max(most - 5.0, 4.0));
    layout.half = static_cast<int>(std::ceil(layout.support / layout.step)) + 2;
    return layout;
}

/// The memory one direction's tables take.
template <IsSpectral TSpectral>
std::size_t GridPsf<TSpectral>::line_table_bytes() const
{
    const auto count = static_cast<std::size_t>(2 * line_layout_().half + 1);
    return (count * count + count) * N * sizeof(double);
}

/// About how many times the collected PSF is evaluated to make one direction's tables.
template <IsSpectral TSpectral>
double GridPsf<TSpectral>::line_table_work() const
{
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
        const GridPsf& q = collected_();
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
 * @brief About how many times the collected PSF is evaluated to draw a piece of a path of the
 * given length (in pixels along the axes) directly (see draw_line_direct()), out to reach pixel
 * widths from it: for the pixels at each distance from it, three for each panel a line takes
 * there, or for a shorter piece, those its length takes.
 */
template <IsSpectral TSpectral>
double GridPsf<TSpectral>::line_direct_work(double length, double reach) const
{
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
    const GridPsf& q = collected_();
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
    line.cumulative.assign(count * count * N, 0.0);
    line.spread.assign(count * N, 0.0);

    const double nx = -sin;
    const double ny = cos;
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t row) {
        const double p = line.first + static_cast<double>(row) * line.step;
        double* out = &line.cumulative[row * count * N];
        Light sum{};
        for (std::size_t k = 1; k < count; ++k) {
            const double v0 = line.first + static_cast<double>(k - 1) * line.step;
            for (std::size_t i = 0; i < grid_psf_detail::GL4_NODES.size(); ++i) {
                const double v = v0 + 0.5 * line.step * (1.0 + grid_psf_detail::GL4_NODES[i]);
                const Light value = q.interpolate_(p * nx + v * cos, p * ny + v * sin);
                for (std::size_t c = 0; c < N; ++c) {
                    sum[c] += 0.5 * line.step * grid_psf_detail::GL4_WEIGHTS[i] * value[c];
                }
            }
            for (std::size_t c = 0; c < N; ++c) {
                out[k * N + c] = sum[c] * scale[c];
            }
        }
        for (std::size_t c = 0; c < N; ++c) {
            line.spread[row * N + c] = sum[c] * scale[c];
        }
    });

    // The splines' coefficients, along q and then along p:
    for (std::size_t c = 0; c < N; ++c) {
        psf_tables_detail::bspline_coefficients(&line.spread[c], count, N);
    }
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t row) {
        for (std::size_t c = 0; c < N; ++c) {
            psf_tables_detail::bspline_coefficients(
                &line.cumulative[row * count * N + c], count, N);
        }
    });
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t column) {
        for (std::size_t c = 0; c < N; ++c) {
            psf_tables_detail::bspline_coefficients(
                &line.cumulative[column * N + c], count, count * N);
        }
    });

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
        const double* coefficient = &line.spread[static_cast<std::size_t>(at) * N];
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
        const double* coefficients =
            &line.cumulative[static_cast<std::size_t>(row) * static_cast<std::size_t>(count) * N];
        for (std::size_t b = 0; b < 4; ++b) {
            const double w = wp * wq[b];
            const double* at = coefficients + static_cast<std::size_t>(at_q[b]) * N;
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
 * it from the start, less that from the end, C(p, q) - C(p, q - length) (see Line), from the lines
 * either side of the piece's direction (see line()), in proportion.
 *
 * @return The light drawn, per channel, as a fraction of the power.
 */
template <IsSpectral TSpectral>
template <class Weight>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::draw_line(Image<TSpectral>& target,
                                                                 int x_begin,
                                                                 int x_end,
                                                                 int y_begin,
                                                                 int y_end,
                                                                 const std::array<double, 2>& start,
                                                                 const std::array<double, 2>& end,
                                                                 const TSpectral& power,
                                                                 double reach,
                                                                 const Weight& weight) const
{
    Light drawn{};
    // In pixels along the axes:
    const double span_x = end[0] - start[0];
    const double span_y = end[1] - start[1];
    const double length = std::hypot(span_x, span_y);
    if (!(length > 0.0) || x_end <= x_begin || y_end <= y_begin) {
        return drawn;
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
    const double support = below_line->support;
    // The nearest point of the piece in pixel widths, for the distance the weight takes:
    const double aspect2 = aspect_ * aspect_;
    const double physical2 = span_x * span_x + aspect2 * span_y * span_y;
    const double reach2 = reach * reach;
    const auto piece = [&](const Line& line, double p, double q) {
        const Light to_pixel = line_cumulative_(line, p, q);
        const Light to_end = line_cumulative_(line, p, q - length);
        Light light;
        for (std::size_t c = 0; c < N; ++c) {
            light[c] = to_pixel[c] - to_end[c];
        }
        return light;
    };

    for (int y = y_begin; y < y_end; ++y) {
        const double qy = static_cast<double>(y) + 0.5 - start[1];
        for (int x = x_begin; x < x_end; ++x) {
            const double qx = static_cast<double>(x) + 0.5 - start[0];
            const double p = qx * nx + qy * ny;
            if (std::abs(p) > support) {
                continue;
            }
            const double t =
                std::clamp((qx * span_x + aspect2 * qy * span_y) / physical2, 0.0, 1.0);
            const double fx = qx - t * span_x;
            const double fy = qy - t * span_y;
            const double distance2 = fx * fx + aspect2 * fy * fy;
            if (distance2 > reach2) {
                continue;
            }
            const double share = weight(std::sqrt(distance2));
            if (!(share > 0.0)) {
                continue;
            }
            const double q = qx * ex + qy * ey;
            Light light = piece(*below_line, p, q);
            if (fraction > 0.0) {
                const Light upper = piece(*above_line, p, q);
                for (std::size_t c = 0; c < N; ++c) {
                    light[c] = (1.0 - fraction) * light[c] + fraction * upper[c];
                }
            }
            TSpectral& out = target(x, y);
            for (std::size_t c = 0; c < N; ++c) {
                const double part = share * light[c] / length;
                drawn[c] += part;
                out[c] += static_cast<float>(part * static_cast<double>(power[c]));
            }
        }
    }
    return drawn;
}

/**
 * @brief Adds a moving source's light to the pixels of a block of an image as draw_line() does,
 * but from the collected PSF directly, without tables: a pixel's light is the PSF integrated
 * along the piece, where it reaches the pixel, by three-point Gauss-Legendre quadrature on panels
 * as long as the PSF's roughness allows where they are (see direct_setup_()), which for most
 * PSFs grow with the distance from the source. It costs more for each pixel than the tables,
 * but nothing to make, and so suits pieces whose direction few others share.
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
                                     const Weight& weight) const
{
    Light drawn{};
    // In pixels along the axes:
    const double span_x = end[0] - start[0];
    const double span_y = end[1] - start[1];
    const double length = std::hypot(span_x, span_y);
    if (!(length > 0.0) || x_end <= x_begin || y_end <= y_begin) {
        return drawn;
    }
    const GridPsf& q = collected_();
    const Light scale = line_scale();
    const LineCache& cache = direct_setup_();
    const double ex = span_x / length;
    const double ey = span_y / length;
    const double nx = -ey;
    const double ny = ex;
    const double support = std::hypot(std::max(std::abs(q.left_), std::abs(q.right_)),
                                      std::max(std::abs(q.top_), std::abs(q.bottom_)));
    const double aspect2 = aspect_ * aspect_;
    const double physical2 = span_x * span_x + aspect2 * span_y * span_y;
    const double reach2 = reach * reach;
    // Where along the piece a pixel's offset from it, c - v e, is within [lo, hi] on one axis:
    const auto within = [](double c, double e, double lo, double hi, double& from, double& to) {
        if (std::abs(e) < 1e-300) {
            if (c < lo || c > hi) {
                to = from - 1.0;
            }
            return;
        }
        const double a = (c - hi) / e;
        const double b = (c - lo) / e;
        from = std::max(from, std::min(a, b));
        to = std::min(to, std::max(a, b));
    };

    for (int y = y_begin; y < y_end; ++y) {
        const double qy = static_cast<double>(y) + 0.5 - start[1];
        for (int x = x_begin; x < x_end; ++x) {
            const double qx = static_cast<double>(x) + 0.5 - start[0];
            const double p = qx * nx + qy * ny;
            if (std::abs(p) > support) {
                continue;
            }
            const double t =
                std::clamp((qx * span_x + aspect2 * qy * span_y) / physical2, 0.0, 1.0);
            const double fx = qx - t * span_x;
            const double fy = qy - t * span_y;
            const double distance2 = fx * fx + aspect2 * fy * fy;
            if (distance2 > reach2) {
                continue;
            }
            const double share = weight(std::sqrt(distance2));
            if (!(share > 0.0)) {
                continue;
            }
            // The part of the piece within the PSF's rectangle of the pixel, where its light is:
            double from = 0.0;
            double to = length;
            within(qx, ex, q.left_, q.right_, from, to);
            within(qy, ey, q.top_, q.bottom_, from, to);
            if (!(to > from)) {
                continue;
            }
            const double foot = qx * ex + qy * ey;
            const double nearest = std::clamp(foot, from, to);
            Light sum{};
            const auto panel = [&](double a, double b) {
                const double mid = 0.5 * (a + b);
                const double half = 0.5 * (b - a);
                for (std::size_t i = 0; i < grid_psf_detail::GL3_NODES.size(); ++i) {
                    const double v = mid + half * grid_psf_detail::GL3_NODES[i];
                    const Light value = q.interpolate_(qx - v * ex, qy - v * ey);
                    for (std::size_t c = 0; c < N; ++c) {
                        sum[c] += half * grid_psf_detail::GL3_WEIGHTS[i] * value[c];
                    }
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
            TSpectral& out = target(x, y);
            for (std::size_t c = 0; c < N; ++c) {
                const double part = share * sum[c] * scale[c] / length;
                drawn[c] += part;
                out[c] += static_cast<float>(part * static_cast<double>(power[c]));
            }
        }
    }
    return drawn;
}

} // namespace huira
