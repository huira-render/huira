#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#include "huira/cameras/psfs/airy_band.hpp"
#include "huira/core/constants.hpp"
#include "huira/util/logger.hpp"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

namespace huira {

namespace psf_tables_detail {

/// Three-point Gauss-Legendre rule on [-1, 1].
inline constexpr std::array<double, 3> GL3_NODES{
    -0.7745966692414833770358531, 0.0, 0.7745966692414833770358531};
inline constexpr std::array<double, 3> GL3_WEIGHTS{5.0 / 9.0, 8.0 / 9.0, 5.0 / 9.0};

/// 0 at t <= 0, 1 at t >= 1, and smooth (with zero slope at both ends) between.
inline double smooth_step(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

/**
 * @brief Turns n samples, stride apart, into the coefficients of the cubic B-spline that
 * interpolates them, in place, with the samples mirrored about both ends.
 *
 * The recursive filter of Unser (1999), as in Thevenaz, Blu and Unser (2000), "Interpolation
 * revisited". A mirror about the first sample matches a table of offsets from 0, which is
 * symmetric about 0.
 */
inline void bspline_coefficients(double* data, std::size_t n, std::size_t stride)
{
    if (n < 2) {
        return;
    }
    const double pole = std::sqrt(3.0) - 2.0;
    const double gain = (1.0 - pole) * (1.0 - 1.0 / pole);
    auto at = [&](std::size_t k) -> double& { return data[k * stride]; };
    for (std::size_t k = 0; k < n; ++k) {
        at(k) *= gain;
    }

    // The causal filter starts from the mirrored samples before the first; their weights fall as
    // pole^k, below 1e-17 after 30.
    const std::size_t horizon = std::min<std::size_t>(n, 30);
    double sum = at(0);
    double power = pole;
    for (std::size_t k = 1; k < horizon; ++k) {
        sum += power * at(k);
        power *= pole;
    }
    at(0) = sum;
    for (std::size_t k = 1; k < n; ++k) {
        at(k) += pole * at(k - 1);
    }

    at(n - 1) = (pole / (pole * pole - 1.0)) * (pole * at(n - 2) + at(n - 1));
    for (std::size_t k = n - 1; k-- > 0;) {
        at(k) = pole * (at(k + 1) - at(k));
    }
}

/// Weights of a cubic B-spline's four taps, at a fraction t of the way from the second to the
/// third.
inline std::array<double, 4> bspline_weights(double t)
{
    const double s = 1.0 - t;
    const double t2 = t * t;
    const double t3 = t2 * t;
    return {s * s * s / 6.0,
            (3.0 * t3 - 6.0 * t2 + 4.0) / 6.0,
            (-3.0 * t3 + 3.0 * t2 + 3.0 * t + 1.0) / 6.0,
            t3 / 6.0};
}

/**
 * @brief Quintic Hermite interpolation between two nodes step apart, from the value, first and
 * second derivative at each.
 */
inline double quintic_hermite(double t,
                              double step,
                              double value0,
                              double first0,
                              double second0,
                              double value1,
                              double first1,
                              double second1)
{
    const double t2 = t * t;
    const double t3 = t2 * t;
    const double h0 = 1.0 - t3 * (10.0 - 15.0 * t + 6.0 * t2);
    const double h1 = t - t3 * (6.0 - 8.0 * t + 3.0 * t2);
    const double h2 = 0.5 * t2 * (1.0 - 3.0 * t + 3.0 * t2 - t3);
    const double h4 = t3 * (-4.0 + 7.0 * t - 3.0 * t2);
    const double h5 = 0.5 * t3 * (1.0 - 2.0 * t + t2);
    return h0 * value0 + (1.0 - h0) * value1 + step * (h1 * first0 + h4 * first1) +
           step * step * (h2 * second0 + h5 * second1);
}

/// The most nodes a channel's antiderivatives take (128 MB), far more than any frame needs.
inline constexpr double MAX_STRIP_NODES = 4.0e6;

/// Grid points the near table holds beyond its radius. B-spline taps reach two points past the
/// farthest offset read, and the coefficients near the grid's far end feel its boundary, by
/// 0.27^k at k points from it.
inline constexpr int NEAR_MARGIN = 16;

/// The most memory the near table takes before its pitch is made coarser.
inline constexpr std::size_t NEAR_TABLE_BUDGET = std::size_t{64} * 1024 * 1024;

} // namespace psf_tables_detail

/**
 * @brief Builds the tables from each channel's radial profile.
 *
 * A profile is called as profile(r) for the light per unit area at a distance r from the source
 * (in pixel widths, for a total of 1), and profile.slope(r) for its derivative. Both must be
 * safe to call from several threads. Far out, the profile must match its channel's far field:
 * far_coefficient / r^3, averaged over the rings, plus the scattered light.
 *
 * @param profiles One profile per channel.
 * @param channels What the tables need to know of each channel's profile beyond the near table.
 * @param layout The tables' shape: see Layout.
 * @throws std::runtime_error if there is not one profile and channel per spectral channel, or the
 *         layout is not positive and finite, or near_samples is odd.
 */
template <IsSpectral TSpectral>
template <class Profile>
PsfTables<TSpectral>::PsfTables(const std::vector<Profile>& profiles,
                                const std::vector<Channel>& channels,
                                const Layout& layout)
    : aspect_{layout.aspect}, near_radius_{layout.near_radius}, near_samples_{layout.near_samples},
      symmetric_{layout.aspect == 1.0}
{
    if (profiles.size() != TSpectral::size() || channels.size() != TSpectral::size()) {
        HUIRA_THROW_ERROR("PsfTables - Need one profile and channel per spectral channel (" +
                          std::to_string(TSpectral::size()) + "), not " +
                          std::to_string(profiles.size()) + " and " +
                          std::to_string(channels.size()));
    }
    if (!(layout.aspect > 0.0) || !std::isfinite(layout.aspect) ||
        !(layout.near_radius > BLEND_WIDTH) || !std::isfinite(layout.near_radius) ||
        layout.near_samples < 2 || layout.near_samples % 2 != 0 || !(layout.reach >= 0.0)) {
        HUIRA_THROW_ERROR("PsfTables - The layout must be positive and finite, with the near "
                          "table reaching beyond the blend and an even number of samples");
    }
    for (const Channel& channel : channels) {
        if (!(channel.step > 0.0) || !std::isfinite(channel.step) ||
            !(channel.far_radius >= layout.near_radius) || !(channel.far_coefficient >= 0.0) ||
            !(channel.scatter_fraction >= 0.0 && channel.scatter_fraction < 1.0) ||
            (channel.scatter_fraction > 0.0 && !channel.scatter.has_value())) {
            HUIRA_THROW_ERROR("PsfTables - Each channel needs a positive step, a far radius "
                              "beyond the near table, and a scatter profile for any light "
                              "scattered");
        }
    }

    build_near_(profiles);
    strips_.resize(TSpectral::size());
    tbb::parallel_for(std::size_t{0}, TSpectral::size(), [&](std::size_t channel) {
        strips_[channel] = build_strip_(profiles[channel], channels[channel], layout.reach);
    });
}

/**
 * @brief Tables for a circular aperture's diffraction pattern, averaged over each spectral bin's
 * wavelengths, with scattered light if given.
 *
 * The layout follows the optics. The near table's pitch puts NEAR_SAMPLES_PER_PERIOD points in
 * the pattern's finest period, lambda N at the bluest wavelength; its radius is where the
 * projected integrals come within 0.07% of exact, PROJECTION_PERIODS periods of the coarsest
 * detail (lambda N at the reddest) out and at least PROJECTION_MIN_RADIUS pixels, plus the
 * blend.
 * Each channel's far field takes over where its rings, averaged over the bin's wavelengths and
 * over a pixel, fall below 0.1% of the mean. Scattered light takes NEAR_SAMPLES_PER_SHOULDER
 * points across its shoulder, if that is finer.
 *
 * @param fnumber The aperture's f-number.
 * @param pitch_x Pixel width in meters.
 * @param pitch_y Pixel height in meters.
 * @param reach How far from a source, in pixel widths, the projected integrals are tabulated.
 * @param scatter Scattered light, in pixel widths; the same in every channel.
 * @throws std::runtime_error if the f-number or a pitch is not positive and finite, or the
 *         scattered light is invalid (see detail::ScatterProfile).
 */
template <IsSpectral TSpectral>
PsfTables<TSpectral> PsfTables<TSpectral>::airy(double fnumber,
                                                double pitch_x,
                                                double pitch_y,
                                                double reach,
                                                const std::optional<Scatter>& scatter)
{
    if (!(fnumber > 0.0) || !std::isfinite(fnumber) || !(pitch_x > 0.0) ||
        !std::isfinite(pitch_x) || !(pitch_y > 0.0) || !std::isfinite(pitch_y)) {
        HUIRA_THROW_ERROR("PsfTables::airy - The f-number and pixel pitch must be positive and "
                          "finite");
    }
    // From comparisons with the exact pixel integrals over a range of optics (f/2 to f/16, 5 to
    // 10 um pixels, Visible8 and RGB bins): cubic B-splines with 8 points to the finest period
    // keep every pixel within 0.03%; the projected integrals are within 0.07% beyond 28 periods
    // of the coarsest detail and 6 pixels; and the far field, averaged over the rings, is within
    // 0.1% beyond FAR_FIELD_RINGS A / (c_blue - c_red), where A, the most a pixel lets a ring
    // through, is 1 / (pi c_red w) for a pixel w wide along the ring's direction.
    constexpr double NEAR_SAMPLES_PER_PERIOD = 8.0;
    constexpr double PROJECTION_PERIODS = 28.0;
    constexpr double PROJECTION_MIN_RADIUS = 6.0;
    constexpr double FAR_FIELD_RINGS = 400.0;
    constexpr double STRIP_STEPS_PER_PERIOD = 4.0;
    constexpr double MAX_STRIP_STEP = 0.5;
    constexpr double NEAR_SAMPLES_PER_SHOULDER = 4.0;

    const double scatter_fraction =
        scatter.has_value() && scatter->fraction > 0.0 ? scatter->fraction : 0.0;
    std::optional<detail::ScatterProfile> scatter_profile;
    if (scatter_fraction > 0.0) {
        scatter_profile.emplace(scatter->shoulder, scatter->slope, scatter->outer);
    }

    const double aspect = pitch_y / pitch_x;
    std::vector<detail::OpticsProfile> profiles;
    profiles.reserve(TSpectral::size());
    double finest = 0.0;
    double coarsest = std::numeric_limits<double>::infinity();
    for (std::size_t channel = 0; channel < TSpectral::size(); ++channel) {
        const Bin bin = TSpectral::get_bin(channel);
        // Cutoff frequencies, in cycles per pixel width, at the bin's two ends:
        const double blue = pitch_x / (bin.min_wavelength * fnumber);
        const double red = pitch_x / (bin.max_wavelength * fnumber);
        profiles.emplace_back(
            detail::AiryBandProfile(blue, red), scatter_fraction, scatter_profile);
        finest = std::max(finest, blue);
        coarsest = std::min(coarsest, red);
    }

    Layout layout;
    layout.aspect = aspect;
    layout.reach = reach;
    layout.near_radius =
        std::ceil(std::max(PROJECTION_MIN_RADIUS, PROJECTION_PERIODS / coarsest)) + BLEND_WIDTH;
    double samples_wanted = NEAR_SAMPLES_PER_PERIOD * finest * std::max(1.0, aspect);
    if (scatter_fraction > 0.0) {
        samples_wanted = std::max(
            samples_wanted, NEAR_SAMPLES_PER_SHOULDER * std::max(1.0, aspect) / scatter->shoulder);
    }
    layout.near_samples = 2 * std::max(1, static_cast<int>(std::ceil(samples_wanted / 2.0)));

    // Very fast optics need a fine pitch over a near table that cannot shrink below
    // PROJECTION_MIN_RADIUS: past a budget, the pitch is made coarser.
    const auto near_bytes = [&](int samples) {
        const double margin = psf_tables_detail::NEAR_MARGIN;
        const double side = layout.near_radius * samples + margin;
        const double entries = aspect == 1.0
                                   ? 0.5 * side * (side + 1.0)
                                   : side * (layout.near_radius / aspect * samples + margin);
        return entries * static_cast<double>(sizeof(TSpectral));
    };
    const int wanted_samples = layout.near_samples;
    while (layout.near_samples > 8 &&
           near_bytes(layout.near_samples) >
               static_cast<double>(psf_tables_detail::NEAR_TABLE_BUDGET)) {
        layout.near_samples -= 2;
    }
    if (layout.near_samples < wanted_samples) {
        HUIRA_LOG_INFO("PsfTables - The near table samples the PSF " +
                       std::to_string(layout.near_samples) + " times per pixel rather than " +
                       std::to_string(wanted_samples) + ", to keep it within " +
                       std::to_string(psf_tables_detail::NEAR_TABLE_BUDGET >> 20) + " MiB");
    }

    std::vector<Channel> channels(TSpectral::size());
    for (std::size_t channel = 0; channel < TSpectral::size(); ++channel) {
        const double blue = profiles[channel].diffraction().cutoff_blue();
        const double red = profiles[channel].diffraction().cutoff_red();
        const double through = std::min(1.0, 1.0 / (PI<double>() * red * std::min(1.0, aspect)));
        const double rings = blue > red ? FAR_FIELD_RINGS * through / (blue - red)
                                        : std::numeric_limits<double>::infinity();
        channels[channel].far_radius = std::max(layout.near_radius + BLEND_WIDTH, rings);
        channels[channel].far_coefficient =
            (1.0 - scatter_fraction) * profiles[channel].diffraction().far_field_coefficient();
        channels[channel].scatter_fraction = scatter_fraction;
        channels[channel].scatter = scatter_profile;
        channels[channel].step = std::min(MAX_STRIP_STEP, 1.0 / (STRIP_STEPS_PER_PERIOD * blue));
    }
    return PsfTables(profiles, channels, layout);
}

/**
 * @brief Builds the near table: the light in a pixel against the offset of its center from the
 * source, every 1 / near_samples of a pixel, as cubic B-spline coefficients.
 *
 * The light in each cell between neighbouring offsets is found by 3 x 3 Gauss-Legendre (the
 * pitch is a small part of the finest period, so this is exact to about 1e-9), and summed into
 * the light of rectangles from the source; a pixel's light is then four of those, since its
 * edges fall on the grid.
 */
template <IsSpectral TSpectral>
template <class Profile>
void PsfTables<TSpectral>::build_near_(const std::vector<Profile>& profiles)
{
    using psf_tables_detail::GL3_NODES;
    using psf_tables_detail::GL3_WEIGHTS;

    using psf_tables_detail::NEAR_MARGIN;
    const int half = near_samples_ / 2;
    const double pitch = 1.0 / near_samples_;
    near_width_ = static_cast<int>(std::ceil(near_radius_ * near_samples_)) + NEAR_MARGIN;
    near_height_ =
        symmetric_
            ? near_width_
            : static_cast<int>(std::ceil(near_radius_ / aspect_ * near_samples_)) + NEAR_MARGIN;
    const auto width = static_cast<std::size_t>(near_width_);
    const auto height = static_cast<std::size_t>(near_height_);
    const auto cells_x = width - 1 + static_cast<std::size_t>(half);
    const auto cells_y = height - 1 + static_cast<std::size_t>(half);

    near_.assign(symmetric_ ? width * (width + 1) / 2 : width * height, TSpectral{0.f});
    std::vector<double> cells(cells_x * cells_y);
    std::vector<double> corner((cells_x + 1) * (cells_y + 1), 0.0);
    std::vector<double> grid(width * height);

    for (std::size_t channel = 0; channel < TSpectral::size(); ++channel) {
        const Profile& profile = profiles[channel];

        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(0, cells_y),
            [&](const tbb::blocked_range<std::size_t>& rows) {
                for (std::size_t j = rows.begin(); j != rows.end(); ++j) {
                    // A symmetric table needs each cell below the diagonal only once.
                    for (std::size_t i = symmetric_ ? j : 0; i < cells_x; ++i) {
                        double sum = 0.0;
                        for (std::size_t a = 0; a < GL3_NODES.size(); ++a) {
                            const double x =
                                (static_cast<double>(i) + 0.5 + 0.5 * GL3_NODES[a]) * pitch;
                            for (std::size_t b = 0; b < GL3_NODES.size(); ++b) {
                                const double y =
                                    (static_cast<double>(j) + 0.5 + 0.5 * GL3_NODES[b]) * pitch;
                                sum += GL3_WEIGHTS[a] * GL3_WEIGHTS[b] *
                                       profile(std::hypot(x, aspect_ * y));
                            }
                        }
                        cells[j * cells_x + i] = 0.25 * pitch * pitch * aspect_ * sum;
                    }
                }
            });
        if (symmetric_) {
            for (std::size_t j = 0; j < cells_y; ++j) {
                for (std::size_t i = 0; i < j; ++i) {
                    cells[j * cells_x + i] = cells[i * cells_x + j];
                }
            }
        }

        // corner[(j, i)]: light in [0, i] x [0, j] pitches from the source.
        const std::size_t stride = cells_x + 1;
        for (std::size_t j = 0; j < cells_y; ++j) {
            for (std::size_t i = 0; i < cells_x; ++i) {
                corner[(j + 1) * stride + i + 1] =
                    cells[j * cells_x + i] + corner[j * stride + i + 1] +
                    corner[(j + 1) * stride + i] - corner[j * stride + i];
            }
        }
        // The light in [0, a] x [0, b] for offsets of either sign: the PSF is symmetric about
        // each axis, so a rectangle across an axis holds the sum of its two sides.
        const auto corner_light = [&](long a, long b) {
            const double sign = (a < 0) == (b < 0) ? 1.0 : -1.0;
            return sign * corner[static_cast<std::size_t>(std::abs(b)) * stride +
                                 static_cast<std::size_t>(std::abs(a))];
        };
        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(0, height),
            [&](const tbb::blocked_range<std::size_t>& rows) {
                for (std::size_t j = rows.begin(); j != rows.end(); ++j) {
                    const long y = static_cast<long>(j);
                    for (std::size_t i = 0; i < width; ++i) {
                        const long x = static_cast<long>(i);
                        grid[j * width + i] =
                            corner_light(x + half, y + half) - corner_light(x - half, y + half) -
                            corner_light(x + half, y - half) + corner_light(x - half, y - half);
                    }
                    psf_tables_detail::bspline_coefficients(&grid[j * width], width, 1);
                }
            });
        tbb::parallel_for(tbb::blocked_range<std::size_t>(0, width),
                          [&](const tbb::blocked_range<std::size_t>& columns) {
                              for (std::size_t i = columns.begin(); i != columns.end(); ++i) {
                                  psf_tables_detail::bspline_coefficients(&grid[i], height, width);
                              }
                          });

        for (std::size_t j = 0; j < height; ++j) {
            for (std::size_t i = symmetric_ ? j : 0; i < width; ++i) {
                near_[near_index_(static_cast<int>(i), static_cast<int>(j))][channel] =
                    static_cast<float>(grid[j * width + i]);
            }
        }
    }
}

/**
 * @brief Builds one channel's antiderivatives: F1(r), the profile's integral from r to
 * infinity, and F2(r), F1's.
 *
 * They are taken to infinity, rather than from the source, so that they shrink with r as the
 * profile does and keep their relative precision far out. Beyond the last node the profile is
 * taken as its far field; any error in that is a constant in F1 and a straight line in F2, which
 * the projected integrals do not see.
 */
template <IsSpectral TSpectral>
template <class Profile>
typename PsfTables<TSpectral>::Strip PsfTables<TSpectral>::build_strip_(const Profile& profile,
                                                                        const Channel& channel,
                                                                        double reach) const
{
    Strip strip;
    strip.step = channel.step;
    strip.far_radius = channel.far_radius;
    strip.far_coefficient = channel.far_coefficient;
    strip.scatter_fraction = channel.scatter_fraction;
    strip.scatter = channel.scatter;
    strip.limit = std::min(channel.far_radius, std::max(reach, near_radius_));

    // A pixel's projected integral reads the antiderivatives within half its projected width of
    // its distance (at most half its diagonal), moved out by the curvature correction (at most
    // (1 + aspect^2) / (24 r)).
    const double half_diagonal = 0.5 * std::hypot(1.0, aspect_);
    const double first = near_radius_ - BLEND_WIDTH;
    const double curvature = (1.0 + aspect_ * aspect_) / (24.0 * first);
    strip.start = std::max(0.0, first - half_diagonal - 2.0 * strip.step);
    const double end = strip.limit + half_diagonal + curvature + 2.0 * strip.step;
    const double count_wanted = std::ceil((end - strip.start) / strip.step) + 1.0;
    if (count_wanted > psf_tables_detail::MAX_STRIP_NODES) {
        HUIRA_THROW_ERROR("PsfTables - The projected integrals would need " +
                          std::to_string(count_wanted) +
                          " nodes; the reach or the PSF's detail is out of range");
    }
    const auto count = static_cast<std::size_t>(count_wanted);
    strip.nodes.resize(count);

    // Each node's profile and slope, and the integrals over the interval above it of the
    // profile, and of the profile times the distance from the node.
    std::vector<std::array<double, 2>> pieces(count);
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t k) {
        const double r = strip.start + static_cast<double>(k) * strip.step;
        strip.nodes[k][2] = profile(r);
        strip.nodes[k][3] = profile.slope(r);
        if (k + 1 < count) {
            const double mid = r + 0.5 * strip.step;
            const double half = 0.5 * strip.step;
            double light = 0.0;
            double moment = 0.0;
            for (std::size_t i = 0; i < detail::airy_band_detail::GL8_NODES.size(); ++i) {
                const double offset = half * detail::airy_band_detail::GL8_NODES[i];
                const double weight = half * detail::airy_band_detail::GL8_WEIGHTS[i];
                const double below = profile(mid - offset);
                const double above = profile(mid + offset);
                light += weight * (below + above);
                moment += weight * (below * (half - offset) + above * (half + offset));
            }
            pieces[k] = {light, moment};
        }
    });

    // Summed downward from the diffraction far field's tails, kappa / (2 r^2) and kappa / (2 r),
    // with Neumaier's compensation. Scattered light's tails are left out: a constant in F1 and a
    // straight line in F2 drop out of the differences the projected integrals take.
    const double last = strip.start + static_cast<double>(count - 1) * strip.step;
    double f1 = channel.far_coefficient / (2.0 * last * last);
    double f2 = channel.far_coefficient / (2.0 * last);
    double f1_carry = 0.0;
    double f2_carry = 0.0;
    const auto add = [](double& sum, double& carry, double value) {
        const double next = sum + value;
        carry += std::abs(sum) >= std::abs(value) ? (sum - next) + value : (value - next) + sum;
        sum = next;
    };
    strip.nodes[count - 1][0] = f2;
    strip.nodes[count - 1][1] = f1;
    for (std::size_t k = count - 1; k-- > 0;) {
        // F2(r) - F2(r + step) is the integral of F1 over the interval: step F1(r + step), plus
        // the profile's moment about r.
        add(f2, f2_carry, strip.step * (f1 + f1_carry) + pieces[k][1]);
        add(f1, f1_carry, pieces[k][0]);
        strip.nodes[k][0] = f2 + f2_carry;
        strip.nodes[k][1] = f1 + f1_carry;
    }
    return strip;
}

/// Index of the near table's entry at grid point (i, j), reflected into the first quadrant
/// (and, for square pixels, below the diagonal).
template <IsSpectral TSpectral>
std::size_t PsfTables<TSpectral>::near_index_(int i, int j) const
{
    auto x = static_cast<std::size_t>(std::abs(i));
    auto y = static_cast<std::size_t>(std::abs(j));
    if (symmetric_) {
        if (y > x) {
            std::swap(x, y);
        }
        return x * (x + 1) / 2 + y;
    }
    return y * static_cast<std::size_t>(near_width_) + x;
}

/// The near table's value for a pixel whose center is (dx, dy) from the source.
template <IsSpectral TSpectral>
TSpectral PsfTables<TSpectral>::near_value_(double dx, double dy) const
{
    const double u = std::abs(dx) * near_samples_;
    const double v = std::abs(dy) * near_samples_;
    const int i = static_cast<int>(u);
    const int j = static_cast<int>(v);
    const std::array<double, 4> wx = psf_tables_detail::bspline_weights(u - i);
    const std::array<double, 4> wy = psf_tables_detail::bspline_weights(v - j);
    std::array<double, TSpectral::size()> sum{};
    for (int b = 0; b < 4; ++b) {
        for (int a = 0; a < 4; ++a) {
            const double weight = wx[static_cast<std::size_t>(a)] * wy[static_cast<std::size_t>(b)];
            const TSpectral& coefficient = near_[near_index_(i - 1 + a, j - 1 + b)];
            for (std::size_t channel = 0; channel < TSpectral::size(); ++channel) {
                sum[channel] += weight * static_cast<double>(coefficient[channel]);
            }
        }
    }
    TSpectral result{0.f};
    for (std::size_t channel = 0; channel < TSpectral::size(); ++channel) {
        result[channel] = static_cast<float>(sum[channel]);
    }
    return result;
}

/**
 * @brief One channel's light in a pixel beyond the near table: the projected integral, then the
 * far field, blended across the far radius.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::outer_value_(const Strip& strip, double dx, double dy, double r) const
{
    if (r >= strip.limit) {
        return far_value_(strip, dx, dy, r);
    }
    const double projected = strip_value_(strip, dx, dy, r);
    const double blend_from = strip.far_radius - BLEND_WIDTH;
    if (r <= blend_from) {
        return projected;
    }
    const double weight = psf_tables_detail::smooth_step((r - blend_from) / BLEND_WIDTH);
    return (1.0 - weight) * projected + weight * far_value_(strip, dx, dy, r);
}

/**
 * @brief One channel's light in a pixel from the profile integrated over the pixel's projection
 * on the line from the source.
 *
 * Seen from far enough away, the circles of equal light cross the pixel as nearly straight
 * lines, so its light is the profile weighted by the pixel's projection on the line from the
 * source: a trapezoid, the convolution of the two boxes its sides project to. Two antiderivatives
 * of the profile make that a second difference. The circles' curvature moves their mean across
 * the pixel out by the mean square of the distance across, over 2r: that is where the profile
 * is taken.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::strip_value_(const Strip& strip, double dx, double dy, double r) const
{
    const double cosine = dx / r;
    const double sine = aspect_ * dy / r;
    const double side_x = std::abs(cosine);
    const double side_y = aspect_ * std::abs(sine);
    const double wide = std::max(side_x, side_y);
    const double narrow = std::min(side_x, side_y);
    const double center = r + (sine * sine + aspect_ * aspect_ * cosine * cosine) / (24.0 * r);

    // Position of a distance among the nodes.
    const auto locate = [&](double distance, std::size_t& k, double& t) {
        const double u = (distance - strip.start) / strip.step;
        const double last = static_cast<double>(strip.nodes.size() - 2);
        const double clamped = std::clamp(u, 0.0, last + 1.0);
        k = static_cast<std::size_t>(std::min(std::floor(clamped), last));
        t = clamped - static_cast<double>(k);
    };
    // F2, with F2' = -F1 and F2'' = the profile.
    const auto second = [&](double distance) {
        std::size_t k = 0;
        double t = 0.0;
        locate(distance, k, t);
        const auto& a = strip.nodes[k];
        const auto& b = strip.nodes[k + 1];
        return psf_tables_detail::quintic_hermite(
            t, strip.step, a[0], -a[1], a[2], b[0], -b[1], b[2]);
    };
    // F1, with F1' = -the profile and F1'' = -its slope.
    const auto first = [&](double distance) {
        std::size_t k = 0;
        double t = 0.0;
        locate(distance, k, t);
        const auto& a = strip.nodes[k];
        const auto& b = strip.nodes[k + 1];
        return psf_tables_detail::quintic_hermite(
            t, strip.step, a[1], -a[2], -a[3], b[1], -b[2], -b[3]);
    };
    const auto slope = [&](double distance) {
        std::size_t k = 0;
        double t = 0.0;
        locate(distance, k, t);
        return (1.0 - t) * strip.nodes[k][3] + t * strip.nodes[k + 1][3];
    };

    // Below this the trapezoid is nearly a box, and its second difference would lose digits:
    // the box's first difference, with its leading correction, is then within about 1e-6.
    constexpr double NARROW = 0.02;
    if (narrow >= NARROW) {
        const double outer = 0.5 * (wide + narrow);
        const double inner = 0.5 * (wide - narrow);
        return aspect_ *
               (second(center + outer) - second(center + inner) - second(center - inner) +
                second(center - outer)) /
               (wide * narrow);
    }
    const double near_edge = center - 0.5 * wide;
    const double far_edge = center + 0.5 * wide;
    return aspect_ *
           (first(near_edge) - first(far_edge) +
            narrow * narrow / 24.0 * (slope(far_edge) - slope(near_edge))) /
           wide;
}

/**
 * @brief One channel's light in a pixel from the profile's far field: the diffraction pattern's
 * kappa / r^3, averaged over its rings, and the scattered light, both averaged over the pixel to
 * second order. Averaging over a side w adds w^2 / 24 of the second derivative along it; for a
 * radial f(r) that is f'' cos^2 + f' sin^2 / r along x, and likewise along y.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::far_value_(const Strip& strip, double dx, double dy, double r) const
{
    const double cosine = dx / r;
    const double sine = aspect_ * dy / r;
    const double cos2 = cosine * cosine;
    const double sin2 = sine * sine;
    const double aspect2 = aspect_ * aspect_;
    const double r2 = r * r;
    // (d2/dx2 + aspect^2 d2/dy2) of r^-3, over r^-3, times r^2:
    const double laplacian = (15.0 * cos2 - 3.0) + aspect2 * (15.0 * sin2 - 3.0);
    double value = strip.far_coefficient / (r2 * r) * (1.0 + laplacian / (24.0 * r2));
    if (strip.scatter_fraction > 0.0) {
        const auto [s, first, second] = strip.scatter->derivatives(r);
        const double along_x = second * cos2 + first * sin2 / r;
        const double along_y = second * sin2 + first * cos2 / r;
        value += strip.scatter_fraction * (s + (along_x + aspect2 * along_y) / 24.0);
    }
    return aspect_ * value;
}

/**
 * @brief The light of a source in the pixel whose center is (dx, dy) pixels from it, per
 * channel, for a source whose light totals 1 in each channel.
 */
template <IsSpectral TSpectral>
TSpectral PsfTables<TSpectral>::pixel(double dx, double dy) const
{
    const double r = std::hypot(dx, aspect_ * dy);
    const double blend_from = near_radius_ - BLEND_WIDTH;
    if (r <= blend_from) {
        return near_value_(dx, dy);
    }
    const double weight =
        r < near_radius_ ? psf_tables_detail::smooth_step((r - blend_from) / BLEND_WIDTH) : 1.0;
    const TSpectral near = weight < 1.0 ? near_value_(dx, dy) : TSpectral{0.f};
    TSpectral result{0.f};
    for (std::size_t channel = 0; channel < TSpectral::size(); ++channel) {
        const double outer = outer_value_(strips_[channel], dx, dy, r);
        result[channel] = static_cast<float>((1.0 - weight) * static_cast<double>(near[channel]) +
                                             weight * outer);
    }
    return result;
}

/**
 * @brief The light of a source in each pixel of a square around it.
 *
 * @param radius The square is 2 radius + 1 pixels on a side.
 * @param x_offset The source's position right of the center pixel's center, in pixels.
 * @param y_offset Its position below the center pixel's center.
 */
template <IsSpectral TSpectral>
Image<TSpectral> PsfTables<TSpectral>::image(int radius, double x_offset, double y_offset) const
{
    const int size = 2 * radius + 1;
    Image<TSpectral> result(size, size, TSpectral{0.f});
    tbb::parallel_for(0, size, [&](int y) {
        for (int x = 0; x < size; ++x) {
            result(x, y) = pixel(static_cast<double>(x - radius) - x_offset,
                                 static_cast<double>(y - radius) - y_offset);
        }
    });
    return result;
}

/// Memory the tables take, in bytes.
template <IsSpectral TSpectral>
std::size_t PsfTables<TSpectral>::memory_bytes() const
{
    std::size_t bytes = near_.size() * sizeof(TSpectral);
    for (const Strip& strip : strips_) {
        bytes += strip.nodes.size() * sizeof(strip.nodes[0]);
    }
    return bytes;
}

} // namespace huira
