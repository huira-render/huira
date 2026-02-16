#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
    /**
     * @brief Configuration parameters for the radius lookup table.
     * 
     * Controls how the effective PSF radius is determined based on photon count thresholds
     * and minimum radius constraints.
     */
    struct RadiusLUTConfig {
        float photon_threshold = 0.1f; // photon count that counts as "visible" (0.1 photons per second)
        int min_radius = 1;            // never go below this
    };

    /**
     * @brief Entry in the radius lookup table.
     * 
     * Maps a PSF radius to the minimum irradiance threshold that requires that radius
     * for accurate rendering.
     */
    struct RadiusLUTEntry {
        int   radius;
        float min_irradiance; 
    };

    /**
     * @brief Build a lookup table mapping PSF radius to minimum irradiance thresholds.
     * 
     * This function analyzes the PSF kernel to determine, for each radius from 1 to full_radius,
     * what minimum irradiance level would produce at least photon_threshold photons per second
     * at the highest-sensitivity pixel within that radius ring. The resulting LUT allows efficient
     * per-star radius culling: stars with low irradiance can use smaller PSF kernels without
     * visible quality loss.
     * 
     * @tparam TSpectral Spectral type for the rendering pipeline
     * @param center_kernel The full PSF kernel centered at (0,0) offset
     * @param full_radius_signed The maximum PSF radius in pixels
     * @param area The projected aperture area for photon flux calculations
     * @param photon_energies Per-channel photon energies for flux-to-photon conversion
     * @param config Configuration parameters for LUT generation
     * @return std::vector<RadiusLUTEntry> Lookup table sorted by increasing radius
     */
    template <IsSpectral TSpectral>
    static std::vector<RadiusLUTEntry> build_radius_lut(
        const Image<TSpectral>& center_kernel,
        int full_radius_signed,
        float area,
        const TSpectral& photon_energies,
        const RadiusLUTConfig& config = {})
    {
        std::vector<RadiusLUTEntry> lut;
        if (full_radius_signed <= 0) {
            return lut;
        }

        const std::size_t full_radius = static_cast<std::size_t>(full_radius_signed);
        const std::size_t k_w = static_cast<std::size_t>(center_kernel.width());
        const std::size_t k_h = static_cast<std::size_t>(center_kernel.height());

        lut.reserve(full_radius);

        // Precompute per-channel conversion: area / photon_energy[c]
        TSpectral conversion;
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            float e = photon_energies[c];
            conversion[c] = (e > 0.0f) ? (area / e) : 0.0f;
        }

        // Helper: accumulate max sensitivity from a single kernel pixel.
        auto scan_pixel = [&](std::size_t kx, std::size_t ky, float& max_sensitivity) {
            if (kx >= k_w || ky >= k_h) {
                return;
            }
            const TSpectral& w = center_kernel(static_cast<int>(kx), static_cast<int>(ky));
            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                float s = w[c] * conversion[c];
                max_sensitivity = std::max(max_sensitivity, s);
            }
            };

        for (std::size_t r = 1; r <= full_radius; ++r) {
            float max_sensitivity = 0.0f;

            // Absolute kernel coordinates for this ring's bounding box.
            // r <= full_radius, so lo >= 0 and hi <= 2*full_radius.
            const std::size_t lo = full_radius - r;
            const std::size_t hi = full_radius + r;

            // Top & bottom rows: kx spans [lo .. hi]
            for (std::size_t kx = lo; kx <= hi; ++kx) {
                scan_pixel(kx, lo, max_sensitivity);
                scan_pixel(kx, hi, max_sensitivity);
            }

            // Left & right columns (excluding corners): ky spans (lo .. hi)
            for (std::size_t ky = lo + 1; ky < hi; ++ky) {
                scan_pixel(lo, ky, max_sensitivity);
                scan_pixel(hi, ky, max_sensitivity);
            }

            if (max_sensitivity > 0.0f) {
                float min_irradiance = config.photon_threshold / max_sensitivity;
                lut.push_back({ static_cast<int>(r), min_irradiance });
            }
        }

        return lut;
    }

    /**
     * @brief Look up the effective PSF radius for a given irradiance level.
     * 
     * Searches the radius LUT to find the smallest radius that can accurately render
     * a point source with the specified maximum irradiance. The effective radius is
     * clamped to be at least min_radius.
     * 
     * @param lut The radius lookup table (result of build_radius_lut)
     * @param max_irradiance The maximum irradiance of the point source across all channels
     * @param min_radius Minimum allowed radius (typically 1)
     * @return int The effective PSF radius to use for rendering this point source
     */
    static int lookup_effective_radius(
        const std::vector<RadiusLUTEntry>& lut,
        float max_irradiance,
        int min_radius)
    {
        if (lut.empty()) {
            return min_radius;
        }

        std::size_t i = lut.size();
        while (i-- > 0) {
            if (max_irradiance >= lut[i].min_irradiance) {
                return std::max(lut[i].radius, min_radius);
            }
        }

        return min_radius;
    }

    /**
     * @brief Generic render item representing a point source (star or unresolved object).
     * 
     * Aggregates position, spectral irradiance, and computed effective PSF radius
     * for unified rendering of both stars and unresolved objects.
     * 
     * @tparam TSpectral Spectral type for the rendering pipeline
     */
    template <IsSpectral TSpectral>
    struct RenderItem {
        Vec3<float> point;
        TSpectral   irradiance;
        int         effective_radius;
    };

    /**
     * @brief Render unresolved point sources (stars and unresolved objects) into the frame buffer.
     * 
     * This method implements an optimized pipeline for rendering point sources that cannot be
     * resolved into visible geometry. It supports both delta-function (no PSF) and spatially-
     * distributed PSF rendering with adaptive radius culling for performance.
     * 
     * The rendering pipeline:
     * 1. Collects all stars and unresolved objects into a unified list
     * 2. Builds a radius LUT and assigns per-source effective PSF radii based on irradiance
     * 3. Projects sources to screen space and bins them into tiles
     * 4. Renders each tile in parallel into local buffers
     * 5. Combines tile buffers into the final frame buffer
     * 
     * Performance optimizations:
     * - Adaptive PSF radius: dim sources use smaller kernels
     * - Tiled rendering: parallel processing with minimal synchronization
     * - Depth occlusion testing: skip sources behind resolved geometry
     * 
     * @tparam TSpectral Spectral type for the rendering pipeline
     * @param scene_view The scene view containing stars and unresolved objects
     * @param frame_buffer The frame buffer to render into
     */
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
                              instance.unresolved_object->get_irradiance(scene_view.get_time()),
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
