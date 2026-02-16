#pragma once

#include <memory>
#include <vector>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/render/sampler.hpp"
#include "huira/scene/scene_view.hpp"

namespace huira {
    /**
     * @brief Abstract base class for scene renderers.
     *
     * Renderer provides the interface and common helpers for rendering a SceneView into a FrameBuffer.
     * Derived classes implement specific rendering algorithms (e.g., rasterization, ray tracing).
     *
     * @tparam TSpectral Spectral type for the rendering pipeline
     */
    template <IsSpectral TSpectral>
    class Renderer {
    public:
        virtual ~Renderer() = default;

        virtual void render(SceneView<TSpectral>& scene_view, FrameBuffer<TSpectral>& frame_buffer, float exposure_time) = 0;

    protected:
        void render_unresolved_(SceneView<TSpectral>& scene_view, FrameBuffer<TSpectral>& frame_buffer);

        std::shared_ptr<CameraModel<TSpectral>> get_camera(SceneView<TSpectral>& scene_view) const { return scene_view.camera_model_; }
        std::vector<MeshBatch<TSpectral>> get_meshes(SceneView<TSpectral>& scene_view) const { return scene_view.geometry_; }
        std::vector<LightInstance<TSpectral>> get_lights(SceneView<TSpectral>& scene_view) const { return scene_view.lights_; }

        RandomSampler<float> sampler_;
    };
}

#include "huira_impl/render/renderer.ipp"
