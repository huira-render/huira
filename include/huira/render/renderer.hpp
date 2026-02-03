#pragma once

#include <vector>
#include <memory>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/sampler.hpp"
#include "huira/scene/scene_view.hpp"
#include "huira/render/frame_buffer.hpp"

namespace huira {
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
