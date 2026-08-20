#pragma once

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
 * @tparam TSpectral Spectral type for the rendering pipeline
 */
template <IsSpectral TSpectral>
class Renderer {
  public:
    virtual ~Renderer() = default;

    virtual void render(SceneView<TSpectral>& scene_view, FrameBuffer<TSpectral>& frame_buffer);

    /**
     * @brief Set the number of samples per pixel.
     */
    void set_samples_per_pixel(int spp) { spp_ = spp; }

    /**
     * @brief Set the maximum number of bounces for path tracing.
     */
    void set_max_bounces(int max_bounces) { max_bounces_ = max_bounces; }

    /**
     * @brief Enable or disable dynamic sampling of pixels based on variance.
     */
    void set_dynamic_sampling(bool dynamic_sample = true) { dynamic_sampling_ = dynamic_sample; }

    /**
     * @brief Set the minimum samples per pixel for dynamic sampling.
     */
    void set_min_samples(int min_samples) { min_spp_ = min_samples; }

    /**
     * @brief Set the variance threshold for dynamic sampling.
     */
    void set_variance_threshold(float threshold) { variance_threshold_ = threshold; }

    /**
     * @brief Set the indirect lighting clamp threshold.
     */
    void set_indirect_clamp(float indirect_clamp) { indirect_clamp_threshold_ = indirect_clamp; }

    /**
     * @brief Enable or disable occlusion testing of unresolved point sources.  (Enabled by default)
     */
    void set_unresolved_occlusion(bool occlusion = true) { unresolved_occlusion_ = occlusion; }

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

    RandomSampler<float> sampler_;

    /**
     * @brief Conservative screen-space record of where an occluder may lie.
     */
    Image<uint8_t> occluder_mask_;

    /// False until path_trace_() has filled occluder_mask_ for the current frame.
    /// While false, every unresolved source is ray tested.
    bool occluder_mask_valid_ = false;

    // Settings
    int spp_ = 10;
    int max_bounces_ = 3;

    bool unresolved_occlusion_ = true;

    bool dynamic_sampling_ = false;
    int min_spp_ = 16;
    float variance_threshold_ = 0.001f;

    float indirect_clamp_threshold_ = std::numeric_limits<float>::infinity();
};
} // namespace huira

#include "huira_impl/render/renderer.ipp"
