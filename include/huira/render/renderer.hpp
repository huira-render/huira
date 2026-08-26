#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/fft_convolver.hpp"
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
 * @tparam TSpectral The spectral type (e.g., RGB, Visible8)
 */
template <IsSpectral TSpectral>
class Renderer {
  public:
    virtual ~Renderer() = default;

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

    /// Enable or disable occlusion testing of unresolved point sources.  */
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

    /// @brief A persistent FFT convolver plus the configuration it was built for.
    struct ConvolverCache {
        FftConvolver<TSpectral> convolver;
        const void* camera = nullptr;
        std::uint64_t kernel_version = 0;
        int image_width = 0;
        int image_height = 0;
        int kernel_width = 0;
        int kernel_height = 0;
        bool valid = false;
    };

    void convolve_cached_(Image<TSpectral>& image,
                          const Image<TSpectral>& kernel,
                          const CameraModel<TSpectral>& camera,
                          ConvolverCache& cache);

    /// Scattered-light wings applied to the unresolved splat buffer, once per frame.
    ConvolverCache wings_convolver_;

    /// Composite PSF applied to the whole unresolved image in the defocus path.
    ConvolverCache defocus_convolver_;

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

    bool dynamic_sampling_ = false;
    int min_spp_ = 16;
    float variance_threshold_ = 0.001f;

    float indirect_clamp_threshold_ = std::numeric_limits<float>::infinity();
};
} // namespace huira

#include "huira_impl/render/renderer.ipp"
