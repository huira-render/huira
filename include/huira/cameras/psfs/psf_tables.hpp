#pragma once

#include <array>
#include <cstddef>
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
    [[nodiscard]] Image<TSpectral> image(int radius, double x_offset, double y_offset) const;

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

    [[nodiscard]] std::size_t memory_bytes() const;

  private:
    /// One channel's antiderivatives, from start in steps of step. Each node holds the two
    /// antiderivatives (integrals of the profile to infinity, once and twice), the profile and
    /// its slope.
    struct Strip {
        double start = 0.0;
        double step = 0.25;
        double limit = 0.0;
        double far_radius = 0.0;
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

    template <class Profile>
    void build_near_(const std::vector<Profile>& profiles);
    template <class Profile>
    Strip build_strip_(const Profile& profile, const Channel& channel, double reach) const;

    [[nodiscard]] std::size_t near_index_(int i, int j) const;
    [[nodiscard]] TSpectral near_value_(double dx, double dy) const;
    [[nodiscard]] double outer_value_(const Strip& strip, double dx, double dy, double r) const;
    [[nodiscard]] double strip_value_(const Strip& strip, double dx, double dy, double r) const;
    [[nodiscard]] double far_value_(const Strip& strip, double dx, double dy, double r) const;
};

} // namespace huira

#include "huira_impl/cameras/psfs/psf_tables.ipp"
