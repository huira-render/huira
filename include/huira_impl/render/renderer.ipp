
namespace huira {
    template <IsSpectral TSpectral>
    void Renderer<TSpectral>::render_unresolved_(
        SceneView<TSpectral>& scene_view,
        FrameBuffer<TSpectral>& frame_buffer)
    {
        // Loop over all stars:
        for (const auto& star : scene_view.stars_) {
            Pixel star_p = scene_view.camera_model_->project_point(star.get_direction());
            if (std::isnan(star_p.x) || std::isnan(star_p.y)) {
                continue;
            }

            if (frame_buffer.has_received_power()) {
                bool valid = true;
                if (frame_buffer.has_depth()) {
                    if (!std::isinf(frame_buffer.depth()(star_p))) {
                        valid = false;
                    }
                }
                if (valid) {
                    frame_buffer.received_power()(star_p) += 10000;
                }
            }
        }
    }

}
