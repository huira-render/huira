#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/fft_convolver.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/sampling/sampler.hpp"
#include "huira/scene/scene_view.hpp"
#include "huira/util/logger.hpp"

namespace huira {
/**
 * @brief Abstract base class for scene renderers.
 *
 * Renderer provides the interface and common helpers for rendering a SceneView into a FrameBuffer.
 * Derived classes implement specific rendering algorithms.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class Renderer {
  public:
    Renderer() = default;
    virtual ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    virtual void render(SceneView<TSpectral>& scene_view, FrameBuffer<TSpectral>& frame_buffer);

    /// Set the number of samples per pixel: at least 1, or every pixel would be NaN.
    void set_samples_per_pixel(int spp)
    {
        if (spp < 1) {
            HUIRA_THROW_ERROR("Renderer::set_samples_per_pixel - At least one sample per pixel is "
                              "needed: " +
                              std::to_string(spp));
        }
        spp_ = spp;
    }

    /// Set the maximum number of bounces for path tracing: at least 1, the camera ray's first
    /// hit, or every pixel would be black.
    void set_max_bounces(int max_bounces)
    {
        if (max_bounces < 1) {
            HUIRA_THROW_ERROR("Renderer::set_max_bounces - At least one bounce, the camera ray's "
                              "first hit, is needed: " +
                              std::to_string(max_bounces));
        }
        max_bounces_ = max_bounces;
    }

    /// Enable or disable dynamic sampling of pixels based on variance.
    void set_dynamic_sampling(bool dynamic_sample = true) { dynamic_sampling_ = dynamic_sample; }

    /// Set the minimum samples per pixel for dynamic sampling: at least 2, since one sample has
    /// no variance to judge by.
    void set_min_samples(int min_samples)
    {
        if (min_samples < 2) {
            HUIRA_THROW_ERROR("Renderer::set_min_samples - At least two samples are needed to "
                              "estimate a pixel's variance: " +
                              std::to_string(min_samples));
        }
        min_spp_ = min_samples;
    }

    /// Set the variance threshold for dynamic sampling: non-negative and finite (0 never stops
    /// early).
    void set_variance_threshold(float threshold)
    {
        if (!(threshold >= 0.f) || !std::isfinite(threshold)) {
            HUIRA_THROW_ERROR("Renderer::set_variance_threshold - The threshold must be "
                              "non-negative and finite: " +
                              std::to_string(threshold));
        }
        variance_threshold_ = threshold;
    }

    /// Set the indirect lighting clamp threshold: non-negative, or infinite (the default) for
    /// none. A negative one would make indirect light negative.
    void set_indirect_clamp(float indirect_clamp)
    {
        if (!(indirect_clamp >= 0.f)) {
            HUIRA_THROW_ERROR("Renderer::set_indirect_clamp - The clamp must be non-negative, or "
                              "infinite for none: " +
                              std::to_string(indirect_clamp));
        }
        indirect_clamp_threshold_ = indirect_clamp;
    }

    /// Enable or disable occlusion testing of unresolved point sources.
    void set_unresolved_occlusion(bool occlusion = true) { unresolved_occlusion_ = occlusion; }

    /// How faint, as a fraction of the sensor's read noise, the light an unresolved source puts
    /// in a pixel gets before it is tapered off. See set_unresolved_taper().
    static constexpr double DEFAULT_UNRESOLVED_TAPER = 1e-3;

    void set_unresolved_taper(double fraction_of_read_noise);

    /// See set_unresolved_taper().
    [[nodiscard]] double unresolved_taper() const { return unresolved_taper_; }

    /// How much of the read noise the rounding of convolving bodies with the PSF in single
    /// precision may put in a pixel's readout before they are convolved in double precision, by
    /// default. See set_fft_rounding_tolerance().
    static constexpr double DEFAULT_FFT_ROUNDING_TOLERANCE = 0.1;

    void set_fft_rounding_tolerance(double fraction_of_read_noise);

    /// See set_fft_rounding_tolerance().
    [[nodiscard]] double fft_rounding_tolerance() const { return fft_rounding_tolerance_; }

    /**
     * @brief How hard FFTW plans the transforms that convolve bodies with the PSF (see
     * FftPlanEffort). Estimate, the default, plans at once. Measure finds faster transforms,
     * which convolve a 1920 x 1080 Visible8 frame in about 15% less time, but takes 2 to 5
     * seconds the first time a size and precision are used in a process: worth it for long
     * sequences of frames.
     */
    void set_fft_planning(FftPlanEffort effort) { fft_planning_ = effort; }

    /// See set_fft_planning().
    [[nodiscard]] FftPlanEffort fft_planning() const { return fft_planning_; }

    /**
     * @brief Enable or disable skipping screen regions that provably contain no geometry.
     *
     * Enabled by default. Purely an optimization: the image is identical either way.
     */
    void set_region_culling(bool enable = true) { region_culling_ = enable; }

    /// Scale the safety margin added to each tile's direction cone: finite, and taken as 1 if
    /// less.
    void set_region_cull_margin_scale(float scale)
    {
        if (!std::isfinite(scale)) {
            HUIRA_THROW_ERROR(
                "Renderer::set_region_cull_margin_scale - The scale must be finite: " +
                std::to_string(scale));
        }
        region_cull_margin_scale_ = std::max(1.0f, scale);
    }

    /// (CI): Trace culled regions anyway and report any that turn out to contain geometry.
    void set_region_cull_validation(bool enable = true) { region_cull_validation_ = enable; }

  protected:
    virtual Image<TSpectral> path_trace_(SceneView<TSpectral>& scene_view,
                                         FrameBuffer<TSpectral>& frame_buffer);

    virtual Image<TSpectral> render_unresolved_(SceneView<TSpectral>& scene_view,
                                                FrameBuffer<TSpectral>& frame_buffer);

    std::shared_ptr<CameraModel<TSpectral>> get_camera(SceneView<TSpectral>& scene_view) const
    {
        return scene_view.camera_model_;
    }
    std::vector<PrimitiveBatch<TSpectral>> get_primitives(SceneView<TSpectral>& scene_view) const
    {
        return scene_view.primitives_;
    }
    std::vector<LightInstance<TSpectral>> get_lights(SceneView<TSpectral>& scene_view) const
    {
        return scene_view.lights_;
    }

    /// A set of ray directions, as a cone about a unit axis.
    ///
    /// Region culling works at the angular scale of a pixel, which for a narrow field of view is
    /// far smaller than an arc cosine of a single-precision dot product can resolve: near 1, the
    /// cosine changes too little. So angles are measured in double precision, between directions
    /// by atan2 of their cross and dot products (angle_between_()), and between cones by the
    /// chord between their axes, which keeps its precision at small angles.
    struct DirectionCone {
        Vec3<double> axis{0.0, 0.0, 1.0};
        double half_angle = 0.0;
        double sin_quarter = 0.0; ///< sin(half_angle / 2)
        double cos_quarter = 1.0; ///< cos(half_angle / 2)

        void set_half_angle(double angle)
        {
            half_angle = angle;
            sin_quarter = std::sin(0.5 * angle);
            cos_quarter = std::cos(0.5 * angle);
        }

        /// Whether two cones share a direction: their axes are no further apart than the sum of
        /// their half-angles, compared by the chord between the axes, 2 sin(angle / 2).
        [[nodiscard]] bool overlaps(const DirectionCone& other) const
        {
            if (half_angle + other.half_angle >= PI<double>()) {
                return true;
            }
            // sin((a + b) / 2), from the halves of each half-angle, which is positive as
            // (a + b) / 2 is below a right angle, so the squared chords compare:
            const double sin_half_sum =
                sin_quarter * other.cos_quarter + cos_quarter * other.sin_quarter;
            const Vec3<double> chord = axis - other.axis;
            return glm::dot(chord, chord) <= 4.0 * sin_half_sum * sin_half_sum;
        }
    };

    /// The angle between two unit directions, precise at any size.
    [[nodiscard]] static double angle_between_(const Vec3<double>& a, const Vec3<double>& b)
    {
        return std::atan2(glm::length(glm::cross(a, b)), glm::dot(a, b));
    }

    /// Cones subtending each TLAS occupant as seen from the camera.
    bool build_occupancy_cones_(SceneView<TSpectral>& scene_view,
                                std::vector<DirectionCone>& cones) const;

    /// Cone enclosing every ray direction a tile can produce.
    DirectionCone tile_direction_cone_(
        const CameraModel<TSpectral>& camera, float x0, float y0, float x1, float y1) const;

    [[nodiscard]] static bool image_is_zero_(const Image<TSpectral>& image);

    RandomSampler<float> sampler_;

    /// Conservative screen-space record of where an occluder may lie.  */
    Image<uint8_t> occluder_mask_;

    /// False until path_trace_() has filled occluder_mask_ for the current frame.
    /// While false, every unresolved source is ray tested.
    bool occluder_mask_valid_ = false;

    // Settings
    int spp_ = 10;
    int max_bounces_ = 3;

    bool unresolved_occlusion_ = true;
    double unresolved_taper_ = DEFAULT_UNRESOLVED_TAPER;
    FftPlanEffort fft_planning_ = FftPlanEffort::Estimate;
    double fft_rounding_tolerance_ = DEFAULT_FFT_ROUNDING_TOLERANCE;

    bool region_culling_ = true;
    bool region_cull_validation_ = false;
    float region_cull_margin_scale_ = 1.0f;

    bool dynamic_sampling_ = false;
    int min_spp_ = 16;
    float variance_threshold_ = 0.001f;

    float indirect_clamp_threshold_ = std::numeric_limits<float>::infinity();
};
} // namespace huira

#include "huira_impl/render/renderer.ipp"
