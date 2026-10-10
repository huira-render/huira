#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "huira/cameras/psfs/psf_profile.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {

/**
 * @brief Tables that give the light an unresolved source puts in each pixel, for a radially
 * symmetric point spread function with a profile per channel: diffraction, and scattered light.
 *
 * Every pixel's value is the PSF integrated over that pixel, for a source at its exact position:
 * nothing is rounded to a grid of positions. Three representations of each channel's profile
 * cover three ranges of the distance r from the source:
 *
 * - Within near_radius(): a table of the pixel-integrated PSF against the offset from the source
 *   to the pixel's center, read by cubic B-spline interpolation. Its pitch follows the PSF's
 *   finest detail, so that every pixel is within about 0.03% of exact.
 * - From there to the channel's far_radius(): the profile integrated over the pixel's
 *   projection on the line from the source, taken a little further out to allow for the
 *   circles' curvature across the pixel. Two antiderivatives of the profile, tabulated, make
 *   this four lookups.
 * - Beyond: the profile's far field, averaged over its rings and over the pixel. Scattered light
 *   has no rings, so it is taken as it is, averaged over the pixel.
 *
 * A defocused PSF, the in-focus one blurred by a uniform disc, is tabulated the same way, with the
 * near table reaching over blurs up to MAX_NEAR_BLUR.
 *
 * Neighbouring representations agree to 0.1%, and are blended over BLEND_WIDTH so that no seam
 * forms. Beyond the antiderivatives' reach (twice the frame's diagonal, for a camera's tables)
 * the far field is used whatever the rings.
 *
 * A moving source's light is its path's line integral of the pixels' light: for each straight
 * piece of the path, the light of the whole line through it, from the line spread (the profile
 * integrated along a line, and its antiderivatives across the pixel), less that of the half
 * lines beyond its ends (see draw_line()). light_in_rectangle() gives the light a source puts on
 * the frame, from the line spread and the light beyond two edges at once.
 *
 * Lengths are in units of the pixel's width; a pixel is aspect() widths high.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class PsfTables {
  public:
    /// Width of the zones where neighbouring representations are blended.
    static constexpr double BLEND_WIDTH = 2.0;

    /// The tables' shape, and the near table's extent and pitch.
    struct Layout {
        /// The pixel's height over its width.
        double aspect = 1.0;
        /// Where the near table hands over to the projected integrals.
        double near_radius = 8.0;
        /// Near-table points per pixel, in each axis. Even, so that pixel edges fall on them.
        int near_samples = 64;
        /// How far from the source the projected integrals are tabulated.
        double reach = 0.0;
    };

    /// One channel's profile beyond the near table.
    struct Channel {
        /// Where the far field takes over.
        double far_radius = 0.0;
        /// Spacing of the antiderivatives' nodes.
        double step = 0.25;
    };

    /// Below this blur radius, in pixel widths, the PSF is taken as in focus: the blur changes
    /// no pixel by more than a part in 10^5.
    static constexpr double MIN_BLUR = 1e-4;

    /// Blur radii, in pixel widths, closer than this give the same pixels, within 1e-5 of the
    /// peak: a pixel changes with the blur by at most four times as much, relative to the peak
    /// (2.7 measured, at f/1 with 10 um pixels and a blur of 0.7 px).
    static constexpr double BLUR_TOLERANCE = 2.5e-6;

    /// Blur radii up to this, in pixel widths, are covered by the near table, so that the blur's
    /// edge is exact; beyond, the projected integrals take its edge, within 0.25%.
    static constexpr double MAX_NEAR_BLUR = 6.0;

    /// Harvey-Shack scattered light, in pixel widths: see detail::ScatterProfile.
    struct Scatter {
        double fraction = 0.0;
        double slope = 2.0;
        double shoulder = 1.0;
        std::optional<double> outer;
    };

    template <class Profile>
    PsfTables(const std::vector<Profile>& profiles,
              const std::vector<Channel>& channels,
              const Layout& layout);

    static PsfTables airy(double fnumber,
                          double pitch_x,
                          double pitch_y,
                          double reach,
                          const std::optional<Scatter>& scatter = std::nullopt,
                          double blur = 0.0);

    [[nodiscard]] TSpectral pixel(double dx, double dy) const;
    [[nodiscard]] TSpectral pixel(double dx, double dy, double rings_within) const;
    [[nodiscard]] Image<TSpectral> image(int radius, double x_offset, double y_offset) const;

    template <class Weight>
    std::array<double, TSpectral::size()> draw(Image<TSpectral>& target,
                                               int x_begin,
                                               int x_end,
                                               int y_begin,
                                               int y_end,
                                               double source_x,
                                               double source_y,
                                               const TSpectral& power,
                                               double rings_within,
                                               double reach,
                                               const Weight& weight) const;

    /// The pixel's height over its width.
    [[nodiscard]] double aspect() const { return aspect_; }

    /// Where the near table hands over to the projected integrals.
    [[nodiscard]] double near_radius() const { return near_radius_; }

    /// Near-table points per pixel, in each axis.
    [[nodiscard]] int near_samples() const { return near_samples_; }

    /// Where a channel's far field takes over.
    [[nodiscard]] double far_radius(std::size_t channel) const
    {
        return strips_[channel].far_radius;
    }

    /// Beyond this distance pixel() takes every channel from its far field, rings or not.
    [[nodiscard]] double far_from() const { return far_from_; }

    [[nodiscard]] std::array<double, TSpectral::size()> far_pixel(double dx, double dy) const;

    [[nodiscard]] double profile(std::size_t channel, double r) const;
    [[nodiscard]] double smooth_profile(std::size_t channel, double r) const;
    [[nodiscard]] double smooth_light_beyond(std::size_t channel, double r) const;
    [[nodiscard]] double
    envelope(std::size_t channel,
             double r,
             double rings_within = std::numeric_limits<double>::infinity()) const;
    [[nodiscard]] double ring_deviation(std::size_t channel, double r) const;

    template <class Weight>
    [[nodiscard]] std::array<double, TSpectral::size()>
    integrate(double lo, double hi, std::vector<double> breaks, const Weight& weight) const;

    /// The radius of the circle about a source that holds 90% of its light in a channel.
    [[nodiscard]] double radius_holding_90(std::size_t channel) const
    {
        return radials_[channel].radius_90;
    }

    /// How far from a moving source's path, and from its ends, its light is drawn exactly. See
    /// draw_line().
    struct LineReach {
        /// The line spread's rings are followed within this of the path; beyond, the far
        /// field's line spread.
        double exact_within = std::numeric_limits<double>::infinity();
        /// Near its ends, the pixels' own light is integrated along the path within this of
        /// them; beyond, the far field's.
        double ends_exact_within = 0.0;
        /// The ends are allowed for within this of them.
        double ends_within = std::numeric_limits<double>::infinity();
        /// The pixels drawn: those within this of the path.
        double reach = std::numeric_limits<double>::infinity();
    };

    template <class Weight>
    std::array<double, TSpectral::size()> draw_line(Image<TSpectral>& target,
                                                    int x_begin,
                                                    int x_end,
                                                    int y_begin,
                                                    int y_end,
                                                    const std::array<double, 2>& start,
                                                    const std::array<double, 2>& end,
                                                    const TSpectral& density,
                                                    const LineReach& reach,
                                                    const Weight& weight) const;

    [[nodiscard]] double line_envelope(std::size_t channel, double r) const;
    [[nodiscard]] double line_deviation(std::size_t channel, double d) const;
    [[nodiscard]] double end_deviation(std::size_t channel, double r) const;

    /// The distance either side of a line that holds 90% of its light in a channel.
    [[nodiscard]] double line_holding_90(std::size_t channel) const
    {
        return line_holding_90_[channel];
    }

    [[nodiscard]] std::array<double, TSpectral::size()> half_plane_light(double d) const;
    [[nodiscard]] std::array<double, TSpectral::size()> corner_light(double a, double b) const;
    [[nodiscard]] std::array<double, TSpectral::size()>
    light_in_rectangle(double x0, double x1, double y0, double y1) const;

    [[nodiscard]] std::size_t memory_bytes() const;

  private:
    /// Where the channels' antiderivatives and far fields are tabulated: the same nodes for
    /// all, so that a pixel finds its place among them once.
    struct Grid {
        double start = 0.0;
        double step = 0.25;
        std::size_t count = 0;
        double far_start = 1.0;
        double far_end = 2.0;
    };

    /// One channel's antiderivatives, every step from start. Each node holds the two
    /// antiderivatives (integrals of the profile to infinity, once and twice), the profile and
    /// its slope. Its far field, averaged over any rings, holds from smooth_from out.
    struct Strip {
        double limit = 0.0;
        double far_radius = 0.0;
        double smooth_from = 0.0;
        detail::RadialTable far;
        std::vector<std::array<double, 4>> nodes;
    };

    double aspect_ = 1.0;
    double near_radius_ = 0.0;
    int near_samples_ = 0;
    bool symmetric_ = true;
    int near_width_ = 0;
    int near_height_ = 0;
    std::vector<TSpectral> near_;
    std::vector<Strip> strips_;

    // The strips of all channels at each node, channel by channel: 4 values per channel per
    // node.
    Grid grid_;
    std::vector<double> strip_nodes_;

    // The far fields, at far_radii_, which grow by a constant ratio. They are mostly one shape at
    // different scales (the diffraction pattern's r^-3 at each wavelength, and scattered light,
    // the same in every channel), so they are held as far_rank_ shapes, each with its value and
    // first two derivatives at every node, shape by shape; a channel's far field is a
    // combination of them, with far_coefficients_ channel by channel. A pixel evaluates each
    // shape once. Without coefficients, the shapes are the channels.
    std::vector<double> far_radii_;
    std::vector<double> far_inverse_steps_;
    // The node below each of FAR_KEY_BITS-bit slices of the radii's binary representation from
    // far_radii_'s first: a slice is narrower than the nodes' spacing, so the node below r is
    // its slice's or the next. far_key_start_ is the first slice's key.
    static constexpr int FAR_KEY_BITS = 8;
    std::uint64_t far_key_start_ = 0;
    std::vector<std::uint32_t> far_index_;
    std::size_t far_rank_ = 0;
    std::vector<double> far_basis_;
    std::vector<double> far_coefficients_;

    // How far a pixel's light in each channel can stray from its far field, from ring_start_,
    // where the first far field holds, out: the greatest straying found at or beyond each node,
    // every RING_STEP.
    static constexpr double RING_STEP = 0.1;
    double ring_start_ = 0.0;
    std::vector<std::vector<double>> ring_deviation_;

    std::array<double, TSpectral::size()> strip_limit_{};
    std::array<double, TSpectral::size()> far_blend_from_{};
    std::array<double, TSpectral::size()> smooth_from_{};
    // Beyond far_from_ every channel takes its far field, rings or not; beyond smooth_from_all_,
    // every channel's far field holds.
    double far_from_ = 0.0;
    double smooth_from_all_ = 0.0;

    /// One channel's profile from the source out to its far field, every step, for questions
    /// about the light at a distance: how much there is, how far it reaches. The suffix maxima
    /// bound the profile beyond each node.
    struct Radial {
        double step = 0.0;
        std::vector<double> value;
        std::vector<double> slope;
        std::vector<double> suffix_max;
        double radius_90 = 0.0;
    };
    std::vector<Radial> radials_;

    // A moving source's light: its path's line integral (see draw_line()). Each channel's line
    // spread, its two antiderivatives and its slope, every grid_.step from the line out to its
    // strip limit and beyond (line_nodes_, 4 values per node, each for every channel in turn);
    // each far field shape's line spread with its first two derivatives at far_radii_, shape by
    // shape (far_line_nodes_); how far the pixels stray from the far
    // field's line spread (line_deviation_, every RING_STEP from ring_start_); the suffix maxima
    // of each channel's line spread at the nodes (line_suffix_); how far a half line's light
    // strays (end_deviation_); and each shape's integral along half lines (half_lines_).
    static constexpr double END_STEP = 0.5;
    std::size_t line_count_ = 0;
    std::vector<double> line_nodes_;
    std::vector<double> far_line_nodes_;
    std::vector<std::vector<double>> line_deviation_;
    std::vector<std::vector<double>> line_suffix_;
    std::vector<std::vector<double>> end_deviation_;
    std::array<double, TSpectral::size()> line_holding_90_{};

    /// One far field shape's integrals along half lines from a point, against the distance rho
    /// from it and the angle beta of the half line from the pixel's foot on it: the shape (f),
    /// and its Laplacian (laplacian), scaled by rho^2 and rho^4 so that they change slowly, on
    /// a grid even in log rho and beta.
    struct HalfLines {
        double log_start = 0.0;
        double log_step = 0.0;
        std::size_t rho_count = 0;
        double beta_step = 0.0;
        std::size_t beta_count = 0;
        std::vector<double> f;
        std::vector<double> laplacian;
    };
    std::vector<HalfLines> half_lines_;

    /// The light beyond two perpendicular edges, at distances a, b >= 0 from the source (see
    /// corner_light()), from corner_start_ out: every CORNER_STEP in log R, R = hypot(a, b),
    /// from one node before corner_start_ to one after the last, corner_rows_ in all, and at
    /// CORNER_ANGLES steps of the angle atan(min(a, b) / max(a, b)) up to pi / 4, from one step
    /// below 0; each node's channels together.
    static constexpr double CORNER_STEP = 1.0 / 16.0;
    static constexpr std::size_t CORNER_ANGLES = 16;
    static constexpr double CORNER_END = 131072.0;
    double corner_start_ = 0.0;
    std::size_t corner_rows_ = 0;
    std::vector<double> corners_;

    template <class Profile>
    Radial build_radial_(const Profile& profile, const Strip& strip) const;

    template <class Profile>
    void build_near_(const std::vector<Profile>& profiles);
    template <class Profile>
    Strip build_strip_(const Profile& profile, const Channel& channel, double reach) const;

    using Values = std::array<double, TSpectral::size()>;

    [[nodiscard]] std::size_t near_index_(int i, int j) const;
    [[nodiscard]] TSpectral near_value_(double dx, double dy) const;
    void strip_values_(double dx,
                       double dy,
                       double r,
                       const std::array<bool, TSpectral::size()>& wanted,
                       Values& values) const;
    [[nodiscard]] std::size_t far_node_(double r) const;
    void smooth_profiles_(double r, Values& values) const;
    void far_shapes_(double dx,
                     double dy,
                     double r,
                     Values& shapes,
                     double spread = 0.0,
                     double along2 = 0.0) const;
    void combine_far_(const Values& shapes, Values& values) const;
    void far_values_(double dx, double dy, double r, Values& values) const;
    void build_far_basis_(const std::vector<double>& far_nodes);
    void build_ring_deviation_();

    template <class Profile>
    void build_lines_(const std::vector<Profile>& profiles);
    void build_far_lines_();
    void build_line_deviation_();
    void build_half_lines_();
    void build_corners_();
    [[nodiscard]] Values corner_integral_(double a, double b) const;
    [[nodiscard]] Values corner_table_(double a, double b) const;
    [[nodiscard]] std::array<double, 3> far_shape_(std::size_t shape, double r) const;
    void line_exact_(double normal_x,
                     double normal_y,
                     double d,
                     const std::array<bool, TSpectral::size()>& wanted,
                     Values& values) const;
    void line_far_(double normal_x, double normal_y, double d, Values& values) const;
    [[nodiscard]] std::array<double, 2> far_line_(std::size_t shape, double d) const;
    void half_line_far_(double qx, double qy, double ex, double ey, Values& values) const;
    void half_line_(
        double qx, double qy, double hx, double hy, double exact_within, Values& values) const;
};

} // namespace huira

#include "huira_impl/cameras/psfs/psf_tables.ipp"
#include "huira_impl/cameras/psfs/psf_tables_lines.ipp"
