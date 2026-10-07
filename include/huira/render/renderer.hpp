#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/sampling/sampler.hpp"
#include "huira/scene/scene_view.hpp"

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

    /// Set the number of samples per pixel.
    void set_samples_per_pixel(int spp) { spp_ = spp; }

    /// Set the maximum number of bounces for path tracing.
    void set_max_bounces(int max_bounces) { max_bounces_ = max_bounces; }

    /// Enable or disable dynamic sampling of pixels based on variance.
    void set_dynamic_sampling(bool dynamic_sample = true) { dynamic_sampling_ = dynamic_sample; }

    /// Set the minimum samples per pixel for dynamic sampling.
    void set_min_samples(int min_samples) { min_spp_ = min_samples; }

    /// Set the variance threshold for dynamic sampling.
    void set_variance_threshold(float threshold) { variance_threshold_ = threshold; }

    /// Set the indirect lighting clamp threshold.
    void set_indirect_clamp(float indirect_clamp) { indirect_clamp_threshold_ = indirect_clamp; }

    /// Enable or disable occlusion testing of unresolved point sources.
    void set_unresolved_occlusion(bool occlusion = true) { unresolved_occlusion_ = occlusion; }

    /**
     * @brief Crop each unresolved source's PSF stamp to the pixels its light would show in (the
     * default), or always stamp the whole PSF.
     *
     * Cropped, a source's stamp keeps the pixels that receive at least a tenth of the sensor's
     * read noise over the exposure (a tenth of an electron for a sensor without noise), and is
     * scaled back to the whole stamp's energy, so that no light is lost: what lay beyond the
     * crop is spread over the pixels kept. A stamp is never cropped so far that the source's
     * centroid moves by more than 0.01 px. Faint stars then cost a few pixels each rather than
     * the whole stamp.
     */
    void set_stamp_cropping(bool enable = true) { stamp_cropping_ = enable; }

    /**
     * @brief Enable or disable skipping screen regions that provably contain no geometry.
     *
     * Enabled by default. Purely an optimization: the image is identical either way.
     */
    void set_region_culling(bool enable = true) { region_culling_ = enable; }

    /// Scale the safety margin added to each tile's direction cone.
    void set_region_cull_margin_scale(float scale)
    {
        region_cull_margin_scale_ = std::max(1.0f, scale);
    }

    /// (CI): Trace culled regions anyway and report any that turn out to contain geometry.
    void set_region_cull_validation(bool enable = true) { region_cull_validation_ = enable; }

  protected:
    virtual Image<TSpectral> path_trace_(SceneView<TSpectral>& scene_view,
                                         FrameBuffer<TSpectral>& frame_buffer);

    virtual Image<TSpectral> render_unresolved_(SceneView<TSpectral>& scene_view,
                                                FrameBuffer<TSpectral>& frame_buffer,
                                                Image<TSpectral>& wing_splat);

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
    struct DirectionCone {
        Vec3<float> axis{0.f, 0.f, 1.f};
        float half_angle = 0.f;
    };

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
    bool stamp_cropping_ = true;

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
