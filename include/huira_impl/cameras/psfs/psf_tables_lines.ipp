#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "huira/cameras/psfs/psf_profile.hpp"
#include "huira/core/constants.hpp"
#include "tbb/parallel_for.h"

namespace huira {

namespace psf_tables_detail {

/// Catmull-Rom weights of four evenly spaced values, at a fraction t of the way from the second
/// to the third.
inline std::array<double, 4> catmull_rom_weights(double t)
{
    const double t2 = t * t;
    const double t3 = t2 * t;
    return {0.5 * (-t3 + 2.0 * t2 - t),
            0.5 * (3.0 * t3 - 5.0 * t2 + 2.0),
            0.5 * (-3.0 * t3 + 4.0 * t2 + t),
            0.5 * (t3 - t2)};
}

} // namespace psf_tables_detail

/**
 * @brief Tabulates what drawing a moving source needs: each channel's line spread, the light
 * per unit length across a line, with its two antiderivatives (see line_exact_()); the far
 * field shapes' line spreads; and how far the far field's light strays near the line and near
 * its ends.
 */
template <IsSpectral TSpectral>
template <class Profile>
void PsfTables<TSpectral>::build_lines_(const std::vector<Profile>& profiles)
{
    constexpr std::size_t N = TSpectral::size();
    const double step = grid_.step;
    const double half_diagonal = 0.5 * std::hypot(1.0, aspect_);
    double limit = near_radius_;
    for (std::size_t c = 0; c < N; ++c) {
        limit = std::max(limit, strip_limit_[c]);
    }
    const double count_wanted = std::ceil((limit + half_diagonal + 2.0 * step) / step) + 2.0;
    if (count_wanted > psf_tables_detail::MAX_STRIP_NODES) {
        HUIRA_THROW_ERROR("PsfTables - The line spreads would need " +
                          std::to_string(count_wanted) +
                          " nodes; the reach or the PSF's detail is out of range");
    }
    line_count_ = static_cast<std::size_t>(count_wanted);

    // Each node's line spread and slope, and, as for the strips, the line spread's integrals
    // over the interval above the node, plain and times the distance from the node, summed
    // downward into the two antiderivatives. Beyond the last node they are a constant and a
    // straight line, which the second differences drop.
    std::vector<std::vector<std::array<double, 4>>> nodes(N);
    tbb::parallel_for(std::size_t{0}, N, [&](std::size_t c) {
        std::vector<std::array<double, 4>>& channel = nodes[c];
        channel.resize(line_count_);
        std::vector<std::array<double, 2>> pieces(line_count_);
        for (std::size_t k = 0; k < line_count_; ++k) {
            const double d = static_cast<double>(k) * step;
            const std::array<double, 3> line = profiles[c].line_derivatives(d);
            channel[k][2] = line[0];
            channel[k][3] = line[1];
            if (k + 1 < line_count_) {
                const double mid = d + 0.5 * step;
                const double half = 0.5 * step;
                double light = 0.0;
                double moment = 0.0;
                for (std::size_t i = 0; i < detail::airy_band_detail::GL8_NODES.size(); ++i) {
                    const double offset = half * detail::airy_band_detail::GL8_NODES[i];
                    const double weight = half * detail::airy_band_detail::GL8_WEIGHTS[i];
                    const double below = profiles[c].line_derivatives(mid - offset)[0];
                    const double above = profiles[c].line_derivatives(mid + offset)[0];
                    light += weight * (below + above);
                    moment += weight * (below * (half - offset) + above * (half + offset));
                }
                pieces[k] = {light, moment};
            }
        }
        double g1 = 0.0;
        double g2 = 0.0;
        double g1_carry = 0.0;
        double g2_carry = 0.0;
        const auto add = [](double& sum, double& carry, double value) {
            const double next = sum + value;
            carry += std::abs(sum) >= std::abs(value) ? (sum - next) + value : (value - next) + sum;
            sum = next;
        };
        channel[line_count_ - 1][0] = 0.0;
        channel[line_count_ - 1][1] = 0.0;
        for (std::size_t k = line_count_ - 1; k-- > 0;) {
            add(g2, g2_carry, step * (g1 + g1_carry) + pieces[k][1]);
            add(g1, g1_carry, pieces[k][0]);
            channel[k][0] = g2 + g2_carry;
            channel[k][1] = g1 + g1_carry;
        }
    });

    line_nodes_.assign(line_count_ * N * 4, 0.0);
    line_suffix_.assign(N, {});
    for (std::size_t c = 0; c < N; ++c) {
        for (std::size_t k = 0; k < line_count_; ++k) {
            for (std::size_t j = 0; j < 4; ++j) {
                line_nodes_[(k * 4 + j) * N + c] = nodes[c][k][j];
            }
        }
        std::vector<double>& suffix = line_suffix_[c];
        suffix.resize(line_count_);
        double running = 0.0;
        for (std::size_t k = line_count_; k-- > 0;) {
            running = std::max(running, nodes[c][k][2]);
            suffix[k] = running;
        }
        // Half of the light lies either side; 90% within d of the line leaves 45% on each.
        const double total = nodes[c][0][1];
        line_holding_90_[c] = static_cast<double>(line_count_ - 1) * step;
        for (std::size_t k = 1; k < line_count_; ++k) {
            const double inside = total - nodes[c][k][1];
            if (inside >= 0.45) {
                const double before = total - nodes[c][k - 1][1];
                line_holding_90_[c] =
                    step * (static_cast<double>(k - 1) + (0.45 - before) / (inside - before));
                break;
            }
        }
    }

    build_far_lines_();
    build_half_lines_();
    build_corners_();
    build_line_deviation_();
}

/**
 * @brief A far field shape (see far_rank_) at r, with its first two derivatives: from its nodes,
 * and beyond the last, as the power law it follows there.
 */
template <IsSpectral TSpectral>
std::array<double, 3> PsfTables<TSpectral>::far_shape_(std::size_t shape, double r) const
{
    const std::size_t last = far_radii_.size() - 1;
    if (r >= far_radii_[last]) {
        const double* end = &far_basis_[(last * far_rank_ + shape) * 3];
        const double r_end = far_radii_[last];
        if (!(end[0] > 0.0)) {
            return {0.0, 0.0, 0.0};
        }
        const double falloff = -r_end * end[1] / end[0];
        const double value = end[0] * std::pow(r / r_end, -falloff);
        return {value, -falloff * value / r, falloff * (falloff + 1.0) * value / (r * r)};
    }
    const std::size_t k = far_node_(r);
    const double h = far_radii_[k + 1] - far_radii_[k];
    const double t = (r - far_radii_[k]) * far_inverse_steps_[k];
    const double* a = &far_basis_[(k * far_rank_ + shape) * 3];
    const double* b = &far_basis_[((k + 1) * far_rank_ + shape) * 3];
    return detail::psf_profile_detail::quintic_hermite(
        t, h, {a[0], a[1], a[2]}, {b[0], b[1], b[2]});
}

/**
 * @brief Each far field shape's line spread, its Abel transform, with its first two
 * derivatives, at the far fields' nodes (see far_radii_), so that a pixel finds its place among
 * them as it does for a still source.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::build_far_lines_()
{
    const std::size_t count = far_radii_.size();
    far_line_nodes_.assign(count * far_rank_ * 3, 0.0);
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t k) {
        const double d = far_radii_[k];
        for (std::size_t shape = 0; shape < far_rank_; ++shape) {
            const std::array<double, 3> line =
                detail::abel_derivatives([&](double r) { return far_shape_(shape, r); }, d, d);
            for (std::size_t j = 0; j < 3; ++j) {
                far_line_nodes_[(k * far_rank_ + shape) * 3 + j] = line[j];
            }
        }
    });
}

/**
 * @brief A far field shape's line spread at d, with its second derivative: from its nodes by
 * quintic Hermite interpolation, the second derivative linearly, and beyond the last node as the
 * power law it follows there.
 */
template <IsSpectral TSpectral>
std::array<double, 2> PsfTables<TSpectral>::far_line_(std::size_t shape, double d) const
{
    const std::size_t last = far_radii_.size() - 1;
    if (d >= far_radii_[last]) {
        const double* end = &far_line_nodes_[(last * far_rank_ + shape) * 3];
        if (!(end[0] > 0.0)) {
            return {0.0, 0.0};
        }
        const double d_end = far_radii_[last];
        const double falloff = -d_end * end[1] / end[0];
        const double value = end[0] * std::pow(d / d_end, -falloff);
        return {value, falloff * (falloff + 1.0) * value / (d * d)};
    }
    const std::size_t k = far_node_(d);
    const double h = far_radii_[k + 1] - far_radii_[k];
    const double t = (d - far_radii_[k]) * far_inverse_steps_[k];
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double h0 = 1.0 - t3 * (10.0 - 15.0 * t + 6.0 * t2);
    const double h1 = h * (t - t3 * (6.0 - 8.0 * t + 3.0 * t2));
    const double h2 = h * h * 0.5 * t2 * (1.0 - 3.0 * t + 3.0 * t2 - t2 * t);
    const double h4 = h * t3 * (-4.0 + 7.0 * t - 3.0 * t2);
    const double h5 = h * h * 0.5 * t3 * (1.0 - 2.0 * t + t2);
    const double* a = &far_line_nodes_[(k * far_rank_ + shape) * 3];
    const double* b = a + far_rank_ * 3;
    return {h0 * a[0] + (1.0 - h0) * b[0] + h1 * a[1] + h4 * b[1] + h2 * a[2] + h5 * b[2],
            a[2] + t * (b[2] - a[2])};
}

/**
 * @brief Every channel's light in a pixel from a line through the source's path a signed
 * distance d from the pixel's center, for a unit of light per unit length, exactly: the line
 * spread integrated over the pixel's projection across the line, a trapezoid, as a second
 * difference of its second antiderivative (see strip_values_()). The line spread is even, so
 * its antiderivatives at -x follow from those at x.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::line_exact_(double normal_x,
                                       double normal_y,
                                       double d,
                                       const std::array<bool, TSpectral::size()>& wanted,
                                       Values& values) const
{
    constexpr std::size_t N = TSpectral::size();
    const double side_x = std::abs(normal_x);
    const double side_y = aspect_ * std::abs(normal_y);
    const double wide = std::max(side_x, side_y);
    const double narrow = std::min(side_x, side_y);
    const double step = grid_.step;
    const double last = static_cast<double>(line_count_ - 2);

    // G2 at a signed distance (G2' = -G1, G2'' = the line spread), or G1: with the
    // antiderivatives taken from x to the table's end, G1(-x) = 2 G1(0) - G1(x) and
    // G2(-x) = G2(x) + 2 G1(0) x. Each point is placed among the nodes once, and the channels,
    // which lie together in each node, taken together.
    struct Place {
        const double* a;
        double x;
        bool negative;
        double h0;
        double h1;
        double h2;
        double h4;
        double h5;
    };
    const auto place = [&](double distance) {
        const double x = std::abs(distance);
        const double u = std::clamp(x / step, 0.0, last + 1.0);
        const auto k = static_cast<std::size_t>(std::min(std::floor(u), last));
        const double t = u - static_cast<double>(k);
        const double t2 = t * t;
        const double t3 = t2 * t;
        return Place{&line_nodes_[k * 4 * N],
                     x,
                     distance < 0.0,
                     1.0 - t3 * (10.0 - 15.0 * t + 6.0 * t2),
                     step * (t - t3 * (6.0 - 8.0 * t + 3.0 * t2)),
                     step * step * 0.5 * t2 * (1.0 - 3.0 * t + 3.0 * t2 - t3),
                     step * t3 * (-4.0 + 7.0 * t - 3.0 * t2),
                     step * step * 0.5 * t3 * (1.0 - 2.0 * t + t2)};
    };
    const double* at_zero = &line_nodes_[N]; // G1(0)
    // scale times G2 at a place, added to every channel's value:
    const auto add_second = [&](const Place& p, double scale) {
        const double* a = p.a;
        const double* b = p.a + 4 * N;
        const double h3 = 1.0 - p.h0;
        const double mirror = p.negative ? 2.0 * p.x : 0.0;
        for (std::size_t c = 0; c < N; ++c) {
            values[c] += scale * (p.h0 * a[c] + h3 * b[c] - p.h1 * a[N + c] - p.h4 * b[N + c] +
                                  p.h2 * a[2 * N + c] + p.h5 * b[2 * N + c] + mirror * at_zero[c]);
        }
    };
    // scale times G1 at a place, likewise:
    const auto add_first = [&](const Place& p, double scale) {
        const double* a = p.a;
        const double* b = p.a + 4 * N;
        const double h3 = 1.0 - p.h0;
        const double sign = p.negative ? -scale : scale;
        const double mirror = p.negative ? 2.0 * scale : 0.0;
        for (std::size_t c = 0; c < N; ++c) {
            values[c] += sign * (p.h0 * a[N + c] + h3 * b[N + c] - p.h1 * a[2 * N + c] -
                                 p.h4 * b[2 * N + c] - p.h2 * a[3 * N + c] - p.h5 * b[3 * N + c]) +
                         mirror * at_zero[c];
        }
    };

    values.fill(0.0);
    constexpr double NARROW = 0.02;
    if (narrow >= NARROW) {
        const double outer = 0.5 * (wide + narrow);
        const double inner = 0.5 * (wide - narrow);
        const double scale = aspect_ / (wide * narrow);
        add_second(place(d + outer), scale);
        add_second(place(d + inner), -scale);
        add_second(place(d - inner), -scale);
        add_second(place(d - outer), scale);
    } else {
        const double near_edge = d - 0.5 * wide;
        const double far_edge = d + 0.5 * wide;
        const double scale = aspect_ / wide;
        add_first(place(near_edge), scale);
        add_first(place(far_edge), -scale);
        // The narrow side's correction, narrow^2 / 24 times the slope's change across; the
        // slope is odd.
        const auto add_slope = [&](double distance, double weight) {
            const double x = std::abs(distance);
            const double u = std::clamp(x / step, 0.0, last + 1.0);
            const auto k = static_cast<std::size_t>(std::min(std::floor(u), last));
            const double t = u - static_cast<double>(k);
            const double* a = &line_nodes_[(k * 4 + 3) * N];
            const double* b = a + 4 * N;
            const double signed_weight = distance < 0.0 ? -weight : weight;
            for (std::size_t c = 0; c < N; ++c) {
                values[c] += signed_weight * ((1.0 - t) * a[c] + t * b[c]);
            }
        };
        const double correction = scale * narrow * narrow / 24.0;
        add_slope(far_edge, correction);
        add_slope(near_edge, -correction);
    }
    for (std::size_t c = 0; c < N; ++c) {
        if (!wanted[c]) {
            values[c] = 0.0;
        }
    }
}

/**
 * @brief Every channel's light in a pixel from a line a distance d from its center, from the far
 * field's line spread, averaged over the pixel's projection across the line to second order.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::line_far_(double normal_x,
                                     double normal_y,
                                     double d,
                                     Values& values) const
{
    const double spread = (normal_x * normal_x + aspect_ * aspect_ * normal_y * normal_y) / 24.0;
    Values shapes;
    for (std::size_t i = 0; i < far_rank_; ++i) {
        const std::array<double, 2> line = far_line_(i, d);
        shapes[i] = aspect_ * (line[0] + spread * line[1]);
    }
    combine_far_(shapes, values);
}

/**
 * @brief Tabulates line_deviation(): every RING_STEP from where the first far field holds to
 * the end of the line spreads' tables, the most that a pixel, for a line in any of 16 directions
 * across a quadrant, strays from the far field's line spread, then the greatest at or beyond
 * each node.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::build_line_deviation_()
{
    constexpr std::size_t N = TSpectral::size();
    constexpr int DIRECTIONS = 16;
    double limit = near_radius_;
    for (std::size_t c = 0; c < N; ++c) {
        limit = std::max(limit, strip_limit_[c]);
    }
    // (Far out of focus, the far field can hold only from beyond the tables' ends.)
    const auto count =
        static_cast<std::size_t>(std::ceil(std::max(limit - ring_start_, 0.0) / RING_STEP)) + 1;
    std::vector<Values> found(count);
    std::array<bool, N> all{};
    all.fill(true);
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t k) {
        const double d = ring_start_ + static_cast<double>(k) * RING_STEP;
        Values most{};
        for (int j = 0; j < DIRECTIONS; ++j) {
            const double angle =
                0.5 * PI<double>() * static_cast<double>(j) / static_cast<double>(DIRECTIONS - 1);
            const double normal_x = std::cos(angle);
            const double normal_y = std::sin(angle);
            Values exact{};
            Values far{};
            line_exact_(normal_x, normal_y, d, all, exact);
            line_far_(normal_x, normal_y, d, far);
            for (std::size_t c = 0; c < N; ++c) {
                most[c] = std::max(most[c], std::abs(exact[c] - far[c]));
            }
        }
        for (std::size_t c = 0; c < N; ++c) {
            if (d >= strip_limit_[c]) {
                most[c] = 0.0;
            } else if (d < smooth_from_[c]) {
                most[c] = std::numeric_limits<double>::infinity();
            }
        }
        found[k] = most;
    });
    line_deviation_.assign(N, {});
    for (std::size_t c = 0; c < N; ++c) {
        std::vector<double>& deviation = line_deviation_[c];
        deviation.resize(count);
        double running = 0.0;
        for (std::size_t k = count; k-- > 0;) {
            running = std::max(running, found[k][c]);
            deviation[k] = running;
        }
        while (!deviation.empty() && deviation.back() == 0.0) {
            deviation.pop_back();
        }
    }

    // How far a half line's light strays from the far field's, from the pixels' own integrated
    // along it out past the near table: at a sample of distances from its start, angles from
    // the pixel's foot and directions across a quadrant, then the greatest at or beyond each.
    constexpr int ANGLES = 7;
    constexpr int END_DIRECTIONS = 3;
    const double end_limit = near_radius_ + BLEND_WIDTH;
    const auto end_count =
        static_cast<std::size_t>(std::ceil(std::max(end_limit - ring_start_, 0.0) / END_STEP)) + 1;
    const double exact_within = near_radius_ + 2.0 * BLEND_WIDTH;
    std::vector<Values> end_found(end_count);
    tbb::parallel_for(std::size_t{0}, end_count, [&](std::size_t k) {
        const double r = ring_start_ + static_cast<double>(k) * END_STEP;
        Values most{};
        for (int j = 0; j < END_DIRECTIONS; ++j) {
            const double direction = 0.5 * PI<double>() * static_cast<double>(j) /
                                     static_cast<double>(END_DIRECTIONS - 1);
            const double hx = std::cos(direction);
            const double hy = std::sin(direction);
            for (int a = 0; a < ANGLES; ++a) {
                const double beta =
                    0.5 * PI<double>() * static_cast<double>(a) / static_cast<double>(ANGLES - 1);
                const double along = r * std::cos(beta);
                const double beside = r * std::sin(beta);
                const double qx = -along * hx - beside * hy;
                const double qy = -along * hy + beside * hx;
                Values exact{};
                Values far{};
                half_line_(qx, qy, hx, hy, exact_within, exact);
                half_line_far_(qx, qy, hx, hy, far);
                for (std::size_t c = 0; c < N; ++c) {
                    most[c] = std::max(most[c], std::abs(exact[c] - far[c]));
                }
            }
        }
        for (std::size_t c = 0; c < N; ++c) {
            if (r < smooth_from_[c]) {
                most[c] = std::numeric_limits<double>::infinity();
            }
        }
        end_found[k] = most;
    });
    end_deviation_.assign(N, {});
    for (std::size_t c = 0; c < N; ++c) {
        std::vector<double>& deviation = end_deviation_[c];
        deviation.resize(end_count);
        double running = 0.0;
        for (std::size_t k = end_count; k-- > 0;) {
            running = std::max(running, end_found[k][c]);
            deviation[k] = running;
        }
    }
}

/**
 * @brief An upper bound on how far a pixel at least d from a line strays, in a channel, from
 * the far field's line spread, for a unit of light per unit length: how much following the line
 * spread's rings beyond d can matter. Infinite where the far field does not hold, 0 beyond the
 * line spread's table.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::line_deviation(std::size_t channel, double d) const
{
    constexpr double MARGIN = 1.5;
    if (d < ring_start_) {
        return std::numeric_limits<double>::infinity();
    }
    const std::vector<double>& deviation = line_deviation_[channel];
    const double u = std::max(0.0, (d - ring_start_ - 1.0) / RING_STEP);
    if (u >= static_cast<double>(deviation.size())) {
        return 0.0;
    }
    return MARGIN * deviation[static_cast<std::size_t>(u)];
}

/**
 * @brief An upper bound on how far the light a half line puts in a pixel at least r from its
 * start strays, in a channel, from the far field's, for a unit of light per unit length: how
 * much integrating the pixels' own light along it beyond r can matter. Tabulated out to the near
 * table's radius, beyond which ends are not followed more closely (0); infinite where the far
 * field does not hold.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::end_deviation(std::size_t channel, double r) const
{
    constexpr double MARGIN = 1.5;
    if (r < ring_start_) {
        return std::numeric_limits<double>::infinity();
    }
    const std::vector<double>& deviation = end_deviation_[channel];
    const double u = (r - ring_start_) / END_STEP;
    if (u >= static_cast<double>(deviation.size())) {
        return 0.0;
    }
    return MARGIN * deviation[static_cast<std::size_t>(u)];
}

/**
 * @brief An upper bound on the light, in a channel, of any pixel whose center is at least r from
 * a line, for a unit of light per unit length along it: the pixel's area times the line spread's
 * greatest value from the pixel's nearest corner outward.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::line_envelope(std::size_t channel, double r) const
{
    const double nearest = std::max(0.0, r - 0.5 * std::hypot(1.0, aspect_));
    const auto k = static_cast<std::size_t>(nearest / grid_.step);
    if (k < line_count_) {
        return aspect_ * line_suffix_[channel][k];
    }
    Values shapes{};
    for (std::size_t i = 0; i < far_rank_; ++i) {
        shapes[i] = far_line_(i, nearest)[0];
    }
    Values values{};
    combine_far_(shapes, values);
    return aspect_ * values[channel];
}

/**
 * @brief The light of a still source beyond a straight edge, per channel, with the source a
 * signed distance d from it: positive on the near side. Within the line spread's table it is the
 * line spread's integral from |d| out, read from the table's first antiderivative, with the half
 * of the light either side of the line fixing what lies beyond the table's end; beyond the end,
 * twice the light beyond two edges with one through the source (see corner_light()).
 */
template <IsSpectral TSpectral>
std::array<double, TSpectral::size()> PsfTables<TSpectral>::half_plane_light(double d) const
{
    constexpr std::size_t N = TSpectral::size();
    const double x = std::abs(d);
    const double step = grid_.step;
    const double table_end = static_cast<double>(line_count_ - 1) * step;
    Values result{};
    if (x < table_end) {
        const double u = x / step;
        const auto k = std::min(static_cast<std::size_t>(u), line_count_ - 2);
        const double t = u - static_cast<double>(k);
        const double t2 = t * t;
        const double t3 = t2 * t;
        const double h0 = 1.0 - t3 * (10.0 - 15.0 * t + 6.0 * t2);
        const double h1 = step * (t - t3 * (6.0 - 8.0 * t + 3.0 * t2));
        const double h2 = step * step * 0.5 * t2 * (1.0 - 3.0 * t + 3.0 * t2 - t3);
        const double h4 = step * t3 * (-4.0 + 7.0 * t - 3.0 * t2);
        const double h5 = step * step * 0.5 * t3 * (1.0 - 2.0 * t + t2);
        const double* a = &line_nodes_[k * 4 * N];
        const double* b = a + 4 * N;
        for (std::size_t c = 0; c < N; ++c) {
            const double g1 = h0 * a[N + c] + (1.0 - h0) * b[N + c] - h1 * a[2 * N + c] -
                              h4 * b[2 * N + c] - h2 * a[3 * N + c] - h5 * b[3 * N + c];
            result[c] = g1 + (0.5 - line_nodes_[N + c]);
        }
    } else {
        result = corner_table_(x, 0.0);
        for (double& value : result) {
            value *= 2.0;
        }
    }
    if (d < 0.0) {
        for (double& value : result) {
            value = 1.0 - value;
        }
    }
    return result;
}

/**
 * @brief The light of a still source beyond two perpendicular edges, per channel: in the
 * quarter of the plane past both, with the source signed distances a and b from them, positive
 * on the near side. With a negative distance, the edge's far side holds the source, and the
 * light is that beyond the other edge less that in the quarter across the first; with both
 * positive, from the table (see corners_), or nearer than corner_start_ to the corner, from the
 * profile.
 */
template <IsSpectral TSpectral>
std::array<double, TSpectral::size()> PsfTables<TSpectral>::corner_light(double a, double b) const
{
    if (a < 0.0 || b < 0.0) {
        const Values beyond = half_plane_light(a < 0.0 ? b : a);
        const Values across = a < 0.0 ? corner_light(-a, b) : corner_light(a, -b);
        Values result{};
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            result[c] = beyond[c] - across[c];
        }
        return result;
    }
    return std::hypot(a, b) < corner_start_ ? corner_integral_(a, b) : corner_table_(a, b);
}

/**
 * @brief The light of a still source in a rectangle, per channel, with the source at the origin
 * and the rectangle [x0, x1] by [y0, y1] in pixel widths: all of it, less that beyond each edge,
 * plus that beyond two edges, which was taken away twice.
 */
template <IsSpectral TSpectral>
std::array<double, TSpectral::size()>
PsfTables<TSpectral>::light_in_rectangle(double x0, double x1, double y0, double y1) const
{
    constexpr std::size_t N = TSpectral::size();
    Values result;
    result.fill(1.0);
    const std::array<double, 2> across_x{-x0, x1};
    const std::array<double, 2> across_y{-y0, y1};
    for (const double d : {-x0, x1, -y0, y1}) {
        const Values beyond = half_plane_light(d);
        for (std::size_t c = 0; c < N; ++c) {
            result[c] -= beyond[c];
        }
    }
    for (const double a : across_x) {
        for (const double b : across_y) {
            const Values both = corner_light(a, b);
            for (std::size_t c = 0; c < N; ++c) {
                result[c] += both[c];
            }
        }
    }
    return result;
}

/**
 * @brief The light beyond two perpendicular edges at distances a, b from the source, positive
 * or a's or b's negative, from the profile: integrated along the arcs of circles about the
 * source that lie past both edges, out to 64 times the furthest distance, and beyond as the light
 * there times the share of a circle past them.
 */
template <IsSpectral TSpectral>
typename PsfTables<TSpectral>::Values PsfTables<TSpectral>::corner_integral_(double a,
                                                                             double b) const
{
    // The length of the arc of radius r past an edge at a distance c >= 0, and past two at
    // distances p, q >= 0:
    const auto past_edge = [](double c, double r) {
        return r > c ? 2.0 * r * std::acos(c / r) : 0.0;
    };
    const auto past_both = [](double p, double q, double r) {
        if (p * p + q * q >= r * r) {
            return 0.0;
        }
        return r * (std::acos(std::min(q / r, 1.0)) - std::asin(std::min(p / r, 1.0)));
    };
    const auto arc = [&](double r) {
        if (b < 0.0) {
            return past_edge(a, r) - past_both(a, -b, r);
        }
        if (a < 0.0) {
            return past_edge(b, r) - past_both(-a, b, r);
        }
        return past_both(a, b, r);
    };
    const double from = a < 0.0 ? b : b < 0.0 ? a : std::hypot(a, b);
    const double end = 64.0 * std::max({std::abs(a), std::abs(b), 1.0});
    Values result = integrate(std::max(from, 0.0), end, {std::hypot(a, b)}, arc);
    const double share = arc(end) / (2.0 * PI<double>() * end);
    for (std::size_t c = 0; c < TSpectral::size(); ++c) {
        result[c] += share * smooth_light_beyond(c, end);
    }
    return result;
}

/**
 * @brief Tabulates corner_light() from corner_start_, the near table's radius, out to
 * CORNER_END: see corners_. Each node is integrated from the profile.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::build_corners_()
{
    constexpr std::size_t N = TSpectral::size();
    constexpr std::size_t COLUMNS = CORNER_ANGLES + 2;
    const double angle_step = 0.25 * PI<double>() / static_cast<double>(CORNER_ANGLES);
    corner_start_ = near_radius_;
    const double span = std::log(CORNER_END / corner_start_) / CORNER_STEP;
    corner_rows_ = static_cast<std::size_t>(std::ceil(std::max(span, 1.0))) + 3;
    corners_.assign(corner_rows_ * COLUMNS * N, 0.0);
    tbb::parallel_for(std::size_t{0}, corner_rows_ * COLUMNS, [&](std::size_t node) {
        const std::size_t row = node / COLUMNS;
        const std::size_t column = node % COLUMNS;
        const double radius =
            corner_start_ * std::exp((static_cast<double>(row) - 1.0) * CORNER_STEP);
        const double angle = (static_cast<double>(column) - 1.0) * angle_step;
        const Values light = corner_integral_(radius * std::cos(angle), radius * std::sin(angle));
        std::copy(
            light.begin(), light.end(), corners_.begin() + static_cast<std::ptrdiff_t>(node * N));
    });
}

/**
 * @brief corner_light() for a, b >= 0 at least corner_start_ from the corner, from the table, by
 * Catmull-Rom interpolation in log R and the angle, which is even about pi / 4; beyond the table,
 * falling as the power of R its last two rows give.
 */
template <IsSpectral TSpectral>
typename PsfTables<TSpectral>::Values PsfTables<TSpectral>::corner_table_(double a, double b) const
{
    constexpr std::size_t N = TSpectral::size();
    constexpr std::size_t COLUMNS = CORNER_ANGLES + 2;
    const double angle_step = 0.25 * PI<double>() / static_cast<double>(CORNER_ANGLES);
    const double radius = std::hypot(a, b);
    const double angle = std::atan2(std::min(a, b), std::max(a, b));
    const double v = angle / angle_step;
    const auto column = std::min(static_cast<std::size_t>(v), CORNER_ANGLES - 1);
    const std::array<double, 4> across =
        psf_tables_detail::catmull_rom_weights(v - static_cast<double>(column));
    // Columns column - 1 to column + 2, stored from one step below 0; the last mirrors
    // CORNER_ANGLES - 1 past pi / 4.
    std::array<std::size_t, 4> columns{};
    for (std::size_t i = 0; i < 4; ++i) {
        const std::size_t stored = column + i;
        columns[i] = stored <= CORNER_ANGLES + 1 ? stored : 2 * CORNER_ANGLES + 2 - stored;
    }
    const auto row_values = [&](std::size_t row, Values& values) {
        values.fill(0.0);
        for (std::size_t i = 0; i < 4; ++i) {
            const double* node = &corners_[(row * COLUMNS + columns[i]) * N];
            for (std::size_t c = 0; c < N; ++c) {
                values[c] += across[i] * node[c];
            }
        }
    };
    const double last = static_cast<double>(corner_rows_ - 3);
    const double t = std::log(radius / corner_start_) / CORNER_STEP;
    Values result{};
    if (t >= last) {
        Values before;
        Values at;
        row_values(corner_rows_ - 3, before);
        row_values(corner_rows_ - 2, at);
        const double beyond = t - last;
        for (std::size_t c = 0; c < N; ++c) {
            result[c] =
                before[c] > 0.0 && at[c] > 0.0 ? at[c] * std::pow(at[c] / before[c], beyond) : 0.0;
        }
        return result;
    }
    const auto row = static_cast<std::size_t>(std::max(t, 0.0));
    const std::array<double, 4> along =
        psf_tables_detail::catmull_rom_weights(std::max(t, 0.0) - static_cast<double>(row));
    Values values;
    for (std::size_t i = 0; i < 4; ++i) {
        row_values(row + i, values);
        for (std::size_t c = 0; c < N; ++c) {
            result[c] += along[i] * values[c];
        }
    }
    return result;
}

/**
 * @brief Each far field shape's integral along half lines, and its Laplacian's: see HalfLines.
 * At a distance rho from the half line's start and an angle beta from the pixel's foot, the
 * foot rho cos(beta) behind the start and the pixel rho sin(beta) beside the line, the integral
 * of g(sqrt(s^2 + (rho sin beta)^2)) for s from rho cos(beta) out: on Gauss-Legendre panels
 * from there, growing, and beyond 10^7 rho, the power law g follows.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::build_half_lines_()
{
    const double pi = PI<double>();
    constexpr double RHO_RATIO = 1.05;
    constexpr std::size_t BETA_STEPS = 32; // across a right angle
    constexpr std::size_t MARGIN = 3;      // nodes beyond each end, for the cubic's neighbours
    const double log_step = std::log(RHO_RATIO);
    const double log_start = std::log(ring_start_) - static_cast<double>(MARGIN) * log_step;
    const double log_end = std::log(std::max(grid_.far_end, 2.0 * ring_start_));
    const auto rho_count =
        static_cast<std::size_t>(std::ceil((log_end - log_start) / log_step)) + MARGIN + 1;
    const double beta_step = 0.5 * pi / static_cast<double>(BETA_STEPS);
    const std::size_t beta_count = BETA_STEPS + 1 + MARGIN;

    half_lines_.assign(far_rank_, {});
    for (std::size_t shape = 0; shape < far_rank_; ++shape) {
        HalfLines& table = half_lines_[shape];
        table.log_start = log_start;
        table.log_step = log_step;
        table.rho_count = rho_count;
        table.beta_step = beta_step;
        table.beta_count = beta_count;
        table.f.resize(rho_count * beta_count);
        table.laplacian.resize(rho_count * beta_count);
        tbb::parallel_for(std::size_t{0}, rho_count, [&](std::size_t i) {
            const double rho = std::exp(log_start + static_cast<double>(i) * log_step);
            for (std::size_t j = 0; j < beta_count; ++j) {
                const double beta = static_cast<double>(j) * beta_step;
                const double along = rho * std::cos(beta);
                const double beside = rho * std::sin(beta);
                const double beside2 = beside * beside;
                const auto integrand = [&](double s) {
                    const double r = std::sqrt(s * s + beside2);
                    const std::array<double, 3> g = far_shape_(shape, r);
                    return std::array<double, 2>{g[0], g[2] + g[1] / r};
                };
                std::array<double, 2> sum{};
                double start = along;
                double panel = 0.25 * rho;
                while (start < 1e7 * rho) {
                    const std::array<double, 2> part =
                        detail::psf_profile_detail::gauss_legendre_panels<2>(
                            integrand, start, start + panel, 1);
                    sum[0] += part[0];
                    sum[1] += part[1];
                    start += panel;
                    panel *= 1.3;
                }
                const std::array<double, 3> tail = far_shape_(shape, start);
                if (tail[0] > 0.0) {
                    const double falloff = -start * tail[1] / tail[0];
                    if (falloff > 1.0) {
                        sum[0] += tail[0] * start / (falloff - 1.0);
                        sum[1] += falloff * falloff * tail[0] / (start * (falloff + 1.0));
                    }
                }
                table.f[i * beta_count + j] = sum[0] * rho * rho;
                table.laplacian[i * beta_count + j] = sum[1] * rho * rho * rho * rho;
            }
        });
    }
}

/**
 * @brief Every channel's light in a pixel from a half line, for a unit of light per unit length,
 * from the far field: the half line starts at q's negative from the pixel's center, q in
 * pixel widths, and runs along the unit vector e, away from the pixel's foot on it. Averaged
 * over the pixel to second order, from the integral's Hessian: along the line its derivatives
 * are those of the shape at the start, and the Laplacian's integral gives the rest.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::half_line_far_(
    double qx, double qy, double ex, double ey, Values& values) const
{
    // In the frame of e1 = -e and e2 beside it: the foot is along behind the start, the pixel
    // beside the line.
    const double along = -(qx * ex + qy * ey);
    const double beside = qx * -ey + qy * ex;
    const double rho = std::sqrt(along * along + beside * beside);
    const double beta = std::atan2(std::abs(beside), along);
    const double e1x = -ex;
    const double e1y = -ey;
    const double e2x = -ey;
    const double e2y = ex;
    const double rho2 = rho * rho;

    Values shapes{};
    for (std::size_t i = 0; i < far_rank_; ++i) {
        const HalfLines& table = half_lines_[i];
        const double u = (std::log(rho) - table.log_start) / table.log_step;
        const auto row = static_cast<std::size_t>(
            std::clamp(std::floor(u), 1.0, static_cast<double>(table.rho_count - 3)));
        const double v = beta / table.beta_step;
        const auto column = static_cast<std::size_t>(
            std::clamp(std::floor(v), 0.0, static_cast<double>(table.beta_count - 3)));
        const std::array<double, 4> wu =
            psf_tables_detail::catmull_rom_weights(u - static_cast<double>(row));
        const std::array<double, 4> wv =
            psf_tables_detail::catmull_rom_weights(v - static_cast<double>(column));
        double f = 0.0;
        double laplacian = 0.0;
        for (std::size_t a = 0; a < 4; ++a) {
            const std::size_t r = row - 1 + a;
            for (std::size_t b = 0; b < 4; ++b) {
                // Below beta = 0 the integral mirrors: it is even in the distance beside.
                const std::size_t col = column + b == 0 ? 1 : column + b - 1;
                const double weight = wu[a] * wv[b];
                f += weight * table.f[r * table.beta_count + col];
                laplacian += weight * table.laplacian[r * table.beta_count + col];
            }
        }
        f /= rho2;
        laplacian /= rho2 * rho2;
        const double slope = far_shape_(i, rho)[1];
        const double f11 = -slope * along / rho;
        const double f12 = -slope * beside / rho;
        const double f22 = laplacian - f11;
        const double hxx = e1x * e1x * f11 + 2.0 * e1x * e2x * f12 + e2x * e2x * f22;
        const double hyy = e1y * e1y * f11 + 2.0 * e1y * e2y * f12 + e2y * e2y * f22;
        shapes[i] = aspect_ * (f + (hxx + aspect_ * aspect_ * hyy) / 24.0);
    }
    combine_far_(shapes, values);
}

/**
 * @brief Every channel's light in a pixel from a half line, for a unit of light per unit
 * length: from the pixels' own light within exact_within of the pixel, integrated along it on
 * three-point Gauss-Legendre panels half the pattern's finest period long, and beyond, from the
 * far field. The half line starts at -q from the pixel's center and runs along the unit vector
 * h, away from the pixel's foot on it; lengths are in pixel widths.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::half_line_(
    double qx, double qy, double hx, double hy, double exact_within, Values& values) const
{
    constexpr std::size_t N = TSpectral::size();
    const double along = -(qx * hx + qy * hy);
    const double beside = qx * -hy + qy * hx;
    const double rho2 = along * along + beside * beside;
    if (rho2 >= exact_within * exact_within) {
        half_line_far_(qx, qy, hx, hy, values);
        return;
    }
    const double exit = std::sqrt(exact_within * exact_within - beside * beside) - along;
    const double panel = std::min(0.125, 4.0 / static_cast<double>(near_samples_));
    const auto panels = static_cast<std::size_t>(std::ceil(exit / panel));
    const double width = exit / static_cast<double>(std::max<std::size_t>(panels, 1));
    std::array<double, N> sum{};
    for (std::size_t k = 0; k < panels; ++k) {
        const double mid = (static_cast<double>(k) + 0.5) * width;
        for (std::size_t i = 0; i < psf_tables_detail::GL3_NODES.size(); ++i) {
            const double s = mid + 0.5 * width * psf_tables_detail::GL3_NODES[i];
            const double weight = 0.5 * width * psf_tables_detail::GL3_WEIGHTS[i];
            const TSpectral light = pixel(qx - s * hx, (qy - s * hy) / aspect_);
            for (std::size_t c = 0; c < N; ++c) {
                sum[c] += weight * static_cast<double>(light[c]);
            }
        }
    }
    half_line_far_(qx - exit * hx, qy - exit * hy, hx, hy, values);
    for (std::size_t c = 0; c < N; ++c) {
        values[c] += sum[c];
    }
}

/**
 * @brief Adds a moving source's light to the pixels of a block of an image: its path's straight
 * piece from start to end, with density light per unit length along it, out to reach.reach from
 * the piece, times weight(r) at a distance r from it.
 *
 * A pixel's light is the piece's line integral of the pixels' light from a still source: the
 * whole line's, from the line spread (exactly within reach.exact_within of the line, from the
 * far field's beyond), less the half lines beyond each end. A half line's light comes from the
 * pixels' own within reach.ends_exact_within of the pixel, and from the far field beyond; ends
 * further than reach.ends_within are left out. See draw() for the coordinates.
 *
 * @return The light drawn, per channel.
 */
template <IsSpectral TSpectral>
template <class Weight>
std::array<double, TSpectral::size()>
PsfTables<TSpectral>::draw_line(Image<TSpectral>& target,
                                int x_begin,
                                int x_end,
                                int y_begin,
                                int y_end,
                                const std::array<double, 2>& start,
                                const std::array<double, 2>& end,
                                const TSpectral& density,
                                const LineReach& reach,
                                const Weight& weight) const
{
    constexpr std::size_t N = TSpectral::size();
    Values drawn{};
    // Physical lengths, in pixel widths.
    const double span_x = end[0] - start[0];
    const double span_y = aspect_ * (end[1] - start[1]);
    const double length = std::hypot(span_x, span_y);
    if (!(length > 0.0)) {
        return drawn;
    }
    const double tx = span_x / length;
    const double ty = span_y / length;
    const double nx = -ty;
    const double ny = tx;
    std::array<double, N> per_length{};
    for (std::size_t c = 0; c < N; ++c) {
        per_length[c] = static_cast<double>(density[c]);
    }
    const double ends_within2 = reach.ends_within * reach.ends_within;
    // Beyond this every channel takes the far field's line spread.
    const double far_only =
        std::min(far_from_, std::max(reach.exact_within, smooth_from_all_) + BLEND_WIDTH);

    // Each channel's far field is blended in from here, where its rings stop mattering or its
    // projected integrals end, whichever is nearer (the blend's weight rises steadily):
    Values blend_from{};
    for (std::size_t c = 0; c < N; ++c) {
        blend_from[c] = std::min(far_blend_from_[c], std::max(reach.exact_within, smooth_from_[c]));
    }

    // Far from the piece, against its length, its light is a still source's at its middle,
    // spread along it to second order (see far_shapes_()): the next order is
    // (length / r)^4 n (n + 1) (n + 2) (n + 3) / 1920 of the light for a far field falling as
    // r^-n, under 5e-5 for n = 3 beyond POINT_LENGTHS lengths.
    constexpr double POINT_LENGTHS = 8.0;
    const double point_from = std::max(POINT_LENGTHS * length, far_only + 0.5 * length);
    const double point_from2 = point_from * point_from;
    const double point_to = far_radii_.back();
    const double spread = length * length / 24.0;
    const double middle_x = 0.5 * span_x;
    const double middle_y = 0.5 * span_y;
    // Each shape's light per channel, for the whole piece:
    std::array<TSpectral, N> shape_power{};
    Values piece_light{};
    for (std::size_t c = 0; c < N; ++c) {
        piece_light[c] = length * per_length[c];
    }
    for (std::size_t i = 0; i < far_rank_; ++i) {
        for (std::size_t c = 0; c < N; ++c) {
            const double coefficient = far_coefficients_.empty()
                                           ? (c == i ? 1.0 : 0.0)
                                           : far_coefficients_[c * far_rank_ + i];
            shape_power[i][c] = static_cast<float>(coefficient * piece_light[c]);
        }
    }
    Values shape_drawn{};
    Values shapes{};

    // The pixels whose centers lie within reach of the piece: on each row, an interval, where
    // the row crosses the capsule the reach sweeps around the piece.
    const double radius = reach.reach;
    const auto row_span = [&](double qy) {
        double low = std::numeric_limits<double>::infinity();
        double high = -low;
        const auto join = [&](double a, double b) {
            if (a <= b) {
                low = std::min(low, a);
                high = std::max(high, b);
            }
        };
        const auto disc = [&](double cy, double cx) {
            const double h2 = radius * radius - (qy - cy) * (qy - cy);
            if (h2 >= 0.0) {
                const double h = std::sqrt(h2);
                join(cx - h, cx + h);
            }
        };
        disc(0.0, 0.0);
        disc(span_y, span_x);
        // The band beside the piece: 0 <= u <= length and |d| <= radius, each linear in x.
        double a = -std::numeric_limits<double>::infinity();
        double b = std::numeric_limits<double>::infinity();
        const auto bound = [&](double coefficient, double lo, double hi) {
            if (std::abs(coefficient) < 1e-12) {
                if (lo > 0.0 || hi < 0.0) {
                    a = std::numeric_limits<double>::infinity();
                }
                return;
            }
            const double first = lo / coefficient;
            const double second = hi / coefficient;
            a = std::max(a, std::min(first, second));
            b = std::min(b, std::max(first, second));
        };
        bound(tx, -qy * ty, length - qy * ty);
        bound(nx, -radius - qy * ny, radius - qy * ny);
        join(a, b);
        return std::array<double, 2>{low, high};
    };

    for (int y = y_begin; y < y_end; ++y) {
        const double qy = aspect_ * (static_cast<double>(y) + 0.5 - start[1]);
        const std::array<double, 2> span =
            std::isinf(radius) ? std::array<double, 2>{-1e300, 1e300} : row_span(qy);
        if (!(span[0] <= span[1])) {
            continue;
        }
        const int row_begin = std::max(
            x_begin, static_cast<int>(std::ceil(std::max(span[0] + start[0] - 0.5, -1e9))));
        const int row_end = std::min(
            x_end, static_cast<int>(std::floor(std::min(span[1] + start[0] - 0.5, 1e9))) + 1);
        for (int x = row_begin; x < row_end; ++x) {
            const double qx = static_cast<double>(x) + 0.5 - start[0];
            const double u = qx * tx + qy * ty;
            const double d = qx * nx + qy * ny;
            const double to_end_x = qx - span_x;
            const double to_end_y = qy - span_y;
            const double distance = u < 0.0      ? std::hypot(qx, qy)
                                    : u > length ? std::hypot(to_end_x, to_end_y)
                                                 : std::abs(d);
            if (distance > reach.reach) {
                continue;
            }
            const double share = weight(distance);
            if (share == 0.0) {
                continue;
            }
            const double from_middle_x = qx - middle_x;
            const double from_middle_y = qy - middle_y;
            const double r2 = from_middle_x * from_middle_x + from_middle_y * from_middle_y;
            if (r2 >= point_from2 && r2 < point_to * point_to) {
                const double r = std::sqrt(r2);
                const double cosine = (from_middle_x * tx + from_middle_y * ty) / r;
                far_shapes_(
                    from_middle_x, from_middle_y / aspect_, r, shapes, spread, cosine * cosine);
                TSpectral value{0.f};
                for (std::size_t i = 0; i < far_rank_; ++i) {
                    const double part = share * shapes[i];
                    shape_drawn[i] += part;
                    value += shape_power[i] * static_cast<float>(part);
                }
                target(x, y) += value;
                continue;
            }
            Values light{};

            if (u >= 0.0 && u < length && std::abs(d) >= far_only) {
                Values far_values{};
                line_far_(nx, ny, std::abs(d), far_values);
                for (std::size_t c = 0; c < N; ++c) {
                    light[c] += per_length[c] * far_values[c];
                }
            } else if (u >= 0.0 && u < length) {
                // The whole line's light, exactly within its rings' reach and from the far
                // field's line spread beyond, each channel blended into the next as pixel()
                // does.
                const double offset = std::abs(d);
                Values far_weight{};
                std::array<bool, N> exact{};
                bool any_exact = false;
                bool any_far = false;
                for (std::size_t c = 0; c < N; ++c) {
                    const double w =
                        psf_tables_detail::smooth_step((offset - blend_from[c]) / BLEND_WIDTH);
                    far_weight[c] = w;
                    exact[c] = w < 1.0;
                    any_exact = any_exact || exact[c];
                    any_far = any_far || w > 0.0;
                }
                Values exact_values{};
                Values far_values{};
                if (any_exact) {
                    line_exact_(nx, ny, d, exact, exact_values);
                }
                if (any_far) {
                    line_far_(nx, ny, offset, far_values);
                }
                for (std::size_t c = 0; c < N; ++c) {
                    const double whole = far_weight[c] >= 1.0 ? far_values[c]
                                         : far_weight[c] > 0.0
                                             ? (1.0 - far_weight[c]) * exact_values[c] +
                                                   far_weight[c] * far_values[c]
                                             : exact_values[c];
                    light[c] += per_length[c] * whole;
                }
            }

            // Less the half lines beyond the ends, or for a pixel beyond an end, the half line
            // from one end less that from the other: each runs from its end away from the
            // pixel's foot.
            const auto end_light = [&](double ex, double ey, double along, double sign) {
                if (ex * ex + ey * ey >= ends_within2) {
                    return;
                }
                const double direction = along >= 0.0 ? -1.0 : 1.0;
                Values half{};
                half_line_(ex, ey, direction * tx, direction * ty, reach.ends_exact_within, half);
                const double signed_sign = along >= 0.0 ? sign : -sign;
                for (std::size_t c = 0; c < N; ++c) {
                    light[c] += signed_sign * per_length[c] * half[c];
                }
            };
            end_light(qx, qy, u, -1.0);
            end_light(to_end_x, to_end_y, u - length, 1.0);

            TSpectral value{0.f};
            for (std::size_t c = 0; c < N; ++c) {
                const double part = share * light[c];
                drawn[c] += part;
                value[c] = static_cast<float>(part);
            }
            target(x, y) += value;
        }
    }
    Values from_shapes{};
    combine_far_(shape_drawn, from_shapes);
    for (std::size_t c = 0; c < N; ++c) {
        drawn[c] += piece_light[c] * from_shapes[c];
    }
    return drawn;
}

} // namespace huira
