#include <cmath>

#include "huira/core/types.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void Renderer<TSpectral>::render_unresolved_(
        SceneView<TSpectral>& scene_view,
        FrameBuffer<TSpectral>& frame_buffer)
    {
        auto& camera = scene_view.camera_model_;
        const int fb_width = frame_buffer.width();
        const int fb_height = frame_buffer.height();
        const auto& depth_buffer = frame_buffer.depth();
        auto& power_buffer = frame_buffer.received_power();

        // Loop over all stars:
        for (const auto& star : scene_view.stars_) {
            Pixel star_p = camera->project_point(star.get_direction());
            if (std::isnan(star_p.x) || std::isnan(star_p.y)) {
                continue;
            }

            if (frame_buffer.has_received_power()) {
                bool unobstructed = true;
                if (frame_buffer.has_depth()) {
                    if (!std::isinf(depth_buffer(star_p))) {
                        unobstructed = false;
                    }
                }

                if (unobstructed) {
                    // Compute received energy:
                    TSpectral irradiance = star.get_irradiance();
                    float projected_area = camera->get_projected_aperture_area(star.get_direction());
                    TSpectral power = irradiance * projected_area;

                    if (camera->has_psf()) {
                        float floor_x = std::floor(star_p.x);
                        float floor_y = std::floor(star_p.y);
                        float frac_x = star_p.x - floor_x;
                        float frac_y = star_p.y - floor_y;

                        const Image<TSpectral>& kernel = camera->get_psf_kernel(frac_x, frac_y);
                        int radius = camera->get_psf_radius();

                        int start_x = static_cast<int>(floor_x) - radius;
                        int start_y = static_cast<int>(floor_y) - radius;

                        int k_w = static_cast<int>(kernel.width());
                        int k_h = static_cast<int>(kernel.height());

                        int kx_begin = std::max(0, -start_x);
                        int kx_end = std::min(k_w, fb_width - start_x);

                        int ky_begin = std::max(0, -start_y);
                        int ky_end = std::min(k_h, fb_height - start_y);
                        
                        for (int ky = ky_begin; ky < ky_end; ++ky) {
                            int img_y = start_y + ky;

                            for (int kx = kx_begin; kx < kx_end; ++kx) {
                                int img_x = start_x + kx;

                                power_buffer(img_x, img_y) += power * kernel(ky, kx);
                            }
                        }
                    }
                    else {
                        power_buffer(star_p) += power;
                    }                    
                }
            }
        }
    }

}
