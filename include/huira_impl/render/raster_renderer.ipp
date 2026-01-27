#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/core/scene_view.hpp"
#include "huira/render/frame_buffer.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void RasterRenderer<TSpectral>::render(
        SceneView<TSpectral>& scene_view,
        FrameBuffer<TSpectral>& frame_buffer)
    {
        (void)scene_view;
        (void)frame_buffer;
    }
}
