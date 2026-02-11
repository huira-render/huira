#include <cmath>
#include <algorithm>
#include <vector>
#include <limits>

#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"

#include "huira/core/types.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    struct RadiusLUTConfig {
        float photon_threshold = 0.1f; // photon count that counts as "visible" (0.1 photons per second)
        int min_radius = 1;            // never go below this
    };

    struct RadiusLUTEntry {
        int   radius;
        float min_irradiance; 
    };

    // Build the radius -> irradiance LUT from the center kernel.
    template <IsSpectral TSpectral>
    static std::vector<RadiusLUTEntry> build_radius_lut(
        const Image<TSpectral>& center_kernel,
        int full_radius,
        float area,
        const TSpectral& photon_energies,
        const RadiusLUTConfig& config = {})
    {
        std::vector<RadiusLUTEntry> lut;
        lut.reserve(static_cast<std::size_t>(full_radius));

        // Precompute per-channel conversion: area / photon_energy[c]
        TSpectral conversion;
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            float e = photon_energies[c];
            conversion[c] = (e > 0.0f) ? (area / e) : 0.0f;
        }

        for (int r = 1; r <= full_radius; ++r) {
            // For this ring, find the minimum irradiance across all pixels
            // and channels that would produce `threshold` photons.
            float max_sensitivity = 0.0f;
            auto scan_pixel = [&](int kx, int ky) {
                if (kx < 0 || kx >= center_kernel.width() ||
                    ky < 0 || ky >= center_kernel.height()) {
                    return;
                }

                const TSpectral& w = center_kernel(kx, ky);
                for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                    float s = w[c] * conversion[c];
                    max_sensitivity = std::max(max_sensitivity, s);
                }
                };

            // Top & bottom rows of the ring
            for (int dx = -r; dx <= r; ++dx) {
                scan_pixel(full_radius + dx, full_radius - r);
                scan_pixel(full_radius + dx, full_radius + r);
            }

            // Left & right columns (excluding corners)
            for (int dy = -r + 1; dy <= r - 1; ++dy) {
                scan_pixel(full_radius - r, full_radius + dy);
                scan_pixel(full_radius + r, full_radius + dy);
            }

            if (max_sensitivity > 0.0f) {
                float min_irradiance = config.photon_threshold / max_sensitivity;
                lut.push_back({ r, min_irradiance });
            }
        }

        return lut;
    }

    // Look up the effective radius for a star with the given irradiance.
    static int lookup_effective_radius(
        const std::vector<RadiusLUTEntry>& lut,
        float max_irradiance,
        int min_radius)
    {
        if (lut.empty()) {
            return min_radius;
        }

        for (int i = static_cast<int>(lut.size()) - 1; i >= 0; --i) {
            if (max_irradiance >= lut[static_cast<std::size_t>(i)].min_irradiance) {
                return std::max(lut[static_cast<std::size_t>(i)].radius, min_radius);
            }
        }

        return min_radius;
    }

    // Generic point to be rendered (UnresolvedObject or Star):
    template <IsSpectral TSpectral>
    struct RenderItem {
        Vec3<float> point;
        TSpectral   irradiance;
        int         effective_radius;
    };


    template <IsSpectral TSpectral>
    void Renderer<TSpectral>::render_unresolved_(
        SceneView<TSpectral>& scene_view,
        FrameBuffer<TSpectral>& frame_buffer)
    {
        auto& camera = scene_view.camera_model_;

        const int fb_width = frame_buffer.width();
        const int fb_height = frame_buffer.height();

        const int full_radius = camera->has_psf() ? camera->get_psf_radius() : 0;

        // Collect all unresolved points (stars + UnresolvedObjects) in a single list for processing:
        std::vector<RenderItem<TSpectral>> items;
        items.reserve(scene_view.stars_.size() + scene_view.unresolved_objects_.size());

        for (const auto& star : scene_view.stars_) {
            items.push_back({ star.get_direction(), star.get_irradiance(), full_radius });
        }
        for (const auto& instance : scene_view.unresolved_objects_) {
            items.push_back({ instance.transform.position,
                              instance.unresolved_object->get_irradiance(),
                              full_radius });
        }

        if (items.empty()) {
            return;
        }

        // Build radius LUT and assign per-star radii:
        if (camera->has_psf() && full_radius > 1) {
            const Image<TSpectral>& center_kernel =
                camera->get_psf_kernel(0.0f, 0.0f);

            // On-axis area is conservative:
            float representative_area =
                camera->get_projected_aperture_area(Vec3<float>{0.f, 0.f, -1.f});

            // Per-channel photon energies:
            TSpectral photon_energies = TSpectral::photon_energies();

            RadiusLUTConfig config;
            std::vector<RadiusLUTEntry> radius_lut =
                build_radius_lut(center_kernel, full_radius,
                    representative_area,
                    photon_energies, config);

            // Per-star lookup - just a scalar comparison, no kernel traversal.
            for (auto& item : items) {
                item.effective_radius =
                    lookup_effective_radius(radius_lut, item.irradiance.max(),
                        config.min_radius);
            }
        }

        // Project and bin into tiles
        constexpr int TILE_SIZE = 64;

        int tiles_x = (fb_width + TILE_SIZE - 1) / TILE_SIZE;
        int tiles_y = (fb_height + TILE_SIZE - 1) / TILE_SIZE;
        int num_tiles = tiles_x * tiles_y;

        float res_x = static_cast<float>(camera->resolution().x);
        float res_y = static_cast<float>(camera->resolution().y);

        struct ProjectedItem {
            std::size_t item_idx;
            Pixel       projected;
        };

        std::vector<std::vector<ProjectedItem>> tile_bins(
            static_cast<std::size_t>(num_tiles));

        for (std::size_t i = 0; i < items.size(); ++i) {
            Pixel p = camera->project_point(items[i].point);
            if (std::isnan(p.x) || std::isnan(p.y)) continue;
            if (p.x < 0.f || p.x > res_x || p.y < 0.f || p.y > res_y) continue;

            int tx = std::clamp(static_cast<int>(p.x) / TILE_SIZE, 0, tiles_x - 1);
            int ty = std::clamp(static_cast<int>(p.y) / TILE_SIZE, 0, tiles_y - 1);

            tile_bins[static_cast<std::size_t>(ty * tiles_x + tx)].push_back({ i, p });
        }

        // Render tiles in parallel:
        auto& power_buffer = frame_buffer.received_power();
        const auto& depth_buffer = frame_buffer.depth();
        bool has_depth = frame_buffer.has_depth();
        bool has_power = frame_buffer.has_received_power();

        if (!has_power) {
            return;
        }

        int margin = full_radius;

        struct TileBuffer {
            Image<TSpectral> buf;
            int origin_x = 0;
            int origin_y = 0;
            int local_w = 0;
            int local_h = 0;
        };

        std::vector<TileBuffer> tile_buffers(static_cast<std::size_t>(num_tiles));

        tbb::parallel_for(tbb::blocked_range<int>(0, num_tiles),
            [&](const tbb::blocked_range<int>& range) {
                for (int tile_idx = range.begin(); tile_idx < range.end(); ++tile_idx) {
                    const auto& bin = tile_bins[static_cast<std::size_t>(tile_idx)];
                    if (bin.empty()) {
                        continue;
                    }

                    int tile_y = tile_idx / tiles_x;
                    int tile_x = tile_idx % tiles_x;

                    int tile_x0 = tile_x * TILE_SIZE;
                    int tile_y0 = tile_y * TILE_SIZE;

                    int local_x0 = std::max(0, tile_x0 - margin);
                    int local_y0 = std::max(0, tile_y0 - margin);
                    int local_x1 = std::min(fb_width, tile_x0 + TILE_SIZE + margin);
                    int local_y1 = std::min(fb_height, tile_y0 + TILE_SIZE + margin);

                    int local_w = local_x1 - local_x0;
                    int local_h = local_y1 - local_y0;

                    Image<TSpectral> local_buf(local_w, local_h);

                    for (const auto& proj : bin) {
                        const auto& item = items[proj.item_idx];
                        const Pixel& star_p = proj.projected;

                        bool unobstructed = true;
                        if (has_depth) {
                            if (!std::isinf(depth_buffer(star_p))) {
                                unobstructed = false;
                            }
                        }
                        if (!unobstructed) {
                            continue;
                        }

                        float projected_area =
                            camera->get_projected_aperture_area(item.point);
                        TSpectral power = item.irradiance * projected_area;

                        if (camera->has_psf()) {
                            float floor_x = std::floor(star_p.x);
                            float floor_y = std::floor(star_p.y);
                            float frac_x = star_p.x - floor_x;
                            float frac_y = star_p.y - floor_y;

                            const Image<TSpectral>& kernel =
                                camera->get_psf_kernel(frac_x, frac_y);

                            int eff_r = item.effective_radius;
                            int k_offset = full_radius - eff_r;
                            int crop_dim = 2 * eff_r + 1;

                            int start_x = static_cast<int>(floor_x) - eff_r;
                            int start_y = static_cast<int>(floor_y) - eff_r;

                            int kx_begin = std::max(0, local_x0 - start_x);
                            int kx_end = std::min(crop_dim, local_x1 - start_x);

                            int ky_begin = std::max(0, local_y0 - start_y);
                            int ky_end = std::min(crop_dim, local_y1 - start_y);

                            for (int ky = ky_begin; ky < ky_end; ++ky) {
                                int img_y = start_y + ky;
                                int ly = img_y - local_y0;

                                for (int kx = kx_begin; kx < kx_end; ++kx) {
                                    int img_x = start_x + kx;
                                    int lx = img_x - local_x0;

                                    local_buf(lx, ly) +=
                                        power * kernel(kx + k_offset, ky + k_offset);
                                }
                            }
                        }
                        else {
                            int px = static_cast<int>(std::round(star_p.x));
                            int py = static_cast<int>(std::round(star_p.y));
                            if (px >= local_x0 && px < local_x1 &&
                                py >= local_y0 && py < local_y1) {
                                local_buf(px - local_x0, py - local_y0) += power;
                            }
                        }
                    }

                    auto& tb = tile_buffers[static_cast<std::size_t>(tile_idx)];
                    tb.buf = std::move(local_buf);
                    tb.origin_x = local_x0;
                    tb.origin_y = local_y0;
                    tb.local_w = local_w;
                    tb.local_h = local_h;
                }
            });

        // Combine all Tiles:
        std::vector<std::vector<int>> row_tiles(static_cast<std::size_t>(fb_height));
        for (int t = 0; t < num_tiles; ++t) {
            const auto& tb = tile_buffers[static_cast<std::size_t>(t)];
            if (tb.local_w == 0) continue;
            for (int y = tb.origin_y; y < tb.origin_y + tb.local_h; ++y) {
                row_tiles[static_cast<std::size_t>(y)].push_back(t);
            }
        }

        tbb::parallel_for(tbb::blocked_range<int>(0, fb_height),
            [&](const tbb::blocked_range<int>& range) {
                for (int y = range.begin(); y < range.end(); ++y) {
                    for (int t : row_tiles[static_cast<std::size_t>(y)]) {
                        const auto& tb = tile_buffers[static_cast<std::size_t>(t)];
                        int ly = y - tb.origin_y;
                        for (int lx = 0; lx < tb.local_w; ++lx) {
                            const TSpectral& val = tb.buf(lx, ly);
                            bool nonzero = false;
                            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                                if (val[c] != 0.0f) { nonzero = true; break; }
                            }
                            if (nonzero) {
                                power_buffer(tb.origin_x + lx, y) += val;
                            }
                        }
                    }
                }
            });
    }
}
