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
    void far_shapes_(double dx, double dy, double r, Values& shapes) const;
    void combine_far_(const Values& shapes, Values& values) const;
    void far_values_(double dx, double dy, double r, Values& values) const;
    void build_far_basis_(const std::vector<double>& far_nodes);
    void build_ring_deviation_();
};

} // namespace huira

#include "huira_impl/cameras/psfs/psf_tables.ipp"
