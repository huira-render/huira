#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

#include "huira_impl/render/psf_lut.ipp"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void Renderer<TSpectral>::render(SceneView<TSpectral>& scene_view, FrameBuffer<TSpectral>& frame_buffer)
    {
        auto& camera = scene_view.camera_model_;
        const int fb_width = frame_buffer.width();
        const int fb_height = frame_buffer.height();
        if (camera->resolution().width != fb_width || camera->resolution().height != fb_height) {
            HUIRA_THROW_ERROR("Renderer::render - Frame buffer resolution does not match camera resolution.");
        }

        this->path_trace_(scene_view, frame_buffer);

        this->render_unresolved_(scene_view, frame_buffer);

        this->get_camera(scene_view)->readout(frame_buffer, scene_view.duration());
    }



    /**
     * @brief Path trace a scene view into a frame buffer.
     *
     * Traces camera rays through each pixel, evaluating direct lighting with
     * shadow rays and indirect illumination via recursive path tracing with
     * Russian roulette termination.
     *
     * The rendering is parallelized over tiles using TBB. Each tile accumulates
     * results from multiple samples per pixel (spp_) into the frame buffer.
     *
     * @tparam TSpectral Spectral type for the rendering pipeline
     * @param scene_view The scene view containing geometry, lights, and environment
     * @param frame_buffer The frame buffer to render into
     */
    template <IsSpectral TSpectral>
    void Renderer<TSpectral>::path_trace_(
        SceneView<TSpectral>& scene_view,
        FrameBuffer<TSpectral>& frame_buffer)
    {
        auto& camera = scene_view.camera_model_;
        const int fb_width = frame_buffer.width();
        const int fb_height = frame_buffer.height();
        const auto& lights = scene_view.lights_;
        const auto& background = scene_view.background_;

        frame_buffer.clear();

        // Tile-based parallel rendering:
        constexpr int TILE_SIZE = 16;
        int tiles_x = (fb_width + TILE_SIZE - 1) / TILE_SIZE;
        int tiles_y = (fb_height + TILE_SIZE - 1) / TILE_SIZE;
        int num_tiles = tiles_x * tiles_y;

        tbb::parallel_for(tbb::blocked_range<int>(0, num_tiles),
            [&](const tbb::blocked_range<int>& range) {
                for (int tile_idx = range.begin(); tile_idx < range.end(); ++tile_idx) {
                    int tile_y = tile_idx / tiles_x;
                    int tile_x = tile_idx % tiles_x;

                    int x0 = tile_x * TILE_SIZE;
                    int y0 = tile_y * TILE_SIZE;
                    int x1 = std::min(x0 + TILE_SIZE, fb_width);
                    int y1 = std::min(y0 + TILE_SIZE, fb_height);

                    // Per-tile RNG seeded from tile index for reproducibility:
                    RandomSampler<float> sampler(static_cast<unsigned int>(tile_idx));

                    for (int y = y0; y < y1; ++y) {
                        for (int x = x0; x < x1; ++x) {

                            TSpectral pixel_radiance{ 0 };
                            float closest_depth = std::numeric_limits<float>::infinity();
                            std::size_t mesh_id = std::numeric_limits<std::size_t>::max();
                            TSpectral albedo_total{ 0 };
                            Vec3<float> camera_normals{ 0 };

                            TSpectral mean{ 0 };
                            TSpectral M2{ 0 };   // sum of squared deviations
                            int samples_taken = 0;
                            float inv_samples = 0.0f;

                            for (int s = 0; s < spp_; ++s) {
                                // Jittered sub-pixel sample:
                                float sx = static_cast<float>(x) + sampler.get_1d();
                                float sy = static_cast<float>(y) + sampler.get_1d();

                                // Generate camera ray from pixel coordinates:
                                Ray<TSpectral> ray = camera->cast_ray(Pixel{ sx, sy }, sampler);

                                // Motion blur: randomize time sample per ray
                                float time = sampler.get_1d();  // [0, 1] maps to shutter interval

                                TSpectral throughput{ 1 };
                                TSpectral sample_radiance{ 0 };
                                for (int bounce = 0; bounce < max_bounces_; ++bounce) {
                                    HitRecord hit = scene_view.intersect(ray, time);

                                    if (!hit.hit()) {
                                        // Sample environment map using ray direction
                                        Vec3<float> d = glm::normalize(ray.direction());
                                        float u = 0.5f + std::atan2(d.z, d.x) * (0.5f * INV_PI<float>());
                                        float v = 0.5f - std::asin(std::clamp(d.y, -1.0f, 1.0f)) * INV_PI<float>();

                                        TSpectral env_radiance = background->sample_bilinear(u, v);
                                        sample_radiance += throughput * env_radiance;
                                        break;
                                    }

                                    if (s == 0) {
                                        mesh_id = hit.geom_id;
                                    }

                                    // Resolve full shading data:
                                    Interaction<TSpectral> isect = scene_view.resolve_hit(ray, hit);
                                    Vec3<float> new_origin = offset_intersection_(
                                        isect.position, isect.normal_g);
                                    isect.position = new_origin;

                                    // Look up mesh material:
                                    const auto& mapping = scene_view.instance_mappings_[hit.inst_id];
                                    const auto& batch = scene_view.geometry_[mapping.batch_index];
                                    const auto* material = batch.mesh->material();

                                    // Evaluate material textures:
                                    auto [params, shading_isect] = material->evaluate(isect);

                                    // Record primary ray info:
                                    if (bounce == 0) {
                                        closest_depth = std::min(closest_depth, hit.t);
                                        camera_normals += shading_isect.normal_s;
                                        albedo_total += params.albedo;
                                    }

                                    // Direct lighting (next event estimation)
                                    for (const auto& light_instance : lights) {
                                        auto sample = light_instance.light->sample_li(
                                            isect, light_instance.transform, this->sampler_);

                                        if (!sample) {
                                            continue;
                                        }
                                        const auto& ls = *sample;

                                        // Shadow test:
                                        float light_dist = glm::length(
                                            light_instance.transform.position - isect.position);
                                        Ray<TSpectral> shadow_ray(new_origin, ls.wi);

                                        if (scene_view.occluded(shadow_ray, light_dist, time)) {
                                            continue;
                                        }

                                        // Evaluate BSDF:
                                        TSpectral f = material->bsdf_eval(
                                            isect.wo, ls.wi, { params, shading_isect });
                                        float cos_theta = std::max(0.0f,
                                            glm::dot(shading_isect.normal_s, ls.wi));

                                        sample_radiance += throughput * ls.Li * f * cos_theta;
                                    }

                                    // Sample the BSDF:
                                    float u1 = sampler.get_1d();
                                    float u2 = sampler.get_1d();

                                    BSDFSample<TSpectral> bs = material->bsdf_sample(
                                        isect.wo, { params, shading_isect }, u1, u2);

                                    if (!bs.is_valid()) break;

                                    // value already contains f * |cos(theta_i)| / pdf
                                    throughput = throughput * bs.value;

                                    // Russian roulette (after a few bounces):
                                    if (bounce >= 3) {
                                        float p_continue = std::min(0.95f, throughput.max());
                                        if (sampler.get_1d() > p_continue) {
                                            break;
                                        }
                                        throughput = throughput / p_continue;
                                    }

                                    // Spawn next ray:
                                    ray = Ray<TSpectral>(new_origin, bs.wi);
                                }

                                if (std::isnan(sample_radiance[0])) {
                                    continue;
                                }

                                float max_val = sample_radiance.max();
                                if (max_val > clamp_threshold_) {
                                    sample_radiance *= (clamp_threshold_ / max_val);
                                }

                                

                                // Welford's online mean/variance update:
                                samples_taken++;
                                if (dynamic_sampling_) {
                                    TSpectral delta = sample_radiance - mean;
                                    inv_samples = (1.0f / static_cast<float>(samples_taken));
                                    mean += delta * inv_samples;
                                    TSpectral delta2 = sample_radiance - mean;
                                    M2 += delta * delta2;
                                }

                                pixel_radiance += sample_radiance;

                                // Early exit check (only after min_spp samples):
                                if (dynamic_sampling_) {
                                    if (s >= min_spp_ - 1) {
                                        TSpectral variance = M2 * inv_samples;
                                        // Normalize variance relative to mean luminance to avoid
                                        // over-sampling dark regions:
                                        float rel_variance = variance.max() / (mean.max() + 1e-4f);
                                        if (rel_variance < variance_threshold_) {
                                            break;
                                        }
                                    }
                                }
                            }
                            float inv_spp = 1.0f / static_cast<float>(samples_taken);

                            // Average over samples and write to frame buffer:
                            TSpectral avg_radiance = pixel_radiance * inv_spp;
                            Vec3<float> avg_camera_normals = glm::normalize(camera_normals * inv_spp);

                            if (frame_buffer.has_depth()) {
                                if (closest_depth < std::numeric_limits<float>::infinity()) {
                                    frame_buffer.depth()(x, y) = closest_depth;
                                }
                            }

                            if (frame_buffer.has_albedo()) {
                                frame_buffer.albedo()(x, y) = albedo_total * inv_spp;
                            }

                            if (frame_buffer.has_mesh_ids()) {
                                frame_buffer.mesh_ids()(x, y) = mesh_id;
                            }

                            if (frame_buffer.has_camera_normals()) {
                                frame_buffer.camera_normals()(x, y) = avg_camera_normals;
                            }

                            if (frame_buffer.has_world_normals()) {
                                frame_buffer.world_normals()(x, y) = scene_view.camera_to_world_[0].apply_to_direction(avg_camera_normals);
                            }

                            if (frame_buffer.has_received_power()) {
                                frame_buffer.received_power()(x, y) = camera->pixel_radiance_to_power(x, y) * avg_radiance;
                            }
                        }
                    }
                }
            });
    }



    template <IsSpectral TSpectral>
    struct RenderItem {
        RenderItem(TrajectoryArc set_arc, std::vector<TSpectral> set_irradiance, int set_effective_radius)
            : arc(std::move(set_arc)), irradiance(std::move(set_irradiance)), effective_radius(set_effective_radius) {}

        TrajectoryArc arc;
        std::vector<TSpectral> irradiance;
        int effective_radius;

        TSpectral interpolate_irradiances(float t) const
        {
            if (irradiance.size() == 1) {
                return irradiance[0];
            }

            float scaled = t * static_cast<float>(irradiance.size() - 1);
            std::size_t lo = static_cast<std::size_t>(std::floor(scaled));
            lo = std::min(lo, irradiance.size() - 2);
            float frac = scaled - static_cast<float>(lo);

            return irradiance[lo] + frac * (irradiance[lo + 1] - irradiance[lo]);
        }

        float max_irradiance() const
        {
            float max_irr = 0.f;
            for (const TSpectral& irr : irradiance) {
                max_irr = std::max(max_irr, irr.max());
            }
            return max_irr;
        }
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
        bool has_power = frame_buffer.has_received_power();
        if (!has_power) {
            return;
        }

        auto& camera = scene_view.camera_model_;
        const int fb_width = frame_buffer.width();
        const int fb_height = frame_buffer.height();
        
        const int full_radius = camera->has_psf() ? camera->get_psf_radius() : 0;
        
        // Collect all unresolved points (stars + UnresolvedObjects) in a single list for processing:
        std::vector<RenderItem<TSpectral>> items;
        items.reserve(scene_view.stars_.size() + scene_view.unresolved_objects_.size());

        const auto& times = scene_view.temporal_samples_;
        
        for (const auto& star : scene_view.stars_) {
            std::vector<Vec3<float>> directions(star.size());
            std::vector<TSpectral> irradiances(star.size());
            for (std::size_t i = 0; i < star.size(); ++i) {
                directions[i] = star[i].get_direction();
                irradiances[i] = star[i].get_irradiance();
            }
            TrajectoryArc arc(directions);
            items.push_back({ arc, irradiances, full_radius });
        }
        for (const auto& instance : scene_view.unresolved_objects_) {
            std::vector<Vec3<float>> directions(instance.transforms.size());
            std::vector<TSpectral> irradiances(instance.transforms.size());
            for (std::size_t i = 0; i < instance.transforms.size(); ++i) {
                directions[i] = glm::normalize(instance.transforms[i].position);
                irradiances[i] = instance.unresolved_object->get_irradiance(times[i]);
            }
            TrajectoryArc arc(directions);
            items.push_back({ arc, irradiances, full_radius});
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
                    lookup_effective_radius(radius_lut, item.max_irradiance(),
                        config.min_radius);
            }
        }
        
        // Create the screens-pace tiles for parallel rendering:
        constexpr int TILE_SIZE = 64;
        
        int tiles_x = (fb_width + TILE_SIZE - 1) / TILE_SIZE;
        int tiles_y = (fb_height + TILE_SIZE - 1) / TILE_SIZE;
        int num_tiles = tiles_x * tiles_y;
        
        float res_x = static_cast<float>(camera->resolution().x);
        float res_y = static_cast<float>(camera->resolution().y);

        // Process arcs into tiles:
        struct ProjectedItem {
            std::size_t item_idx;
            Pixel       projected;
            float       weight;
            TSpectral   irradiance;
            Vec3<float> direction;
        };


        std::vector<std::vector<ProjectedItem>> tile_bins(
            static_cast<std::size_t>(num_tiles));

        float max_pixel_step = 0.75f; // TODO Make this configurable
        for (std::size_t i = 0; i < items.size(); ++i) {
            const auto& item = items[i];
            const auto& arc = item.arc;

            // Clip arc to frustum:
            auto visible_intervals = camera->view_frustum().clip_arc(arc);
            if (visible_intervals.empty()) continue;

            // For each visible interval, adaptively sample in pixel space:
            for (const auto& [t_start, t_end] : visible_intervals) {

                // Start with the original sample parameter values that fall within
                // this visible interval. For N input samples, these are at
                // t = 0, 1/(N-1), 2/(N-1), ..., 1
                std::vector<float> params;
                params.push_back(t_start);
                std::size_t N = arc.sample_count();
                for (std::size_t k = 0; k < N; ++k) {
                    float t_k = (N == 1) ? 0.0f : static_cast<float>(k) / static_cast<float>(N - 1);
                    if (t_k > t_start && t_k < t_end) {
                        params.push_back(t_k);
                    }
                }
                params.push_back(t_end);

                // Project initial points to pixel space:
                std::vector<Pixel> pixels(params.size());
                for (std::size_t k = 0; k < params.size(); ++k) {
                    Vec3<float> dir = arc.evaluate(params[k]);
                    pixels[k] = camera->project_point(dir);
                }

                // Adaptive subdivision: bisect intervals where pixel distance > threshold
                // Process from back to front so insertions don't invalidate indices
                constexpr int MAX_SUBDIVISIONS = 12;  // safety limit
                for (int pass = 0; pass < MAX_SUBDIVISIONS; ++pass) {
                    bool subdivided = false;
                    for (std::size_t k = params.size() - 1; k > 0; --k) {
                        float dx = pixels[k].x - pixels[k - 1].x;
                        float dy = pixels[k].y - pixels[k - 1].y;
                        float dist = std::sqrt(dx * dx + dy * dy);

                        if (dist > max_pixel_step) {
                            float t_mid = (params[k - 1] + params[k]) / 2.0f;
                            Vec3<float> dir_mid = arc.evaluate(t_mid);
                            Pixel p_mid = camera->project_point(dir_mid);

                            params.insert(params.begin() + static_cast<decltype(params)::difference_type>(k), t_mid);
                            pixels.insert(pixels.begin() + static_cast<decltype(params)::difference_type>(k), p_mid);
                            subdivided = true;
                        }
                    }
                    if (!subdivided) break;
                }

                // Compute weights (proportional to parameter interval around each sample):
                // Each sample represents the midpoint of its surrounding interval.
                std::size_t num_samples = params.size();
                for (std::size_t k = 0; k < num_samples; ++k) {
                    float dt;
                    if (num_samples == 1) {
                        dt = t_end - t_start;
                    }
                    else if (k == 0) {
                        dt = (params[1] - params[0]) / 2.0f;
                    }
                    else if (k == num_samples - 1) {
                        dt = (params[k] - params[k - 1]) / 2.0f;
                    }
                    else {
                        dt = (params[k + 1] - params[k - 1]) / 2.0f;
                    }
                    float weight = dt / (t_end - t_start);  // normalize so weights sum to ~1

                    // Interpolate irradiance at this parameter value:
                    TSpectral irrad = item.interpolate_irradiances(params[k]);
                    Vec3<float> dir = arc.evaluate(params[k]);
                    const Pixel& p = pixels[k];
                    if (p.x < 0.f || p.x > res_x || p.y < 0.f || p.y > res_y) continue;

                    int tx = std::clamp(static_cast<int>(p.x) / TILE_SIZE, 0, tiles_x - 1);
                    int ty = std::clamp(static_cast<int>(p.y) / TILE_SIZE, 0, tiles_y - 1);

                    tile_bins[static_cast<std::size_t>(ty * tiles_x + tx)].push_back({
                        i, p, weight, irrad, dir
                        });
                }
            }
        }

        
        // Render tiles in parallel:
        auto& power_buffer = frame_buffer.received_power();
        const auto& depth_buffer = frame_buffer.depth();
        bool has_depth = frame_buffer.has_depth();
        
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
                            camera->get_projected_aperture_area(proj.direction);
                        TSpectral power = proj.weight * proj.irradiance * projected_area;
        
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
