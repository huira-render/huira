#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "huira/cameras/psfs/psf.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {

/**
 * @brief A PSF set with CameraModel::set_psf() or set_measured_psf(), on a grid of samples,
 * ready to draw an unresolved source from at its exact position.
 *
 * The PSF is the cubic B-spline that interpolates its samples, within the rectangle they span,
 * and is taken as complete: it holds all of a source's light, the same in every channel, and
 * none falls beyond the rectangle. The spline is continued past the samples by mirroring them,
 * and cut off at the rectangle's edge only when it is integrated, so that an edge where the
 * samples are not yet zero does not ring. A measurement is taken out to the PSF's radius
 * (PSF::get_radius()) from its center; any other PSF is sampled from PSF::evaluate() every
 * SAMPLE_STEP pixels out to its radius, or more coarsely if that would take more than
 * MEMORY_BUDGET. For a pattern sampled at least twice as finely as its finest detail, the
 * spline is within a few parts in 10^3 of the pattern, where straight lines between the samples
 * are off by several percent.
 *
 * Point samples (PSFSampling::PointSampled, and PSFs that are not integrated over a pixel) are
 * the pattern's intensity. A pixel's light is the spline integrated over the pixel, exactly:
 * four lookups of the light within a corner (see corner_light_()), from running sums of the
 * spline's coefficients and the integral of its basis. The light on all pixels is then the same
 * wherever the source is.
 *
 * Pixel-integrated samples (PSFSampling::PixelIntegrated) are what a pixel centered at each one
 * collects. A pixel's light is the spline at its center, if that is within the rectangle, over
 * the same summed over all such pixels for the source's position (see scale()), which keeps
 * each source's light exact, as the stamps' normalization did.
 *
 * Coordinates are in pixels along each axis, as PSF::evaluate() takes them; distances, as
 * elsewhere, in pixel widths, a pixel being aspect() widths high.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class GridPsf {
  public:
    using Light = std::array<double, TSpectral::size()>;

    /// How finely a PSF that is not a measurement is sampled, in pixels.
    static constexpr double SAMPLE_STEP = 1.0 / 16.0;

    /// The most memory a sampled PSF takes, before its samples are made coarser.
    static constexpr std::size_t MEMORY_BUDGET = std::size_t{64} * 1024 * 1024;

    GridPsf(PSF<TSpectral>& psf, double aspect);

    [[nodiscard]] TSpectral pixel(double dx, double dy, const Light& scale) const;
    [[nodiscard]] Light scale(double source_x, double source_y) const;

    template <class Weight>
    Light draw(Image<TSpectral>& target,
               int x_begin,
               int x_end,
               int y_begin,
               int y_end,
               double source_x,
               double source_y,
               const TSpectral& power,
               const Light& scale,
               double reach,
               const Weight& weight) const;

    [[nodiscard]] Light light_on_frame(
        double source_x, double source_y, int width, int height, const Light& scale) const;

    [[nodiscard]] double envelope(std::size_t channel, double r) const;

    /// The distance, in pixel widths, within which a source puts 90% of its light in a channel.
    [[nodiscard]] double radius_holding_90(std::size_t channel) const
    {
        return radius_90_[channel];
    }

    /// The farthest a pixel's center can be from a source, in pixel widths, and get its light.
    [[nodiscard]] double reach() const { return reach_; }

    /// Whether the samples are what a pixel collects, rather than the pattern's intensity.
    [[nodiscard]] bool pixel_integrated() const { return pixel_integrated_; }

    /// The pixel's height over its width.
    [[nodiscard]] double aspect() const { return aspect_; }

    /// The samples' spacing, in pixels.
    [[nodiscard]] double step() const { return step_; }

    [[nodiscard]] std::size_t memory_bytes() const;

  private:
    static constexpr std::size_t N = TSpectral::size();
    /// The envelope's bins, per pixel width.
    static constexpr double ENVELOPE_BINS = 8.0;
    /// Coefficients beyond the samples on every side, mirrored, which the spline takes within
    /// two samples of them.
    static constexpr int MARGIN = 2;

    double aspect_;
    bool pixel_integrated_ = false;
    double step_ = 1.0;
    int nx_ = 0;
    int ny_ = 0;
    double x0_ = 0.0; ///< The first coefficient's position, in pixels.
    double y0_ = 0.0;
    /// The rectangle the samples span, in pixels: the PSF's extent.
    double left_ = 0.0;
    double right_ = 0.0;
    double top_ = 0.0;
    double bottom_ = 0.0;

    /// The spline's coefficients, normalized, channel by channel at each: (j nx + i) N + c.
    std::vector<double> coefficients_;
    /// For point samples, running sums of the coefficients: over i' <= i and j' <= j, over
    /// i' <= i in row j, and over j' <= j in column i.
    std::vector<double> below_;
    std::vector<double> row_;
    std::vector<double> column_;

    std::vector<std::array<double, N>> envelope_; ///< By distance bin, the largest beyond.
    Light radius_90_{};
    double reach_ = 0.0;

    [[nodiscard]] std::size_t at_(int i, int j) const
    {
        return (static_cast<std::size_t>(j) * static_cast<std::size_t>(nx_) +
                static_cast<std::size_t>(i)) *
               N;
    }
    /// Where a position falls among the coefficients along an axis, and the weights of the four
    /// around it: the basis's integrals up to it, or its values there.
    struct Axis {
        long first = 0;      ///< The first of the four coefficients.
        bool before = false; ///< Whether it is before every coefficient's reach.
        std::array<double, 4> weights{};
    };
    [[nodiscard]] Axis integral_axis_(double offset, double origin, int count) const;
    [[nodiscard]] Axis value_axis_(double offset, double origin) const;
    [[nodiscard]] Light corner_light_(const Axis& x, const Axis& y) const;
    [[nodiscard]] Light rectangle_light_(double x0, double x1, double y0, double y1) const;
    [[nodiscard]] Light corner_light_(double x, double y) const;
    [[nodiscard]] Light interpolate_(const Axis& x, const Axis& y) const;
    [[nodiscard]] Light interpolate_(double x, double y) const;
    [[nodiscard]] Light sum_over_pixels_(double source_x, double source_y) const;
    void build_point_sampled_();
    void build_bounds_();
};

} // namespace huira

#include "huira_impl/cameras/psfs/grid_psf.ipp"
