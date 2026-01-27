#pragma once

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/core/scene_view.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/render/renderer.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class RasterRenderer : public Renderer<TSpectral> {
    public:
        ~RasterRenderer() = default;

        void render(SceneView<TSpectral>& scene_view, FrameBuffer<TSpectral>& frame_buffer) override;
    };
}

#include "huira_impl/render/raster_renderer.ipp"
