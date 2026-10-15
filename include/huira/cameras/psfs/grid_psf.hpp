#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
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
 * samples are not yet zero does not ring. A measurement is taken whole, out to its radius
 * (PSF::radius()), its farthest sample; any other PSF is sampled from PSF::evaluate() every
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
 * Out of focus, the PSF is blurred by a uniform disc, the aperture's image (see
 * GridPsf(const GridPsf&, double)), and held as pixel-integrated samples, which sum over the
 * pixels to all of the light wherever the source is, so that scale() is 1.
 *
 * Without a PSF, a source is the defocus blur alone, a uniform disc, or in focus a point (see
 * disc()). A disc under EXACT_DISC pixel widths across its radius, or a point, has no samples:
 * each pixel's light is the disc's area within it, and a line's that integrated along the line,
 * computed exactly where it is drawn.
 *
 * A moving source is drawn as a line integral of its pixels' light along each straight piece of
 * its path: from tables for the piece's direction (see Line and draw_line()), which pay for
 * themselves once enough pieces share it, or else directly (see draw_line_direct()).
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

    /// The directions lines are drawn from tables for, around a full turn, every 0.088 degrees: a
    /// piece of a path takes the two either side of it, in proportion. Even a round PSF needs
    /// them this close, as pixels collect it, since the pixels are square.
    static constexpr int LINE_DIRECTIONS = 4096;

    /// The most memory the tables lines are drawn from are kept in: beyond it, those used least
    /// recently are dropped, and made again if needed.
    static constexpr std::size_t LINE_CACHE_BUDGET = 8 * MEMORY_BUDGET;

    /// The largest disc, by its radius in pixel widths, drawn exactly rather than from samples
    /// (see disc()). A pixel's light from a smaller one bends sharply within its radius of the
    /// pixel's edges, which a spline through samples every SAMPLE_STEP / 4 pixels rings across:
    /// by about 2e-3 of the source's light at a radius of 0.15, and 4e-2 at 0.01, against 6e-5
    /// at this one.
    static constexpr double EXACT_DISC = 0.5;

    GridPsf(PSF<TSpectral>& psf, double aspect);
    GridPsf(const GridPsf& in_focus, double blur);
    static GridPsf disc(double blur, double aspect);

    /**
     * @brief What a line in one direction (cos, sin), in pixels along the axes, is drawn from: the
     * light per unit length it puts in a pixel whose center is p along the normal (-sin, cos) from
     * it, and q along it from where it starts, if it runs from -infinity to there: the pixels'
     * light (as pixel() gives it) integrated along it, C(p, q). The whole line's is C(p, infinity),
     * the line spread. Both are tabulated every step (LINE_TABLE_STEP, or as the PSF was sampled)
     * from first, in p and in q, out to where the PSF reaches (support), as cubic splines'
     * coefficients in single precision, channel by channel at each: C by row of p.
     */
    struct Line {
        double cos = 1.0;
        double sin = 0.0;
        double first = 0.0;
        double step = 1.0;
        int count = 0;
        double support = 0.0;
        std::vector<float> spread;
        std::vector<float> cumulative;
        [[nodiscard]] std::size_t memory_bytes() const
        {
            return (spread.size() + cumulative.size()) * sizeof(float);
        }
    };

    /// The directions either side of a piece's, in pixels along the axes, as indices around a
    /// turn, and how far it is from the first towards the second.
    struct LineDirections {
        int below = 0;
        int above = 0;
        double fraction = 0.0;
    };
    [[nodiscard]] static LineDirections line_directions(double dx, double dy);
    [[nodiscard]] std::size_t line_table_bytes() const;
    [[nodiscard]] double line_table_work() const;
    [[nodiscard]] double line_direct_work(double length, double reach) const;
    void prepare_line(int direction) const;

    [[nodiscard]] double line_envelope(std::size_t channel, double r) const;

    /// The cells a line's faint wings are interpolated across, pixels on a side (see
    /// draw_line_pixels_()).
    static constexpr int WING_CELL = 4;
    /// How many times the tolerance a cell's light may come to, by the line's envelope, for it to
    /// be interpolated (see draw_line_pixels_()).
    static constexpr double WING_LIGHT = 100.0;

    /**
     * @brief How far off a line may be in a pixel by interpolating across its faint wings (see
     * draw_line_pixels_()): the electrons each channel of the light it draws in a pixel, as a
     * fraction of its power, comes to, and at most how many electrons interpolating may put in or
     * take from a pixel's readout, of every channel or of their sum. None by default.
     */
    struct LineTolerance {
        std::array<double, TSpectral::size()> electrons{};
        double electrons_off = 0.0;
        bool per_channel = false;
    };
    [[nodiscard]] Light line_scale() const;
    [[nodiscard]] std::size_t line_cache_bytes() const;

    template <class Weight>
    Light draw_line(Image<TSpectral>& target,
                    int x_begin,
                    int x_end,
                    int y_begin,
                    int y_end,
                    const std::array<double, 2>& start,
                    const std::array<double, 2>& end,
                    const TSpectral& power,
                    double reach,
                    const Weight& weight,
                    const LineTolerance& tolerance = {}) const;

    template <class Weight>
    Light draw_line_direct(Image<TSpectral>& target,
                           int x_begin,
                           int x_end,
                           int y_begin,
                           int y_end,
                           const std::array<double, 2>& start,
                           const std::array<double, 2>& end,
                           const TSpectral& power,
                           double reach,
                           const Weight& weight,
                           const LineTolerance& tolerance = {}) const;

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

    /// The samples' spacing, in pixels; 0 for a disc drawn exactly (see exact()).
    [[nodiscard]] double step() const { return step_; }

    /// Whether this is a disc, or a point, drawn exactly rather than from samples (see disc()).
    [[nodiscard]] bool exact() const { return exact_blur_ >= 0.0; }

    [[nodiscard]] std::size_t memory_bytes() const;

  private:
    static constexpr std::size_t N = TSpectral::size();
    /// The envelope's bins, per pixel width.
    static constexpr double ENVELOPE_BINS = 8.0;
    /// Drawing a line directly, how far off each panel's integral may be, per unit of its length,
    /// as a fraction of the brightest the collected PSF gets (see direct_setup_()).
    static constexpr double DIRECT_TOLERANCE = 1e-3;
    /// The finest step lines' tables are made with, in pixels: the light a line puts in a pixel is
    /// smooth enough, being the PSF integrated over the pixel and along the line, that a spline
    /// through it this finely is within about 1e-4 of its peak.
    static constexpr double LINE_TABLE_STEP = 1.0 / 8.0;
    /// The bins, per pixel along the axes, the panels' lengths are set for by distance.
    static constexpr double PANEL_BINS = 4.0;
    /// Coefficients beyond the samples on every side, mirrored, which the spline takes within
    /// two samples of them.
    static constexpr int MARGIN = 2;

    double aspect_ = 1.0;
    bool pixel_integrated_ = false;
    /// Whether the pixel-integrated samples sum, over the pixels, to all of the light wherever
    /// the source is, as they do when integrated from point samples: then scale() is 1.
    bool sums_to_one_ = false;
    double step_ = 1.0;
    /// For a disc drawn exactly, its radius in pixel widths, or 0 for a point; otherwise -1.
    double exact_blur_ = -1.0;
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

    GridPsf() = default;
    [[nodiscard]] static GridPsf integrated_(const GridPsf& point, double finest = SAMPLE_STEP);

    /// By distance bin, the most light per unit length a line puts in a pixel there or beyond.
    std::vector<std::array<double, N>> line_envelope_;

    /// What lines are drawn from, made when first needed and shared by copies: the PSF as pixels
    /// collect it (itself, if pixel-integrated; see integrated_()), its scale() at a pixel's
    /// center, and the tables made so far, by direction, with when each was last used.
    struct LineCache {
        // Holds once_flags and a mutex, so it is neither copied nor moved: copies of the PSF
        // share it through line_cache_.
        LineCache() = default;
        LineCache(const LineCache&) = delete;
        LineCache& operator=(const LineCache&) = delete;
        ~LineCache() = default;

        std::once_flag collected_once;
        std::shared_ptr<const GridPsf> integrated;
        std::once_flag source_once;
        std::shared_ptr<const GridPsf> source;
        std::once_flag scale_once;
        Light scale{};
        std::once_flag direct_once;
        /// Drawing a line directly: by distance from the source (PANEL_BINS per pixel along the
        /// axes), the longest panel that may start there, and the panels a line through it to
        /// the PSF's edge takes from there, both ways.
        std::vector<double> panel_limit;
        std::vector<double> panels_from;
        /// The collected PSF's coefficients in single precision, which lines are drawn directly
        /// from, in half the memory and time (see add_collected_()).
        std::vector<float> coefficients;
        std::mutex mutex;
        struct Entry {
            std::shared_ptr<const Line> line;
            std::uint64_t used = 0;
        };
        std::map<int, Entry> lines;
        std::uint64_t clock = 0;
        std::size_t bytes = 0;
    };
    std::shared_ptr<LineCache> line_cache_ = std::make_shared<LineCache>();
    [[nodiscard]] const GridPsf& collected_() const;
    [[nodiscard]] const GridPsf& line_source_() const;
    [[nodiscard]] const LineCache& direct_setup_() const;
    [[nodiscard]] double panel_length_(const LineCache& cache, double near, double far_limit) const;
    void
    add_collected_(const LineCache& cache, double x, double y, double weight, Light& sum) const;
    /// Where a line's tables are made: every step from -half steps, out to support either side.
    struct LineLayout {
        double support = 0.0;
        double step = 1.0;
        int half = 0;
        /// The quadrature's panels in each step, each one of the collected PSF's steps.
        int panels = 1;
    };
    [[nodiscard]] LineLayout line_layout_() const;
    [[nodiscard]] std::shared_ptr<const Line> line_at_(int index) const;
    [[nodiscard]] Line build_line_(double cos, double sin) const;
    /// Where along a line a pixel's offset from it is within the PSF's rectangle, and what cuts
    /// that off at either end: 0 the line's ends, 1 the rectangle's edges across x, 2 across y,
    /// or -1 if nowhere (see clip_()).
    struct Clip {
        std::array<double, 2> range{};
        std::array<int, 2> cut{0, 0};
    };
    [[nodiscard]] Clip clip_(double qx, double qy, double ex, double ey, double length) const;
    template <class Weight, class LightAt>
    Light draw_line_pixels_(Image<TSpectral>& target,
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
                            const LightAt& light_at) const;
    [[nodiscard]] static Light line_spread_(const Line& line, double p);
    [[nodiscard]] static Light line_cumulative_(const Line& line, double p, double q);

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
    [[nodiscard]] Light exact_light_(double dx, double dy) const;
    [[nodiscard]] Light
    exact_line_light_(double qx, double qy, double ex, double ey, double length) const;
    void build_line_envelope_();
    static void check_light_(const Light& total);
    void build_from_samples_(std::vector<double>& samples, int count_x, int count_y);
    void build_point_sampled_();
    void build_bounds_();
};

} // namespace huira

#include "huira_impl/cameras/psfs/grid_psf.ipp"
#include "huira_impl/cameras/psfs/grid_psf_lines.ipp"
