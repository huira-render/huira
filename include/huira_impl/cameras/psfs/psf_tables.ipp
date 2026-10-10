#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "huira/cameras/psfs/airy_band.hpp"
#include "huira/cameras/psfs/psf_profile.hpp"
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

/// A positive radius's exponent and leading bits of its mantissa, which grow with it: slices of
/// the radii, each 2^-bits of its own scale wide.
inline std::uint64_t radius_key(double r, int bits)
{
    return std::bit_cast<std::uint64_t>(r) >> (52 - bits);
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
 * safe to call from several threads. profile.far_field(r) gives the profile far out, averaged
 * over any rings, with its first two derivatives; it is used beyond the channel's far radius.
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
            !(channel.far_radius >= layout.near_radius)) {
            HUIRA_THROW_ERROR("PsfTables - Each channel needs a positive step, and a far radius "
                              "beyond the near table");
        }
    }

    build_near_(profiles);

    // The grid all channels share: the finest of their steps, from where the near table hands
    // over to beyond where the farthest strip ends, and their far fields from where the first
    // holds (see smooth_profile()). A pixel's projected integral reads the antiderivatives within
    // half its projected width of its distance (at most half its diagonal), moved out by the
    // curvature correction (at most (1 + aspect^2) / (24 r)).
    constexpr double SMOOTH_FROM = 1.0;
    const std::size_t channel_count = TSpectral::size();
    grid_.step = std::numeric_limits<double>::infinity();
    grid_.far_start = std::numeric_limits<double>::infinity();
    double last_limit = 0.0;
    for (std::size_t channel = 0; channel < channel_count; ++channel) {
        grid_.step = std::min(grid_.step, channels[channel].step);
        last_limit =
            std::max(last_limit,
                     std::min(channels[channel].far_radius, std::max(layout.reach, near_radius_)));
        grid_.far_start =
            std::min(grid_.far_start,
                     std::min(std::max(channels[channel].far_radius - BLEND_WIDTH - 1.0, 0.5),
                              std::max(SMOOTH_FROM, profiles[channel].smooth_from())));
    }
    const double half_diagonal = 0.5 * std::hypot(1.0, aspect_);
    const double first = near_radius_ - BLEND_WIDTH;
    const double curvature = (1.0 + aspect_ * aspect_) / (24.0 * first);
    grid_.start = std::max(0.0, first - half_diagonal - 2.0 * grid_.step);
    const double end = last_limit + half_diagonal + curvature + 2.0 * grid_.step;
    const double count_wanted = std::ceil((end - grid_.start) / grid_.step) + 1.0;
    if (count_wanted > psf_tables_detail::MAX_STRIP_NODES) {
        HUIRA_THROW_ERROR("PsfTables - The projected integrals would need " +
                          std::to_string(count_wanted) +
                          " nodes; the reach or the PSF's detail is out of range");
    }
    grid_.count = static_cast<std::size_t>(count_wanted);
    grid_.far_end = std::max(layout.reach, 2.0 * grid_.far_start);

    strips_.resize(channel_count);
    tbb::parallel_for(std::size_t{0}, channel_count, [&](std::size_t channel) {
        strips_[channel] = build_strip_(profiles[channel], channels[channel], layout.reach);
    });

    // Channel by channel at each node, and the per-channel nodes released:
    strip_nodes_.resize(grid_.count * channel_count * 4);
    far_radii_ = strips_[0].far.radii();
    far_inverse_steps_.resize(far_radii_.size() - 1);
    for (std::size_t k = 0; k + 1 < far_radii_.size(); ++k) {
        far_inverse_steps_[k] = 1.0 / (far_radii_[k + 1] - far_radii_[k]);
    }
    far_key_start_ = psf_tables_detail::radius_key(far_radii_.front(), FAR_KEY_BITS);
    far_index_.resize(psf_tables_detail::radius_key(far_radii_.back(), FAR_KEY_BITS) -
                      far_key_start_ + 1);
    for (std::size_t key = 0, k = 0; key < far_index_.size(); ++key) {
        const double slice_start =
            std::bit_cast<double>((far_key_start_ + key) << (52 - FAR_KEY_BITS));
        while (k + 2 < far_radii_.size() && far_radii_[k + 1] <= slice_start) {
            ++k;
        }
        far_index_[key] = static_cast<std::uint32_t>(k);
    }
    std::vector<double> far_nodes(far_radii_.size() * channel_count * 3);
    for (std::size_t channel = 0; channel < channel_count; ++channel) {
        Strip& strip = strips_[channel];
        for (std::size_t k = 0; k < grid_.count; ++k) {
            for (std::size_t j = 0; j < 4; ++j) {
                strip_nodes_[(k * channel_count + channel) * 4 + j] = strip.nodes[k][j];
            }
        }
        const auto& far = strip.far.nodes();
        for (std::size_t k = 0; k < far.size(); ++k) {
            for (std::size_t j = 0; j < 3; ++j) {
                far_nodes[(k * channel_count + channel) * 3 + j] = far[k][j];
            }
        }
        strip_limit_[channel] = strip.limit;
        far_blend_from_[channel] = strip.far_radius - BLEND_WIDTH;
        smooth_from_[channel] = std::max(strip.far.start(), strip.smooth_from);
        far_from_ = std::max(far_from_, std::min(strip.limit, strip.far_radius));
        smooth_from_all_ = std::max(smooth_from_all_, smooth_from_[channel]);
        strip.nodes = {};
    }
    build_far_basis_(far_nodes);

    radials_.resize(channel_count);
    tbb::parallel_for(std::size_t{0}, TSpectral::size(), [&](std::size_t channel) {
        radials_[channel] = build_radial_(profiles[channel], strips_[channel]);
    });
    build_ring_deviation_();
    build_lines_(profiles);
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
 * A blur, defocus's uniform disc, widens the near table by up to MAX_NEAR_BLUR, and moves each
 * channel's far field out to where the blurred profile is smooth: see detail::DefocusedProfile.
 *
 * @param fnumber The aperture's f-number.
 * @param pitch_x Pixel width in meters.
 * @param pitch_y Pixel height in meters.
 * @param reach How far from a source, in pixel widths, the projected integrals are tabulated.
 * @param scatter Scattered light, in pixel widths; the same in every channel.
 * @param blur The radius of defocus's blur disc in pixel widths; below MIN_BLUR, in focus.
 * @throws std::runtime_error if the f-number or a pitch is not positive and finite, the blur is
 *         negative or not finite, or the scattered light is invalid (see
 *         detail::ScatterProfile).
 */
template <IsSpectral TSpectral>
PsfTables<TSpectral> PsfTables<TSpectral>::airy(double fnumber,
                                                double pitch_x,
                                                double pitch_y,
                                                double reach,
                                                const std::optional<Scatter>& scatter,
                                                double blur)
{
    if (!(fnumber > 0.0) || !std::isfinite(fnumber) || !(pitch_x > 0.0) ||
        !std::isfinite(pitch_x) || !(pitch_y > 0.0) || !std::isfinite(pitch_y) || !(blur >= 0.0) ||
        !std::isfinite(blur)) {
        HUIRA_THROW_ERROR("PsfTables::airy - The f-number and pixel pitch must be positive and "
                          "finite, and the blur non-negative and finite");
    }
    const bool defocused = blur >= MIN_BLUR;
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
    layout.near_radius = std::ceil(std::max(PROJECTION_MIN_RADIUS, PROJECTION_PERIODS / coarsest) +
                                   (defocused ? std::min(blur, MAX_NEAR_BLUR) : 0.0)) +
                         BLEND_WIDTH;
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
        channels[channel].step = std::min(MAX_STRIP_STEP, 1.0 / (STRIP_STEPS_PER_PERIOD * blue));
    }
    if (!defocused) {
        return PsfTables(profiles, channels, layout);
    }

    // Each channel blurred, from its rings out to where they have washed out; the blurred
    // profile is smooth beyond the disc's edge plus the radius its rings are followed to.
    std::vector<std::optional<detail::DefocusedProfile>> blurred(TSpectral::size());
    tbb::parallel_for(std::size_t{0}, TSpectral::size(), [&](std::size_t channel) {
        blurred[channel].emplace(
            profiles[channel], blur, channels[channel].far_radius, std::max(reach, 1.0));
    });
    std::vector<detail::DefocusedProfile> defocused_profiles;
    defocused_profiles.reserve(TSpectral::size());
    for (std::size_t channel = 0; channel < TSpectral::size(); ++channel) {
        defocused_profiles.push_back(*blurred[channel]);
        channels[channel].far_radius = std::max(
            layout.near_radius + BLEND_WIDTH, blur + blurred[channel]->ring_radius() + BLEND_WIDTH);
    }
    return PsfTables(defocused_profiles, channels, layout);
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
    strip.far_radius = channel.far_radius;
    strip.limit = std::min(channel.far_radius, std::max(reach, near_radius_));
    strip.smooth_from = profile.smooth_from();

    // The far field, tabulated every 1% in r on the shared grid; beyond its end it continues
    // as a power law.
    constexpr double FAR_RATIO = 1.01;
    strip.far = detail::RadialTable::logarithmic(
        [&](double r) { return profile.far_field(r); }, grid_.far_start, grid_.far_end, FAR_RATIO);

    const std::size_t count = grid_.count;
    const double step = grid_.step;
    strip.nodes.resize(count);

    // Each node's profile and slope, and the integrals over the interval above it of the
    // profile, and of the profile times the distance from the node.
    std::vector<std::array<double, 2>> pieces(count);
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t k) {
        const double r = grid_.start + static_cast<double>(k) * step;
        strip.nodes[k][2] = profile(r);
        strip.nodes[k][3] = profile.slope(r);
        if (k + 1 < count) {
            const double mid = r + 0.5 * step;
            const double half = 0.5 * step;
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

    // Summed downward with Neumaier's compensation, from 0 at the last node: what lies beyond is
    // a constant in F1 and a straight line in F2, which drop out of the differences the
    // projected integrals take.
    double f1 = 0.0;
    double f2 = 0.0;
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
        add(f2, f2_carry, step * (f1 + f1_carry) + pieces[k][1]);
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
    // In single precision, which holds the coefficients, a row at a time: the channels side by
    // side, as vectors.
    constexpr std::size_t N = TSpectral::size();
    std::array<float, N> sum{};
    for (int b = 0; b < 4; ++b) {
        std::array<float, N> row{};
        for (int a = 0; a < 4; ++a) {
            const auto weight = static_cast<float>(wx[static_cast<std::size_t>(a)]);
            const TSpectral& coefficient = near_[near_index_(i - 1 + a, j - 1 + b)];
            for (std::size_t channel = 0; channel < N; ++channel) {
                row[channel] += weight * coefficient[channel];
            }
        }
        const auto weight = static_cast<float>(wy[static_cast<std::size_t>(b)]);
        for (std::size_t channel = 0; channel < N; ++channel) {
            sum[channel] += weight * row[channel];
        }
    }
    TSpectral result{0.f};
    for (std::size_t channel = 0; channel < N; ++channel) {
        result[channel] = sum[channel];
    }
    return result;
}

/**
 * @brief Every channel's light in a pixel from the profile integrated over the pixel's projection
 * on the line from the source.
 *
 * Seen from far enough away, the circles of equal light cross the pixel as nearly straight
 * lines, so its light is the profile weighted by the pixel's projection on the line from the
 * source: a trapezoid, the convolution of the two boxes its sides project to. Two antiderivatives
 * of the profile make that a second difference. The circles' curvature moves their mean across
 * the pixel out by the mean square of the distance across, over 2r: that is where the profile
 * is taken. The channels share the nodes, so each distance is placed among them once.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::strip_values_(double dx,
                                         double dy,
                                         double r,
                                         const std::array<bool, TSpectral::size()>& wanted,
                                         Values& values) const
{
    constexpr std::size_t N = TSpectral::size();
    const double cosine = dx / r;
    const double sine = aspect_ * dy / r;
    const double side_x = std::abs(cosine);
    const double side_y = aspect_ * std::abs(sine);
    const double wide = std::max(side_x, side_y);
    const double narrow = std::min(side_x, side_y);
    const double center = r + (sine * sine + aspect_ * aspect_ * cosine * cosine) / (24.0 * r);
    const double step = grid_.step;
    const double last = static_cast<double>(grid_.count - 2);

    // Adds sign times F2 at a distance (with F2' = -F1, F2'' = the profile) to every channel's
    // value, or with first, F1 (with F1' = -the profile, F1'' = -its slope).
    const auto add = [&](double distance, double sign, bool first) {
        const double u = std::clamp((distance - grid_.start) / step, 0.0, last + 1.0);
        const auto k = static_cast<std::size_t>(std::min(std::floor(u), last));
        const double t = u - static_cast<double>(k);
        const double t2 = t * t;
        const double t3 = t2 * t;
        const double h0 = 1.0 - t3 * (10.0 - 15.0 * t + 6.0 * t2);
        const double h1 = step * (t - t3 * (6.0 - 8.0 * t + 3.0 * t2));
        const double h2 = step * step * 0.5 * t2 * (1.0 - 3.0 * t + 3.0 * t2 - t3);
        const double h4 = step * t3 * (-4.0 + 7.0 * t - 3.0 * t2);
        const double h5 = step * step * 0.5 * t3 * (1.0 - 2.0 * t + t2);
        const double* a = &strip_nodes_[k * N * 4];
        const double* b = a + N * 4;
        for (std::size_t c = 0; c < N; ++c, a += 4, b += 4) {
            if (!wanted[c]) {
                continue;
            }
            const double value =
                first
                    ? h0 * a[1] + (1.0 - h0) * b[1] - h1 * a[2] - h4 * b[2] - h2 * a[3] - h5 * b[3]
                    : h0 * a[0] + (1.0 - h0) * b[0] - h1 * a[1] - h4 * b[1] + h2 * a[2] + h5 * b[2];
            values[c] += sign * value;
        }
    };

    values.fill(0.0);
    // Below this the trapezoid is nearly a box, and its second difference would lose digits:
    // the box's first difference, with its leading correction, is then within about 1e-6.
    constexpr double NARROW = 0.02;
    if (narrow >= NARROW) {
        const double outer = 0.5 * (wide + narrow);
        const double inner = 0.5 * (wide - narrow);
        add(center + outer, 1.0, false);
        add(center + inner, -1.0, false);
        add(center - inner, -1.0, false);
        add(center - outer, 1.0, false);
        const double scale = aspect_ / (wide * narrow);
        for (double& value : values) {
            value *= scale;
        }
        return;
    }
    const double near_edge = center - 0.5 * wide;
    const double far_edge = center + 0.5 * wide;
    add(near_edge, 1.0, true);
    add(far_edge, -1.0, true);
    // The correction: narrow^2 / 24 times the slope's difference across, linear between nodes.
    const auto slope = [&](double distance, std::size_t c) {
        const double u = std::clamp((distance - grid_.start) / step, 0.0, last + 1.0);
        const auto k = static_cast<std::size_t>(std::min(std::floor(u), last));
        const double t = u - static_cast<double>(k);
        return (1.0 - t) * strip_nodes_[(k * N + c) * 4 + 3] +
               t * strip_nodes_[((k + 1) * N + c) * 4 + 3];
    };
    const double correction = narrow * narrow / 24.0;
    for (std::size_t c = 0; c < N; ++c) {
        if (wanted[c]) {
            values[c] = aspect_ *
                        (values[c] + correction * (slope(far_edge, c) - slope(near_edge, c))) /
                        wide;
        }
    }
}

/**
 * @brief The far fields' shapes (see far_rank_) at a pixel whose center is (dx, dy) from the
 * source, r < far_radii_.back(), averaged over the pixel to second order. Averaging over a side
 * w adds w^2 / 24 of the second derivative along it; for a radial f(r) that is
 * f'' cos^2 + f' sin^2 / r along x, and likewise along y.
 *
 * Light spread along a line about the source adds, likewise, spread times the second derivative
 * along the line, with along2 the squared cosine of the angle between the line and the pixel's
 * direction: spread is half the light's second moment along the line over the light.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::far_shapes_(
    double dx, double dy, double r, Values& shapes, double spread, double along2) const
{
    // One division, for the many pixels this is called for:
    const double inverse_r2 = 1.0 / (r * r);
    const double aspect2 = aspect_ * aspect_;
    const double cos2 = dx * dx * inverse_r2;
    const double sin2 = aspect2 * dy * dy * inverse_r2;
    const double by_second = aspect_ * ((cos2 + aspect2 * sin2) / 24.0 + spread * along2);
    const double by_first =
        aspect_ * ((sin2 + aspect2 * cos2) / 24.0 + spread * (1.0 - along2)) * r * inverse_r2;

    const std::size_t k = far_node_(r);
    const double h = far_radii_[k + 1] - far_radii_[k];
    const double t = (r - far_radii_[k]) * far_inverse_steps_[k];
    const double t2 = t * t;
    const double t3 = t2 * t;
    // Quintic Hermite weights of each end's value, slope and curvature. The derivatives enter
    // only the average over the pixel, a correction of 0.5 / r^2 of the value for an r^-3 fall,
    // and are interpolated linearly: within 0.1% of it, as they change by 5% from node to node.
    const double h0 = aspect_ * (1.0 - t3 * (10.0 - 15.0 * t + 6.0 * t2));
    const double h3 = aspect_ - h0;
    const double h1 = aspect_ * h * (t - t3 * (6.0 - 8.0 * t + 3.0 * t2));
    const double h2 = aspect_ * h * h * 0.5 * t2 * (1.0 - 3.0 * t + 3.0 * t2 - t2 * t);
    const double h4 = aspect_ * h * t3 * (-4.0 + 7.0 * t - 3.0 * t2);
    const double h5 = aspect_ * h * h * 0.5 * t3 * (1.0 - 2.0 * t + t2);
    const double first0 = by_first * (1.0 - t);
    const double first1 = by_first * t;
    const double second0 = by_second * (1.0 - t);
    const double second1 = by_second * t;
    const double* a = &far_basis_[k * far_rank_ * 3];
    const double* b = a + far_rank_ * 3;
    for (std::size_t i = 0; i < far_rank_; ++i, a += 3, b += 3) {
        shapes[i] = (h0 * a[0] + h3 * b[0]) + (h1 + first0) * a[1] + (h4 + first1) * b[1] +
                    (h2 + second0) * a[2] + (h5 + second1) * b[2];
    }
}

/// The far fields' node at or below r, for r below the last.
template <IsSpectral TSpectral>
std::size_t PsfTables<TSpectral>::far_node_(double r) const
{
    const std::uint64_t key = psf_tables_detail::radius_key(r, FAR_KEY_BITS);
    std::size_t k = key <= far_key_start_ ? 0 : far_index_[key - far_key_start_];
    if (k + 2 < far_radii_.size() && r >= far_radii_[k + 1]) {
        ++k;
    }
    return k;
}

/**
 * @brief Every channel's profile at a distance r, averaged over its rings from where the near
 * table starts to blend out (see smooth_profile()); where the far fields hold, from their shapes.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::smooth_profiles_(double r, Values& values) const
{
    constexpr std::size_t N = TSpectral::size();
    if (r < near_radius_ - BLEND_WIDTH) {
        for (std::size_t c = 0; c < N; ++c) {
            values[c] = profile(c, r);
        }
        return;
    }
    if (r < smooth_from_all_ || r >= far_radii_.back()) {
        for (std::size_t c = 0; c < N; ++c) {
            values[c] = smooth_profile(c, r);
        }
        return;
    }
    const std::size_t k = far_node_(r);
    const double h = far_radii_[k + 1] - far_radii_[k];
    const double t = (r - far_radii_[k]) / h;
    Values shapes;
    const double* a = &far_basis_[k * far_rank_ * 3];
    const double* b = a + far_rank_ * 3;
    for (std::size_t i = 0; i < far_rank_; ++i, a += 3, b += 3) {
        shapes[i] = psf_tables_detail::quintic_hermite(t, h, a[0], a[1], a[2], b[0], b[1], b[2]);
    }
    combine_far_(shapes, values);
}

/**
 * @brief The integral over r from lo to hi of each channel's profile times weight(r), with the
 * profile's rings followed out to where the near table starts to blend out, and averaged over
 * beyond, which a smooth weight sees the same way. That is at least 26 periods of the pattern's
 * coarsest detail out, where the average over the rings leaves out under 10^-5 of the light.
 *
 * Four-point Gauss-Legendre panels, a twentieth of a pixel wide among the rings and beyond
 * growing by a ratio of 1.2, split at breaks: where the weight has kinks.
 */
template <IsSpectral TSpectral>
template <class Weight>
std::array<double, TSpectral::size()> PsfTables<TSpectral>::integrate(double lo,
                                                                      double hi,
                                                                      std::vector<double> breaks,
                                                                      const Weight& weight) const
{
    constexpr std::size_t N = TSpectral::size();
    constexpr std::array<double, 4> GL4_NODES{
        -0.8611363115940526, -0.3399810435848563, 0.3399810435848563, 0.8611363115940526};
    constexpr std::array<double, 4> GL4_WEIGHTS{
        0.3478548451374538, 0.6521451548625461, 0.6521451548625461, 0.3478548451374538};
    constexpr double PANEL_RATIO = 1.2;
    constexpr double RING_PANEL = 0.05;
    const double rings_within = near_radius_ - BLEND_WIDTH;
    Values result{};
    breaks.push_back(lo);
    breaks.push_back(hi);
    if (lo < rings_within && rings_within < hi) {
        breaks.push_back(rings_within);
    }
    std::sort(breaks.begin(), breaks.end());
    Values values;
    for (std::size_t b = 0; b + 1 < breaks.size(); ++b) {
        const double u = std::max(breaks[b], lo);
        const double v = std::min(breaks[b + 1], hi);
        if (!(v > u)) {
            continue;
        }
        const bool rings = v <= rings_within;
        const int panels =
            std::max(1,
                     static_cast<int>(rings ? std::ceil((v - u) / RING_PANEL)
                                            : std::ceil(std::log(v / u) / std::log(PANEL_RATIO))));
        const double ratio = std::pow(v / u, 1.0 / panels);
        double start = u;
        for (int k = 0; k < panels; ++k) {
            const double end = k + 1 == panels ? v
                               : rings         ? start + (v - u) / panels
                                               : start * ratio;
            const double mid = 0.5 * (start + end);
            const double half = 0.5 * (end - start);
            for (std::size_t i = 0; i < GL4_NODES.size(); ++i) {
                const double r = mid + half * GL4_NODES[i];
                const double w = half * GL4_WEIGHTS[i] * weight(r);
                if (w == 0.0) {
                    continue;
                }
                smooth_profiles_(r, values);
                for (std::size_t c = 0; c < N; ++c) {
                    result[c] += w * values[c];
                }
            }
            start = end;
        }
    }
    return result;
}

/// Every channel's far field from its shapes' values.
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::combine_far_(const Values& shapes, Values& values) const
{
    constexpr std::size_t N = TSpectral::size();
    if (far_coefficients_.empty()) {
        values = shapes;
        return;
    }
    // One shape in focus, two with scattered light: unrolled.
    const double* coefficient = far_coefficients_.data();
    if (far_rank_ == 1) {
        for (std::size_t c = 0; c < N; ++c) {
            values[c] = coefficient[c] * shapes[0];
        }
    } else if (far_rank_ == 2) {
        for (std::size_t c = 0; c < N; ++c) {
            values[c] = coefficient[2 * c] * shapes[0] + coefficient[2 * c + 1] * shapes[1];
        }
    } else {
        for (std::size_t c = 0; c < N; ++c, coefficient += far_rank_) {
            double sum = 0.0;
            for (std::size_t i = 0; i < far_rank_; ++i) {
                sum += coefficient[i] * shapes[i];
            }
            values[c] = sum;
        }
    }
}

/**
 * @brief Every channel's light in a pixel from the profile's far field (averaged over any rings),
 * averaged over the pixel to second order; beyond the shapes' last node, each channel's own power
 * law.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::far_values_(double dx, double dy, double r, Values& values) const
{
    if (r < far_radii_.back()) {
        Values shapes;
        far_shapes_(dx, dy, r, shapes);
        combine_far_(shapes, values);
        return;
    }
    const double cosine = dx / r;
    const double sine = aspect_ * dy / r;
    const double cos2 = cosine * cosine;
    const double sin2 = sine * sine;
    const double aspect2 = aspect_ * aspect_;
    const double by_second = (cos2 + aspect2 * sin2) / 24.0;
    const double by_first = (sin2 + aspect2 * cos2) / (24.0 * r);
    for (std::size_t c = 0; c < TSpectral::size(); ++c) {
        const auto [value, first, second] = strips_[c].far(r);
        values[c] = aspect_ * (value + by_second * second + by_first * first);
    }
}

/**
 * @brief Finds the fewest shapes whose combinations give every channel's far field, by
 * Gram-Schmidt over the channels' tabulated values and derivatives. They are weighted by r^3,
 * r^4 and r^5, so that an r^-3 fall counts the same at every node. A channel whose part outside
 * the shapes so far is below a part in 10^6 of it adds none: the blurred far fields are
 * integrated numerically per channel, which leaves differences of about 10^-8 in shapes that are
 * the same, and a part in 10^6 leaves every pixel within 10^-4 of its own far field's, well
 * inside the tables' accuracy. If every channel adds a shape, the channels are kept as they are.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::build_far_basis_(const std::vector<double>& far_nodes)
{
    constexpr std::size_t N = TSpectral::size();
    constexpr double TOLERANCE = 1e-6;
    const std::size_t count = far_radii_.size();
    const std::size_t length = count * 3;
    std::vector<std::array<double, 3>> weights(count);
    for (std::size_t k = 0; k < count; ++k) {
        const double r = far_radii_[k];
        weights[k] = {r * r * r, r * r * r * r, r * r * r * r * r};
    }
    const auto row = [&](std::size_t c) {
        std::vector<double> result(length);
        for (std::size_t k = 0; k < count; ++k) {
            for (std::size_t j = 0; j < 3; ++j) {
                result[k * 3 + j] = far_nodes[(k * N + c) * 3 + j] * weights[k][j];
            }
        }
        return result;
    };
    const auto dot = [](const std::vector<double>& x, const std::vector<double>& y) {
        double sum = 0.0;
        for (std::size_t i = 0; i < x.size(); ++i) {
            sum += x[i] * y[i];
        }
        return sum;
    };

    std::vector<std::vector<double>> basis;
    for (std::size_t c = 0; c < N; ++c) {
        std::vector<double> residual = row(c);
        const double norm = std::sqrt(dot(residual, residual));
        // Twice, as the first pass leaves rounding along the shapes already found.
        for (int pass = 0; pass < 2; ++pass) {
            for (const std::vector<double>& shape : basis) {
                const double along = dot(residual, shape);
                for (std::size_t i = 0; i < length; ++i) {
                    residual[i] -= along * shape[i];
                }
            }
        }
        const double left = std::sqrt(dot(residual, residual));
        if (left > TOLERANCE * norm) {
            for (double& value : residual) {
                value /= left;
            }
            basis.push_back(std::move(residual));
        }
    }

    if (basis.size() == N) {
        far_rank_ = N;
        far_basis_ = far_nodes;
        far_coefficients_.clear();
        return;
    }
    far_rank_ = basis.size();
    far_coefficients_.assign(N * far_rank_, 0.0);
    for (std::size_t c = 0; c < N; ++c) {
        const std::vector<double> values = row(c);
        for (std::size_t i = 0; i < far_rank_; ++i) {
            far_coefficients_[c * far_rank_ + i] = dot(values, basis[i]);
        }
    }
    far_basis_.assign(count * far_rank_ * 3, 0.0);
    for (std::size_t k = 0; k < count; ++k) {
        for (std::size_t i = 0; i < far_rank_; ++i) {
            for (std::size_t j = 0; j < 3; ++j) {
                far_basis_[(k * far_rank_ + i) * 3 + j] = basis[i][k * 3 + j] / weights[k][j];
            }
        }
    }
}

/**
 * @brief The light of a source in the pixel whose center is (dx, dy) pixels from it, per
 * channel, for a source whose light totals 1 in each channel.
 *
 * Within the near table's radius, the near table; beyond, each channel's projected integral and,
 * beyond its far radius, its far field, each blended into the next over BLEND_WIDTH.
 */
template <IsSpectral TSpectral>
TSpectral PsfTables<TSpectral>::pixel(double dx, double dy) const
{
    return pixel(dx, dy, std::numeric_limits<double>::infinity());
}

/**
 * @brief The light of a source in a pixel, as pixel(dx, dy), with its rings followed only within
 * rings_within of it: beyond, each channel's far field, averaged over the rings, blended in over
 * BLEND_WIDTH. Far from a faint source the rings change a pixel by less than anything that
 * matters (see ring_deviation()), and the far field takes a fraction of the time: no projected
 * integrals, and no reads from the near table, which is too large to stay in a processor's
 * cache.
 *
 * The rings are always followed wherever a channel's far field does not hold.
 */
template <IsSpectral TSpectral>
TSpectral PsfTables<TSpectral>::pixel(double dx, double dy, double rings_within) const
{
    constexpr std::size_t N = TSpectral::size();
    const double r = std::sqrt(dx * dx + aspect_ * aspect_ * dy * dy);
    TSpectral result{0.f};
    Values far;
    if (r >= std::min(far_from_, std::max(rings_within, smooth_from_all_) + BLEND_WIDTH)) {
        // Every channel from its far field:
        far_values_(dx, dy, r, far);
        for (std::size_t c = 0; c < N; ++c) {
            result[c] = static_cast<float>(far[c]);
        }
        return result;
    }

    // Each channel's share from its far field, beyond its rings, and whether its rings are
    // wanted:
    Values smooth_weight{};
    std::array<bool, N> rings{};
    bool any_smooth = false;
    for (std::size_t c = 0; c < N; ++c) {
        smooth_weight[c] = psf_tables_detail::smooth_step(
            (r - std::max(rings_within, smooth_from_[c])) / BLEND_WIDTH);
        rings[c] = smooth_weight[c] < 1.0;
        any_smooth = any_smooth || smooth_weight[c] > 0.0;
    }
    bool have_far = false;
    const auto need_far = [&] {
        if (!have_far) {
            far_values_(dx, dy, r, far);
            have_far = true;
        }
    };

    // The rings: the near table, then the projected integrals, then the far field, each blended
    // into the next.
    Values inner{};
    const double blend_from = near_radius_ - BLEND_WIDTH;
    if (r <= blend_from) {
        const TSpectral near = near_value_(dx, dy);
        for (std::size_t c = 0; c < N; ++c) {
            inner[c] = static_cast<double>(near[c]);
        }
    } else {
        const double near_weight =
            r < near_radius_ ? 1.0 - psf_tables_detail::smooth_step((r - blend_from) / BLEND_WIDTH)
                             : 0.0;
        const TSpectral near = near_weight > 0.0 ? near_value_(dx, dy) : TSpectral{0.f};
        Values outer_weight{};
        std::array<bool, N> strip_wanted{};
        bool any_strip = false;
        bool any_far = false;
        for (std::size_t c = 0; c < N; ++c) {
            outer_weight[c] =
                r < strip_limit_[c]
                    ? psf_tables_detail::smooth_step((r - far_blend_from_[c]) / BLEND_WIDTH)
                    : 1.0;
            strip_wanted[c] = rings[c] && outer_weight[c] < 1.0;
            any_strip = any_strip || strip_wanted[c];
            any_far = any_far || (rings[c] && outer_weight[c] > 0.0);
        }
        Values strip{};
        if (any_strip) {
            strip_values_(dx, dy, r, strip_wanted, strip);
        }
        if (any_far) {
            need_far();
        }
        for (std::size_t c = 0; c < N; ++c) {
            if (!rings[c]) {
                continue;
            }
            double outer = strip[c];
            if (outer_weight[c] >= 1.0) {
                outer = far[c];
            } else if (outer_weight[c] > 0.0) {
                outer = (1.0 - outer_weight[c]) * strip[c] + outer_weight[c] * far[c];
            }
            inner[c] = near_weight * static_cast<double>(near[c]) + (1.0 - near_weight) * outer;
        }
    }

    if (any_smooth) {
        need_far();
    }
    for (std::size_t c = 0; c < N; ++c) {
        const double weight = smooth_weight[c];
        result[c] = static_cast<float>(weight > 0.0 ? (1.0 - weight) * inner[c] + weight * far[c]
                                                    : inner[c]);
    }
    return result;
}

/**
 * @brief An upper bound on how far a pixel at least r from a source strays, in a channel, from
 * its far field, for a source whose light totals 1: how much following the rings beyond r can
 * matter. Infinite where the far field does not hold, and 0 beyond the projected integrals.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::ring_deviation(std::size_t channel, double r) const
{
    // The straying is found at a sample of distances and directions, not everywhere: the
    // greatest found from a pixel nearer in, with a margin, bounds the rest.
    constexpr double MARGIN = 1.5;
    if (r < ring_start_) {
        return std::numeric_limits<double>::infinity();
    }
    const std::vector<double>& deviation = ring_deviation_[channel];
    const double u = std::max(0.0, (r - ring_start_ - 1.0) / RING_STEP);
    if (u >= static_cast<double>(deviation.size())) {
        return 0.0;
    }
    return MARGIN * deviation[static_cast<std::size_t>(u)];
}

/**
 * @brief Tabulates ring_deviation(): every RING_STEP from where the first far field holds to the
 * end of the projected integrals, the most that a pixel in any of 32 directions across a quadrant
 * (and the same reflected) strays from its far field, then the greatest at or beyond each node.
 */
template <IsSpectral TSpectral>
void PsfTables<TSpectral>::build_ring_deviation_()
{
    constexpr std::size_t N = TSpectral::size();
    constexpr int DIRECTIONS = 32;
    ring_start_ = smooth_from_[0];
    double limit = near_radius_;
    for (std::size_t c = 0; c < N; ++c) {
        ring_start_ = std::min(ring_start_, smooth_from_[c]);
        limit = std::max(limit, strip_limit_[c]);
    }
    const auto count = static_cast<std::size_t>(std::ceil((limit - ring_start_) / RING_STEP)) + 1;
    std::vector<Values> found(count);
    tbb::parallel_for(std::size_t{0}, count, [&](std::size_t k) {
        const double r = ring_start_ + static_cast<double>(k) * RING_STEP;
        Values most{};
        for (int j = 0; j < DIRECTIONS; ++j) {
            const double angle =
                0.5 * PI<double>() * static_cast<double>(j) / static_cast<double>(DIRECTIONS - 1);
            const double dx = r * std::cos(angle);
            const double dy = r * std::sin(angle) / aspect_;
            const TSpectral rings = pixel(dx, dy);
            Values far{};
            far_values_(dx, dy, r, far);
            for (std::size_t c = 0; c < N; ++c) {
                most[c] = std::max(most[c], std::abs(static_cast<double>(rings[c]) - far[c]));
            }
        }
        for (std::size_t c = 0; c < N; ++c) {
            if (r >= strip_limit_[c]) {
                most[c] = 0.0;
            } else if (r < smooth_from_[c]) {
                most[c] = std::numeric_limits<double>::infinity();
            }
        }
        found[k] = most;
    });

    ring_deviation_.assign(N, {});
    for (std::size_t c = 0; c < N; ++c) {
        std::vector<double>& deviation = ring_deviation_[c];
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

/**
 * @brief Adds a source's light to the pixels of a block of an image, as pixel() with the rings
 * followed within rings_within, out to reach from the source, times weight(r) at a distance r.
 *
 * Where every channel takes its far field, its shapes are evaluated once for all channels, and
 * the light is added straight from them: the bulk of a bright source's pixels, at a fraction of
 * pixel()'s cost.
 *
 * @param target The image; pixel (x, y) covers [x, x + 1) by [y, y + 1).
 * @param x_begin, x_end, y_begin, y_end The block's pixels, which must be in the image.
 * @param source_x, source_y The source's position, in the same coordinates.
 * @param power The source's light, per channel.
 * @param rings_within How far out its rings are followed.
 * @param reach How far out it is drawn.
 * @param weight A function of the distance, the share drawn there.
 * @return The light drawn per channel, as a fraction of power.
 */
template <IsSpectral TSpectral>
template <class Weight>
std::array<double, TSpectral::size()> PsfTables<TSpectral>::draw(Image<TSpectral>& target,
                                                                 int x_begin,
                                                                 int x_end,
                                                                 int y_begin,
                                                                 int y_end,
                                                                 double source_x,
                                                                 double source_y,
                                                                 const TSpectral& power,
                                                                 double rings_within,
                                                                 double reach,
                                                                 const Weight& weight) const
{
    constexpr std::size_t N = TSpectral::size();
    // Where pixel() takes every channel from its far field, and the shapes hold:
    const double far_from =
        std::min(far_from_, std::max(rings_within, smooth_from_all_) + BLEND_WIDTH);
    const double far_to = far_radii_.back();

    // Each shape's light per channel, for this source:
    std::array<TSpectral, N> shape_power{};
    for (std::size_t i = 0; i < far_rank_; ++i) {
        for (std::size_t c = 0; c < N; ++c) {
            const double coefficient = far_coefficients_.empty()
                                           ? (c == i ? 1.0 : 0.0)
                                           : far_coefficients_[c * far_rank_ + i];
            shape_power[i][c] = static_cast<float>(coefficient * static_cast<double>(power[c]));
        }
    }

    Values drawn{};
    Values shape_drawn{};
    Values shapes;
    const double reach2 = reach * reach;
    const double aspect2 = aspect_ * aspect_;
    for (int y = y_begin; y < y_end; ++y) {
        const double dy = static_cast<double>(y) + 0.5 - source_y;
        for (int x = x_begin; x < x_end; ++x) {
            const double dx = static_cast<double>(x) + 0.5 - source_x;
            const double r2 = dx * dx + aspect2 * dy * dy;
            if (r2 > reach2) {
                continue;
            }
            const double r = std::sqrt(r2);
            const double share = weight(r);
            if (share == 0.0) {
                continue;
            }
            if (r >= far_from && r < far_to) {
                far_shapes_(dx, dy, r, shapes);
                TSpectral light{0.f};
                for (std::size_t i = 0; i < far_rank_; ++i) {
                    const double value = share * shapes[i];
                    shape_drawn[i] += value;
                    light += shape_power[i] * static_cast<float>(value);
                }
                target(x, y) += light;
                continue;
            }
            const TSpectral value = pixel(dx, dy, rings_within);
            TSpectral light{0.f};
            for (std::size_t c = 0; c < N; ++c) {
                const double part = share * static_cast<double>(value[c]);
                drawn[c] += part;
                light[c] = static_cast<float>(part);
            }
            target(x, y) += power * light;
        }
    }
    Values from_shapes;
    combine_far_(shape_drawn, from_shapes);
    for (std::size_t c = 0; c < N; ++c) {
        drawn[c] += from_shapes[c];
    }
    return drawn;
}

/**
 * @brief Tabulates one channel's profile every half of its antiderivatives' step, from the source
 * to beyond where its far field takes over, with the suffix maxima, and finds the radius holding
 * 90% of the light by summing 2 pi r P(r) (with the trapezoid rule, within about 1e-4).
 */
template <IsSpectral TSpectral>
template <class Profile>
typename PsfTables<TSpectral>::Radial PsfTables<TSpectral>::build_radial_(const Profile& profile,
                                                                          const Strip& strip) const
{
    constexpr double MARGIN = 4.0;
    Radial radial;
    radial.step = 0.5 * grid_.step;
    const auto count =
        static_cast<std::size_t>(std::ceil((strip.far_radius + MARGIN) / radial.step)) + 1;
    radial.value.resize(count);
    radial.slope.resize(count);
    for (std::size_t k = 0; k < count; ++k) {
        radial.value[k] = profile(static_cast<double>(k) * radial.step);
        radial.slope[k] = profile.slope(static_cast<double>(k) * radial.step);
    }
    radial.suffix_max.resize(count);
    double running = 0.0;
    for (std::size_t k = count; k-- > 0;) {
        running = std::max(running, radial.value[k]);
        radial.suffix_max[k] = running;
    }
    double light = 0.0;
    radial.radius_90 = static_cast<double>(count - 1) * radial.step;
    for (std::size_t k = 1; k < count; ++k) {
        const double r0 = static_cast<double>(k - 1) * radial.step;
        const double r1 = static_cast<double>(k) * radial.step;
        const double added =
            PI<double>() * radial.step * (r0 * radial.value[k - 1] + r1 * radial.value[k]);
        if (light + added >= 0.9) {
            radial.radius_90 = r0 + radial.step * (0.9 - light) / added;
            break;
        }
        light += added;
    }
    return radial;
}

/**
 * @brief One channel's profile, the light per unit area (in pixel widths squared) at a distance r
 * from the source, by cubic Hermite interpolation from its value and slope at 16 points to the
 * pattern's finest period: within about 10^-4 of the light, where linear interpolation over the
 * rings' troughs would add half a percent. Beyond the far radius, its far field.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::profile(std::size_t channel, double r) const
{
    const Radial& radial = radials_[channel];
    const double u = r / radial.step;
    if (u >= static_cast<double>(radial.value.size() - 1)) {
        return strips_[channel].far(r)[0];
    }
    const auto k = static_cast<std::size_t>(u);
    const double t = u - static_cast<double>(k);
    const double t2 = t * t;
    const double h01 = t2 * (3.0 - 2.0 * t);
    const double h10 = t * (1.0 - t) * (1.0 - t);
    const double h11 = t2 * (t - 1.0);
    return (1.0 - h01) * radial.value[k] + h01 * radial.value[k + 1] +
           radial.step * (h10 * radial.slope[k] + h11 * radial.slope[k + 1]);
}

/**
 * @brief One channel's profile averaged over its rings: its far field, which for an in-focus
 * profile holds from a pixel out, and for a defocused one from where it is smooth. Closer in, the
 * profile itself.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::smooth_profile(std::size_t channel, double r) const
{
    const Strip& strip = strips_[channel];
    return r >= std::max(strip.far.start(), strip.smooth_from) ? strip.far(r)[0]
                                                               : profile(channel, r);
}

/**
 * @brief The light beyond r of one channel's profile averaged over its rings, for r in its far
 * field: the far field integrated out to its table's end, and beyond, the power law it follows
 * there integrated to infinity.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::smooth_light_beyond(std::size_t channel, double r) const
{
    constexpr std::array<double, 4> GL4_NODES{
        -0.8611363115940526, -0.3399810435848563, 0.3399810435848563, 0.8611363115940526};
    constexpr std::array<double, 4> GL4_WEIGHTS{
        0.3478548451374538, 0.6521451548625461, 0.6521451548625461, 0.3478548451374538};
    constexpr double PANEL_RATIO = 1.2;
    const detail::RadialTable& far = strips_[channel].far;
    const double end = std::max(r, far.end());
    double light = 0.0;
    if (end > r) {
        const int panels =
            std::max(1, static_cast<int>(std::ceil(std::log(end / r) / std::log(PANEL_RATIO))));
        const double ratio = std::pow(end / r, 1.0 / panels);
        double start = r;
        for (int k = 0; k < panels; ++k) {
            const double stop = k + 1 == panels ? end : start * ratio;
            const double mid = 0.5 * (start + stop);
            const double half = 0.5 * (stop - start);
            for (std::size_t i = 0; i < GL4_NODES.size(); ++i) {
                const double at = mid + half * GL4_NODES[i];
                light += half * GL4_WEIGHTS[i] * 2.0 * PI<double>() * at * far(at)[0];
            }
            start = stop;
        }
    }
    const std::array<double, 3> tail = far(end);
    if (tail[0] > 0.0) {
        const double falloff = -end * tail[1] / tail[0];
        if (falloff > 2.0) {
            light += 2.0 * PI<double>() * tail[0] * end * end / (falloff - 2.0);
        }
    }
    return light;
}

/**
 * @brief An upper bound on the light, in a channel, of any pixel whose center is at least r from
 * the source, for a source whose light totals 1, drawn by pixel() with its rings followed within
 * rings_within: the pixel's area times the profile's greatest value from the pixel's nearest
 * corner outward. Where pixel() takes the far field, which decreases, that is its value at the
 * nearest corner, below the rings' peaks. It does not increase with r.
 */
template <IsSpectral TSpectral>
double PsfTables<TSpectral>::envelope(std::size_t channel, double r, double rings_within) const
{
    const Radial& radial = radials_[channel];
    const double nearest = std::max(0.0, r - 0.5 * std::hypot(1.0, aspect_));
    const bool far =
        nearest >= std::min(far_from_, std::max(rings_within, smooth_from_all_) + BLEND_WIDTH);
    const double u = nearest / radial.step;
    if (far || u >= static_cast<double>(radial.value.size() - 1)) {
        return aspect_ * strips_[channel].far(nearest)[0];
    }
    return aspect_ * radial.suffix_max[static_cast<std::size_t>(u)];
}

/// Memory the tables take, in bytes.
template <IsSpectral TSpectral>
std::size_t PsfTables<TSpectral>::memory_bytes() const
{
    std::size_t bytes = near_.size() * sizeof(TSpectral);
    bytes +=
        (strip_nodes_.size() + far_radii_.size() + far_basis_.size() + far_coefficients_.size()) *
        sizeof(double);
    for (const Strip& strip : strips_) {
        bytes += strip.far.memory_bytes();
    }
    for (const Radial& radial : radials_) {
        bytes += 3 * radial.value.size() * sizeof(double);
    }
    for (const std::vector<double>& deviation : ring_deviation_) {
        bytes += deviation.size() * sizeof(double);
    }
    bytes += line_nodes_.size() * sizeof(double);
    bytes += far_line_nodes_.size() * sizeof(double);
    for (const auto* tables : {&line_deviation_, &line_suffix_, &end_deviation_}) {
        for (const std::vector<double>& table : *tables) {
            bytes += table.size() * sizeof(double);
        }
    }
    for (const HalfLines& table : half_lines_) {
        bytes += (table.f.size() + table.laplacian.size()) * sizeof(double);
    }
    bytes += corners_.size() * sizeof(double);
    return bytes;
}

} // namespace huira
