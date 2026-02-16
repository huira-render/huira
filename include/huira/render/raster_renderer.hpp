#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/render/renderer.hpp"
#include "huira/scene/scene_view.hpp"

namespace huira {
    /**
     * @brief Simple rasterization-based renderer for mesh geometry.
     *
     * RasterRenderer implements a basic triangle rasterization pipeline for rendering
     * mesh geometry into a frame buffer. It supports per-pixel lighting, mesh ID, and
     * normal output, and is intended for fast, reference-style rendering of scene views.
     *
     * @tparam TSpectral Spectral type for the rendering pipeline
     */
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
