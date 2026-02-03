#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/scene_view.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/render/renderer.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class RasterRenderer : public Renderer<TSpectral> {
    public:
        ~RasterRenderer() override = default;

        void render(SceneView<TSpectral>& scene_view, FrameBuffer<TSpectral>& frame_buffer, float exposure_time) override;

    private:
        void rasterize_(SceneView<TSpectral>& scene_view, FrameBuffer<TSpectral>& frame_buffer);
    };
}

#include "huira_impl/render/raster_renderer.ipp"
