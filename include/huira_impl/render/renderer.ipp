#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

/// DEBUGGING
// #include "tbb/global_control.h"
// static tbb::global_control
// debug_single_thread_control(tbb::global_control::max_allowed_parallelism, 1);

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/render/shading_utils.hpp"
#include "huira/sampling/cone_sampling.hpp"
#include "huira/volumes/medium.hpp"
#include "huira/volumes/medium_stack.hpp"
#include "huira_impl/render/psf_lut.ipp"

namespace huira {

template <IsSpectral TSpectral>
void Renderer<TSpectral>::render(SceneView<TSpectral>& scene_view,
                                 FrameBuffer<TSpectral>& frame_buffer)
{
    auto& camera = scene_view.camera_model_;
    const int fb_width = frame_buffer.width();
    const int fb_height = frame_buffer.height();
    if (camera->resolution().width != fb_width || camera->resolution().height != fb_height) {
        HUIRA_THROW_ERROR(
            "Renderer::render - Frame buffer resolution does not match camera resolution.");
    }

    frame_buffer.clear();

    Image<TSpectral> ray_traced_power = this->path_trace_(scene_view, frame_buffer);

    Image<TSpectral> star_wing_splat(0, 0, TSpectral{0});
    Image<TSpectral> star_power =
        this->render_unresolved_(scene_view, frame_buffer, star_wing_splat);

    // Apply the scattered-light wings to unresolved sources.
    if (star_wing_splat.size() > 0) {
        this->convolve_cached_(
            star_wing_splat, camera->get_psf_wings_kernel(), *camera, wings_convolver_);
        const float f_s = camera->scatter_fraction_;
        const float core_weight = 1.f - f_s;
        for (std::size_t i = 0; i < star_power.size(); ++i) {
            star_power[i] = star_power[i] * core_weight + star_wing_splat[i] * f_s;
        }
    }

    if (frame_buffer.has_received_power()) {
        frame_buffer.received_power() = ray_traced_power + star_power;
    }

    // Unresolved sources reach the sensor without an intervening bounce, so they are
    // direct illumination and belong in that component too.
    if (frame_buffer.has_received_direct_power() && star_power.size() > 0) {
        Image<TSpectral>& direct = frame_buffer.received_direct_power();
        for (std::size_t i = 0; i < direct.size(); ++i) {
            direct[i] += star_power[i];
        }
    }

    // Apply Veiling Glare
    if (camera->veiling_glare_enabled_) {
        const float unveiled = 1.f - camera->veiling_alpha_;

        // Redistributing each component by its own mean is linear, so the
        // decomposition survives: D' + I' = (D + I) * unveiled + alpha * mean(D + I).
        auto apply_veiling = [&](Image<TSpectral>& image) {
            if (image.size() == 0) {
                return;
            }
            TSpectral total_power{0.f};
            for (std::size_t i = 0; i < image.size(); ++i) {
                total_power += image[i];
            }
            TSpectral veiling_bias =
                camera->veiling_alpha_ * total_power / static_cast<float>(image.size());

            for (std::size_t i = 0; i < image.size(); ++i) {
                image[i] = (image[i] * unveiled) + veiling_bias;
            }
        };

        if (frame_buffer.has_received_power()) {
            apply_veiling(frame_buffer.received_power());
        }
        if (frame_buffer.has_received_direct_power()) {
            apply_veiling(frame_buffer.received_direct_power());
        }
        if (frame_buffer.has_received_indirect_power()) {
            apply_veiling(frame_buffer.received_indirect_power());
        }
    }

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
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 * @param scene_view The scene view containing geometry, lights, and environment
 * @param frame_buffer The frame buffer to render into
 */
template <IsSpectral TSpectral>
Image<TSpectral> Renderer<TSpectral>::path_trace_(SceneView<TSpectral>& scene_view,
                                                  FrameBuffer<TSpectral>& frame_buffer)
{
    auto start_clock = std::chrono::high_resolution_clock::now();
    auto& camera = scene_view.camera_model_;
    const int fb_width = frame_buffer.width();
    const int fb_height = frame_buffer.height();
    const auto& lights = scene_view.lights_;
    const auto& background = scene_view.background_;

    Image<TSpectral> received_power(0, 0, TSpectral{0});
    if (frame_buffer.has_received_power()) {
        received_power = Image<TSpectral>(fb_width, fb_height, TSpectral{0});
    }

    // Conservative occluder record consumed by render_unresolved_().
    occluder_mask_valid_ = false;
    occluder_mask_ = Image<uint8_t>(fb_width, fb_height, uint8_t{0});
    std::atomic<bool> any_occluder{false};

    // Tile-based parallel rendering:
    constexpr int TILE_SIZE = 16;
    int tiles_x = (fb_width + TILE_SIZE - 1) / TILE_SIZE;
    int tiles_y = (fb_height + TILE_SIZE - 1) / TILE_SIZE;
    int num_tiles = tiles_x * tiles_y;
    float time = 0.f;
    const bool has_motion_blur = scene_view.temporal_samples_.size() > 1;

    tbb::parallel_for(
        tbb::blocked_range<int>(0, num_tiles), [&](const tbb::blocked_range<int>& range) {
            for (int tile_idx = range.begin(); tile_idx < range.end(); ++tile_idx) {
                int tile_y = tile_idx / tiles_x;
                int tile_x = tile_idx % tiles_x;

                int x0 = tile_x * TILE_SIZE;
                int y0 = tile_y * TILE_SIZE;
                int x1 = std::min(x0 + TILE_SIZE, fb_width);
                int y1 = std::min(y0 + TILE_SIZE, fb_height);

                // Per-tile RNG seeded from tile index for reproducibility:
                RandomSampler<float> sampler(static_cast<unsigned int>(tile_idx));

                // BSDF-side MIS counterweight: for each designated indirect source,
                // the reflector-NEE PDF of the most recent BSDF-sampled direction.
                std::vector<float> prev_reflector_pdf(scene_view.indirect_sources().size(), 0.0f);

                for (int y = y0; y < y1; ++y) {
                    for (int x = x0; x < x1; ++x) {

                        TSpectral pixel_direct_radiance{0};
                        TSpectral pixel_indirect_radiance{0};
                        TSpectral pixel_radiance{0};

                        float closest_depth = std::numeric_limits<float>::infinity();
                        bool primary_occluder = false;
                        std::size_t geometry_id = std::numeric_limits<std::size_t>::max();
                        TSpectral albedo_total{0};
                        Vec3<float> camera_normals{0};

                        TSpectral mean{0};
                        TSpectral M2{0}; // sum of squared deviations
                        int samples_taken = 0;
                        float inv_samples = 0.0f;

                        for (int s = 0; s < spp_; ++s) {
                            // Jittered sub-pixel sample:
                            float sx = static_cast<float>(x) + sampler.get_1d();
                            float sy = static_cast<float>(y) + sampler.get_1d();

                            // Generate camera ray from pixel coordinates:
                            Ray<TSpectral> ray = camera->cast_ray(Pixel{sx, sy}, sampler);

                            // Motion blur: randomize time sample per ray
                            if (has_motion_blur) {
                                time = sampler.get_1d(); // [0, 1] maps to shutter interval
                            }

                            TSpectral throughput{1};
                            TSpectral direct_radiance{0};
                            TSpectral indirect_radiance{0};

                            float prev_roughness = 0.0f;

                            float prev_bsdf_pdf = 1.0f;
                            Interaction<TSpectral> prev_isect;

                            MediumStack<TSpectral> medium_stack;

                            for (int bounce = 0; bounce < max_bounces_; ++bounce) {
                                HitRecord hit = scene_view.intersect(ray, time);

                                if (!medium_stack.is_empty()) {
                                    const Medium<TSpectral>* current_medium = medium_stack.top();

                                    const float t_seg_start = ray.tnear();
                                    const Ray<TSpectral> march_ray(ray.at(t_seg_start),
                                                                   ray.direction());
                                    const float t_seg = hit.t - t_seg_start;

                                    auto opt_mi =
                                        current_medium->sample_free_path(march_ray, sampler);
                                    auto props = current_medium->get_properties(march_ray.origin());
                                    TSpectral ext = props.extinction();

                                    float avg_ext = 0.0f;
                                    for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                                        avg_ext += ext[c];
                                    }
                                    avg_ext /= static_cast<float>(TSpectral::size());

                                    if (opt_mi && opt_mi->t < t_seg) {
                                        float t = opt_mi->t;
                                        TSpectral Tr{0.f};
                                        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                                            Tr[c] = std::exp(-ext[c] * t);
                                        }
                                        float pdf = avg_ext * std::exp(-avg_ext * t);
                                        throughput =
                                            throughput * props.scattering * Tr * (1.0f / pdf);

                                        Interaction<TSpectral> vol_isect;
                                        vol_isect.position = opt_mi->p;
                                        vol_isect.wo = opt_mi->wo;
                                        vol_isect.normal_g = Vec3<float>{0.f};
                                        vol_isect.normal_s = Vec3<float>{0.f};

                                        for (const auto& light_instance : lights) {
                                            Transform<float> current_transform =
                                                interpolate_transform(light_instance.transforms,
                                                                      time);

                                            auto sample = light_instance.light->sample_li(
                                                vol_isect, current_transform, sampler);

                                            if (!sample) {
                                                continue;
                                            }

                                            const auto& ls = *sample;

                                            Ray<TSpectral> shadow_ray(opt_mi->p, ls.wi);
                                            TSpectral shadow_transmittance =
                                                scene_view.evaluate_transmittance(shadow_ray,
                                                                                  ls.distance,
                                                                                  medium_stack,
                                                                                  sampler,
                                                                                  time);

                                            if (shadow_transmittance.max() <= 0.0f) {
                                                continue;
                                            }
                                            float phase_val =
                                                opt_mi->phase_function->evaluate(opt_mi->wo, ls.wi);

                                            TSpectral Ld = throughput * (ls.Li / ls.pdf) *
                                                           phase_val * shadow_transmittance;

                                            if (bounce == 0) {
                                                direct_radiance += Ld;
                                            } else {
                                                indirect_radiance += Ld;
                                            }
                                        }

                                        PhaseSample ps =
                                            opt_mi->phase_function->sample(opt_mi->wo, sampler);

                                        float phase_eval =
                                            opt_mi->phase_function->evaluate(opt_mi->wo, ps.wi);
                                        throughput = throughput * (phase_eval / ps.p);

                                        ray = Ray<TSpectral>(opt_mi->p, ps.wi);
                                        prev_bsdf_pdf = ps.p;
                                        continue;

                                    } else {
                                        TSpectral Tr{0.f};
                                        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                                            Tr[c] = std::exp(-ext[c] * t_seg);
                                        }
                                        float pdf = std::exp(-avg_ext * t_seg);
                                        throughput = throughput * Tr * (1.0f / pdf);
                                    }
                                }

                                if (!hit.hit()) {
                                    // Sample environment map using ray direction
                                    Vec3<float> d = glm::normalize(ray.direction());
                                    float u =
                                        0.5f + std::atan2(d.z, d.x) * (0.5f * INV_PI<float>());
                                    float v = 0.5f - std::asin(std::clamp(d.y, -1.0f, 1.0f)) *
                                                         INV_PI<float>();

                                    TSpectral env_radiance = background->sample_bilinear(u, v);

                                    if (bounce == 0) {
                                        direct_radiance += throughput * env_radiance;
                                    } else {
                                        indirect_radiance += throughput * env_radiance;
                                    }
                                    break;
                                }

                                const auto& mapping = scene_view.instance_mappings_[hit.inst_id];

                                if (bounce == 0 && mapping.type == GeometryType::Primitive) {
                                    primary_occluder = true;
                                }

                                if (mapping.type == GeometryType::Light) {
                                    const auto& light_instance = lights[mapping.light_index];

                                    Vec3<float> hit_p = ray.origin() + ray.direction() * hit.t;
                                    Vec3<float> emission_dir = -ray.direction();
                                    TSpectral Le =
                                        light_instance.light->radiance(hit_p, emission_dir);

                                    float mis_weight = 1.0f;

                                    // Only apply MIS weighting if this wasn't the first camera ray,
                                    // and if it wasn't a perfect mirror reflection (delta BSDF).
                                    if (bounce > 0) {
                                        Transform<float> current_transform =
                                            interpolate_transform(light_instance.transforms, time);

                                        float light_pdf = light_instance.light->pdf_li(
                                            prev_isect, current_transform, ray.direction());
                                        mis_weight = power_heuristic(prev_bsdf_pdf, light_pdf);
                                    }

                                    TSpectral final_radiance = throughput * Le * mis_weight;

                                    if (bounce == 0) {
                                        direct_radiance += final_radiance;
                                    } else {
                                        indirect_radiance += final_radiance;
                                    }

                                    // Terminate for hitting light:
                                    break;
                                } else {

                                    if (s == 0) {
                                        geometry_id = hit.geom_id;
                                    }

                                    // Resolve full shading data:
                                    Interaction<TSpectral> isect = scene_view.resolve_hit(ray, hit);

                                    // Look up mesh material:
                                    const auto& batch = scene_view.primitives_[mapping.batch_index];
                                    const auto* material = batch.primitive->material.get();

                                    // Evaluate material textures to get the opacity parameter:
                                    auto [params, shading_isect] = material->evaluate(isect);

                                    if (params.opacity < 1.0f) {
                                        if (sampler.get_1d() > params.opacity) {
                                            ray = Ray<TSpectral>(ray.origin(),
                                                                 ray.direction(),
                                                                 advance_ray_t(hit.t));

                                            medium_stack.toggle(batch.primitive.get());

                                            bounce--; // Don't count this towards bounce counts
                                            continue;
                                        }
                                    }

                                    // Path regulatization
                                    if (bounce > 0) {
                                        params.roughness =
                                            std::max(params.roughness, prev_roughness);
                                    }
                                    prev_roughness = params.roughness;

                                    // Record primary ray info:
                                    if (bounce == 0) {
                                        closest_depth = std::min(closest_depth, hit.t);
                                        camera_normals += shading_isect.normal_s;
                                        albedo_total += params.albedo;
                                    }

                                    float reflector_nee_weight = 1.0f;
                                    const std::size_t hit_source =
                                        scene_view.indirect_source_index(hit);
                                    if (bounce > 0 &&
                                        hit_source != SceneView<TSpectral>::NO_INDIRECT_SOURCE) {
                                        reflector_nee_weight = power_heuristic(
                                            prev_bsdf_pdf, prev_reflector_pdf[hit_source]);
                                    }

                                    // Direct lighting (next event estimation):
                                    for (const auto& light_instance : lights) {
                                        TSpectral Ld =
                                            throughput * reflector_nee_weight *
                                            scene_view.sample_light_contribution_(light_instance,
                                                                                  isect,
                                                                                  material,
                                                                                  params,
                                                                                  shading_isect,
                                                                                  medium_stack,
                                                                                  sampler,
                                                                                  time);
                                        if (bounce == 0) {
                                            direct_radiance += Ld;
                                        } else {
                                            indirect_radiance += Ld;
                                        }
                                    }

                                    // Reflector next event estimation:
                                    const auto& sources = scene_view.indirect_sources();
                                    for (std::size_t k = 0; k < sources.size(); ++k) {
                                        const auto& source = sources[k];

                                        SphereConeSample cs =
                                            source.sample_toward(isect.position, time, sampler);
                                        if (cs.pdf <= 0.0f) {
                                            continue;
                                        }

                                        // Evaluate the surface response first
                                        TSpectral f = material->bsdf_eval(
                                            isect.wo, cs.wi, {params, shading_isect});
                                        float cos_theta =
                                            std::max(0.0f, glm::dot(shading_isect.normal_s, cs.wi));
                                        if (cos_theta <= 0.0f || f.max() <= 0.0f) {
                                            continue;
                                        }

                                        // Trace toward the source's bounding proxy:
                                        Vec3<float> probe_normal =
                                            (glm::dot(cs.wi, isect.normal_g) < 0.0f)
                                                ? -isect.normal_g
                                                : isect.normal_g;
                                        Vec3<float> probe_origin = offset_spawn_point(
                                            isect.position, probe_normal, isect.p_err);
                                        Ray<TSpectral> probe_ray(probe_origin, cs.wi);
                                        HitRecord probe_hit = scene_view.intersect(probe_ray, time);

                                        if (scene_view.indirect_source_index(probe_hit) != k) {
                                            continue;
                                        }

                                        // Radiance leaving the reflector toward this vertex:
                                        TSpectral Lr = scene_view.direct_lit_radiance(
                                            probe_ray, probe_hit, sampler, time, medium_stack);
                                        if (Lr.max() <= 0.0f) {
                                            continue;
                                        }

                                        // MIS against BSDF sampling
                                        float bsdf_pdf = material->bsdf_pdf(
                                            isect.wo, cs.wi, {params, shading_isect});
                                        const bool partner_truncated = (bounce + 1 >= max_bounces_);
                                        float w = partner_truncated
                                                      ? 1.0f
                                                      : power_heuristic(cs.pdf, bsdf_pdf);

                                        TSpectral Lr_contrib =
                                            throughput * f * cos_theta * (Lr / cs.pdf) * w;
                                        if (bounce == 0) {
                                            direct_radiance += Lr_contrib;
                                        } else {
                                            indirect_radiance += Lr_contrib;
                                        }
                                    }

                                    // Sample the BSDF:
                                    float u1 = sampler.get_1d();
                                    float u2 = sampler.get_1d();

                                    BSDFSample<TSpectral> bs = material->bsdf_sample(
                                        isect.wo, {params, shading_isect}, u1, u2);

                                    if (!bs.is_valid()) {
                                        break;
                                    }

                                    // Direction changes: offset along the geometric normal by
                                    // the propagated position-error bound (see interaction.hpp).
                                    Vec3<float> bounce_normal =
                                        (glm::dot(bs.wi, isect.normal_g) < 0.0f) ? -isect.normal_g
                                                                                 : isect.normal_g;
                                    Vec3<float> bounce_origin = offset_spawn_point(
                                        isect.position, bounce_normal, isect.p_err);

                                    prev_bsdf_pdf = bs.is_delta ? 0.0f : bs.pdf;
                                    prev_isect = shading_isect;

                                    for (std::size_t k = 0; k < prev_reflector_pdf.size(); ++k) {
                                        prev_reflector_pdf[k] =
                                            sources[k].pdf_toward(isect.position, time, bs.wi);
                                    }

                                    throughput = throughput * bs.value;

                                    // Russian roulette (after a few bounces):
                                    if (bounce >= 3) {
                                        float p_continue =
                                            std::clamp(throughput.max(), 0.05f, 0.95f);
                                        if (sampler.get_1d() > p_continue) {
                                            break;
                                        }
                                        throughput = throughput / p_continue;
                                    }

                                    // Spawn next ray:
                                    ray = Ray<TSpectral>(bounce_origin, bs.wi);

                                    const float wo_side = glm::dot(isect.wo, isect.normal_g);
                                    const float wi_side = glm::dot(bs.wi, isect.normal_g);
                                    const bool is_transmission = (wo_side * wi_side) < 0.0f;
                                    if (is_transmission) {
                                        medium_stack.toggle(batch.primitive.get());
                                    }
                                }
                            }

                            // Indirect radiance clamping:
                            float current_indirect_max = indirect_radiance.max();
                            if (current_indirect_max > indirect_clamp_threshold_) {
                                indirect_radiance *=
                                    (indirect_clamp_threshold_ / current_indirect_max);
                            }

                            TSpectral sample_radiance = direct_radiance + indirect_radiance;

                            if (std::isnan(sample_radiance[0])) {
                                continue;
                            }

                            samples_taken++;

                            // Welford's online mean/variance update:
                            TSpectral delta = sample_radiance - mean;
                            inv_samples = (1.0f / static_cast<float>(samples_taken));
                            mean += delta * inv_samples;
                            TSpectral delta2 = sample_radiance - mean;
                            M2 += delta * delta2;

                            pixel_direct_radiance += direct_radiance;
                            pixel_indirect_radiance += indirect_radiance;
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
                        TSpectral direct_radiance = pixel_direct_radiance * inv_spp;
                        TSpectral indirect_radiance = pixel_indirect_radiance * inv_spp;
                        Vec3<float> avg_camera_normals = glm::normalize(camera_normals * inv_spp);

                        if (primary_occluder) {
                            occluder_mask_(x, y) = uint8_t{1};
                            any_occluder.store(true, std::memory_order_relaxed);
                        }

                        if (frame_buffer.has_depth()) {
                            if (closest_depth < std::numeric_limits<float>::infinity()) {
                                frame_buffer.depth()(x, y) = closest_depth;
                            }
                        }

                        if (frame_buffer.has_albedo()) {
                            frame_buffer.albedo()(x, y) = albedo_total * inv_spp;
                        }

                        if (frame_buffer.has_geometry_ids()) {
                            frame_buffer.geometry_ids()(x, y) = geometry_id;
                        }

                        if (frame_buffer.has_camera_normals()) {
                            frame_buffer.camera_normals()(x, y) = avg_camera_normals;
                        }

                        if (frame_buffer.has_world_normals()) {
                            frame_buffer.world_normals()(x, y) =
                                scene_view.camera_to_world_[0].apply_to_direction(
                                    avg_camera_normals);
                        }

                        if (frame_buffer.has_received_direct_power()) {
                            frame_buffer.received_direct_power()(x, y) =
                                camera->pixel_radiance_to_power(x, y) * direct_radiance;
                        }

                        if (frame_buffer.has_received_indirect_power()) {
                            frame_buffer.received_indirect_power()(x, y) =
                                camera->pixel_radiance_to_power(x, y) * indirect_radiance;
                        }

                        if (frame_buffer.has_received_power()) {
                            received_power(x, y) =
                                camera->pixel_radiance_to_power(x, y) * avg_radiance;
                        }
                    }
                }
            }
        });

    // Dilate the occluder mask by one pixel (for safety margin)
    if (any_occluder.load(std::memory_order_relaxed)) {
        Image<uint8_t> dilated(fb_width, fb_height, uint8_t{0});
        tbb::parallel_for(tbb::blocked_range<int>(0, fb_height),
                          [&](const tbb::blocked_range<int>& range) {
                              for (int y = range.begin(); y < range.end(); ++y) {
                                  const int ny0 = std::max(0, y - 1);
                                  const int ny1 = std::min(fb_height - 1, y + 1);
                                  for (int x = 0; x < fb_width; ++x) {
                                      const int nx0 = std::max(0, x - 1);
                                      const int nx1 = std::min(fb_width - 1, x + 1);
                                      uint8_t v = 0;
                                      for (int ny = ny0; ny <= ny1 && v == 0; ++ny) {
                                          for (int nx = nx0; nx <= nx1; ++nx) {
                                              if (occluder_mask_(nx, ny) != 0) {
                                                  v = 1;
                                                  break;
                                              }
                                          }
                                      }
                                      dilated(x, y) = v;
                                  }
                              }
                          });
        occluder_mask_ = std::move(dilated);
    }
    occluder_mask_valid_ = true;

    if (camera->convolve_psf_) {
        // Convolution is linear, so convolving the components independently keeps
        // total == direct + indirect exactly.
        const Image<TSpectral>& psf = camera->get_psf_convolution_kernel();
        if (frame_buffer.has_received_power()) {
            received_power.convolve(psf);
        }
        if (frame_buffer.has_received_direct_power()) {
            frame_buffer.received_direct_power().convolve(psf);
        }
        if (frame_buffer.has_received_indirect_power()) {
            frame_buffer.received_indirect_power().convolve(psf);
        }
    }

    auto end_clock = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_clock - start_clock;
    HUIRA_LOG_INFO("Path tracing completed in " + std::to_string(elapsed.count()) + " seconds");

    return received_power;
}

template <IsSpectral TSpectral>
struct RenderItem {
    RenderItem(TrajectoryArc set_arc,
               std::vector<TSpectral> set_irradiance,
               std::vector<float> set_range,
               int set_effective_radius)
        : arc(std::move(set_arc)), irradiance(std::move(set_irradiance)),
          range(std::move(set_range)), effective_radius(set_effective_radius)
    {
    }

    TrajectoryArc arc;
    std::vector<TSpectral> irradiance;

    /// Distance to the source at each temporal sample
    std::vector<float> range;

    int effective_radius;
    static constexpr float RANGE_AT_INFINITY = std::numeric_limits<float>::max();

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

    float interpolate_ranges(float t) const
    {
        if (range.size() == 1) {
            return range[0];
        }

        float scaled = t * static_cast<float>(range.size() - 1);
        std::size_t lo = static_cast<std::size_t>(std::floor(scaled));
        lo = std::min(lo, range.size() - 2);
        float frac = scaled - static_cast<float>(lo);

        // Guard the infinite case: interpolating between two RANGE_AT_INFINITY
        // endpoints must stay at infinity rather than drifting.
        if (range[lo] == RANGE_AT_INFINITY || range[lo + 1] == RANGE_AT_INFINITY) {
            return RANGE_AT_INFINITY;
        }

        return range[lo] + frac * (range[lo + 1] - range[lo]);
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
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 * @param scene_view The scene view containing stars and unresolved objects
 * @param frame_buffer The frame buffer to render into
 */
template <IsSpectral TSpectral>
Image<TSpectral> Renderer<TSpectral>::render_unresolved_(SceneView<TSpectral>& scene_view,
                                                         FrameBuffer<TSpectral>& frame_buffer,
                                                         Image<TSpectral>& wing_splat)
{
    auto start_clock = std::chrono::high_resolution_clock::now();
    auto& camera = scene_view.camera_model_;
    const int fb_width = frame_buffer.width();
    const int fb_height = frame_buffer.height();

    Image<TSpectral> received_power(0, 0, TSpectral{0});
    if (frame_buffer.has_received_power()) {
        received_power = Image<TSpectral>(fb_width, fb_height, TSpectral{0});
    } else {
        return received_power;
    }

    // Determine the stamp radius based on camera settings:
    bool use_defocus = camera->aperture_->has_defocus();

    // Scattered-light wings for unresolved sources:
    const bool splat_wings = camera->convolve_psf_ && camera->scatter_enabled_ && !use_defocus;
    if (splat_wings) {
        wing_splat = Image<TSpectral>(fb_width, fb_height, TSpectral{0});
    }
    bool use_psf_direct = camera->has_psf() && !use_defocus;
    int stamp_radius = 0;
    if (use_defocus) {
        stamp_radius = camera->aperture_->get_defocus_half_extent();
    } else if (use_psf_direct) {
        stamp_radius = camera->get_psf_radius();
    }

    const auto& times = scene_view.temporal_samples_;
    const auto& star_field = scene_view.stars_;

    if (star_field.empty() && scene_view.unresolved_objects_.empty()) {
        wing_splat = Image<TSpectral>(0, 0, TSpectral{0});
        return received_power;
    }

    // Build the radius LUT up front so that per-source radii can be assigned as each
    // source is created, rather than in a second pass over the whole catalogue.
    bool use_radius_lut = false;
    RadiusLUTConfig radius_config;
    std::vector<RadiusLUTEntry> radius_lut;
    if (use_psf_direct && stamp_radius > 1) {
        const Image<TSpectral>& center_kernel = camera->get_psf_kernel(0.0f, 0.0f);

        // On-axis area is conservative:
        float representative_area =
            camera->get_projected_aperture_area(Vec3<float>{0.f, 0.f, -1.f});

        // Per-channel photon energies:
        TSpectral photon_energies = TSpectral::photon_energies();

        radius_lut = build_radius_lut(
            center_kernel, stamp_radius, representative_area, photon_energies, radius_config);
        use_radius_lut = true;
    }

    // Per-source lookup - just a scalar comparison, no kernel traversal.
    auto assign_radius = [&](RenderItem<TSpectral>& item) {
        if (use_radius_lut) {
            item.effective_radius = lookup_effective_radius(
                radius_lut, item.max_irradiance(), radius_config.min_radius);
        }
    };

    std::vector<RenderItem<TSpectral>> items;

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
        Pixel projected;
        float weight;
        TSpectral irradiance;
        Vec3<float> direction;
        float range; ///< Distance to the source, for the occlusion query's far limit.
        float time;  ///< Shutter parameter in [0, 1], for motion-blurred occluders.
    };

    std::vector<std::vector<ProjectedItem>> tile_bins(static_cast<std::size_t>(num_tiles));

    float max_pixel_step = 0.75f; // TODO Make this configurable

    // Per-thread working storage for the cull-and-bin pass below. Every buffer here
    // is reused across sources, so the pass allocates a bounded amount of memory
    // regardless of catalogue size.
    struct BinScratch {
        std::vector<Vec3<float>> directions;
        TrajectoryArc arc;
        typename Frustum<TSpectral>::ClipScratch clip;
        std::vector<float> params;
        std::vector<Pixel> pixels;
    };

    // Bin one source's visible arc into per-tile lists. Factored out so that stars
    // and unresolved objects share exactly the same code path, in exactly the same
    // order, as the single loop this replaces.
    auto bin_item = [&](const RenderItem<TSpectral>& item,
                        std::size_t item_index,
                        BinScratch& scratch,
                        std::vector<std::vector<ProjectedItem>>& bins) {
        const auto& arc = item.arc;

        // Clip arc to frustum:
        const auto& visible_intervals = camera->view_frustum().clip_arc(arc, scratch.clip);
        if (visible_intervals.empty()) {
            return;
        }

        // For each visible interval, adaptively sample in pixel space:
        for (const auto& [t_start, t_end] : visible_intervals) {

            // Start with the original sample parameter values that fall within
            // this visible interval. For N input samples, these are at
            // t = 0, 1/(N-1), 2/(N-1), ..., 1
            std::vector<float>& params = scratch.params;
            params.clear();
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
            std::vector<Pixel>& pixels = scratch.pixels;
            pixels.assign(params.size(), Pixel{});
            for (std::size_t k = 0; k < params.size(); ++k) {
                Vec3<float> dir = arc.evaluate(params[k]);
                pixels[k] = camera->project_point(dir);
            }

            // Adaptive subdivision: bisect intervals where pixel distance > threshold
            // Process from back to front so insertions don't invalidate indices
            constexpr int MAX_SUBDIVISIONS = 12; // safety limit
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

                        params.insert(params.begin() + static_cast<std::ptrdiff_t>(k), t_mid);
                        pixels.insert(pixels.begin() + static_cast<std::ptrdiff_t>(k), p_mid);
                        subdivided = true;
                    }
                }
                if (!subdivided) {
                    break;
                }
            }

            // Compute weights (proportional to parameter interval around each sample):
            // Each sample represents the midpoint of its surrounding interval.
            std::size_t num_samples = params.size();
            for (std::size_t k = 0; k < num_samples; ++k) {
                float dt;
                if (num_samples == 1) {
                    dt = t_end - t_start;
                } else if (k == 0) {
                    dt = (params[1] - params[0]) / 2.0f;
                } else if (k == num_samples - 1) {
                    dt = (params[k] - params[k - 1]) / 2.0f;
                } else {
                    dt = (params[k + 1] - params[k - 1]) / 2.0f;
                }
                // The arc parameter spans the full exposure on [0, 1], so dt is already the
                // fraction of the exposure this sample represents. Weights over a visible
                // interval therefore sum to the star's visible fraction of the exposure -
                // NOT to 1. Renormalizing by (t_end - t_start) here would compress the full
                // exposure energy into whatever sliver of the trajectory is in frame,
                // overbrightening partially visible streaks (e.g. corner streaks under
                // boresight rotation) by 1 / visible_fraction:
                float weight = dt;

                // Interpolate irradiance at this parameter value:
                TSpectral irrad = item.interpolate_irradiances(params[k]);
                float source_range = item.interpolate_ranges(params[k]);
                Vec3<float> dir = arc.evaluate(params[k]);
                const Pixel& p = pixels[k];
                if (p.x < 0.f || p.x > res_x || p.y < 0.f || p.y > res_y) {
                    continue;
                }

                int tx = std::clamp(static_cast<int>(p.x) / TILE_SIZE, 0, tiles_x - 1);
                int ty = std::clamp(static_cast<int>(p.y) / TILE_SIZE, 0, tiles_y - 1);

                // params[k] is the arc parameter over the full exposure on [0, 1],
                // which is the same parameterization the intersectors take as their
                // motion-blur time, so the occlusion query for this sample sees the
                // scene as it was at the instant this piece of the streak was laid
                // down - not a union of the occluder over the whole exposure.
                bins[static_cast<std::size_t>(ty * tiles_x + tx)].push_back(
                    {item_index, p, weight, irrad, dir, source_range, params[k]});
            }
        }
    };

    if (!star_field.empty()) {
        const std::size_t n_stars = star_field.size();
        const std::size_t n_samples = star_field.sample_count;

        constexpr std::size_t CHUNK = 4096;
        const std::size_t num_chunks = (n_stars + CHUNK - 1) / CHUNK;

        struct Chunk {
            std::vector<RenderItem<TSpectral>> items;
            std::vector<std::vector<ProjectedItem>> bins;
        };
        std::vector<Chunk> chunks(num_chunks);

        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(0, num_chunks),
            [&](const tbb::blocked_range<std::size_t>& range) {
                BinScratch scratch;
                scratch.directions.resize(n_samples);

                for (std::size_t c = range.begin(); c != range.end(); ++c) {
                    Chunk& chunk = chunks[c];
                    chunk.bins.resize(static_cast<std::size_t>(num_tiles));

                    const std::size_t begin = c * CHUNK;
                    const std::size_t end = std::min(begin + CHUNK, n_stars);

                    for (std::size_t i = begin; i < end; ++i) {
                        const Vec3<float>* dirs = star_field.directions_for(i);
                        std::copy(dirs, dirs + n_samples, scratch.directions.begin());
                        scratch.arc.reset(scratch.directions);

                        // Cheap rejection before anything is allocated. This is the
                        // same frustum test the binning pass performs; running it here
                        // only decides whether a RenderItem is worth building.
                        if (camera->view_frustum().clip_arc(scratch.arc, scratch.clip).empty()) {
                            continue;
                        }

                        // A catalogue star's irradiance is constant over the exposure and
                        // its range is fixed at infinity, so both collapse to a single
                        // entry.
                        RenderItem<TSpectral> item(
                            scratch.arc,
                            std::vector<TSpectral>{star_field.irradiances[i]},
                            std::vector<float>{RenderItem<TSpectral>::RANGE_AT_INFINITY},
                            stamp_radius);
                        assign_radius(item);

                        chunk.items.push_back(std::move(item));
                        bin_item(chunk.items.back(), chunk.items.size() - 1, scratch, chunk.bins);
                    }
                }
            });

        // Merge in chunk order, rebasing each chunk's item indices.
        std::size_t total_items = 0;
        for (const auto& chunk : chunks) {
            total_items += chunk.items.size();
        }
        items.reserve(total_items + scene_view.unresolved_objects_.size());

        for (auto& chunk : chunks) {
            const std::size_t base = items.size();
            for (auto& item : chunk.items) {
                items.push_back(std::move(item));
            }
            if (chunk.bins.empty()) {
                continue;
            }
            for (std::size_t t = 0; t < static_cast<std::size_t>(num_tiles); ++t) {
                auto& src = chunk.bins[t];
                if (src.empty()) {
                    continue;
                }
                auto& dst = tile_bins[t];
                dst.reserve(dst.size() + src.size());
                for (auto& proj : src) {
                    proj.item_idx += base;
                    dst.push_back(proj);
                }
            }
        }
    }

    // Unresolved objects (separate from stars):
    if (!scene_view.unresolved_objects_.empty()) {
        BinScratch scratch;
        for (const auto& instance : scene_view.unresolved_objects_) {
            std::vector<Vec3<float>> directions(instance.transforms.size());
            std::vector<TSpectral> irradiances(instance.transforms.size());
            std::vector<float> ranges(instance.transforms.size());
            for (std::size_t i = 0; i < instance.transforms.size(); ++i) {
                const Vec3<float>& position = instance.transforms[i].position;
                directions[i] = glm::normalize(position);
                irradiances[i] = instance.unresolved_object->get_irradiance(times[i]);
                ranges[i] = glm::length(position);
            }
            TrajectoryArc arc(directions);
            RenderItem<TSpectral> item(
                std::move(arc), std::move(irradiances), std::move(ranges), stamp_radius);
            assign_radius(item);

            items.push_back(std::move(item));
            bin_item(items.back(), items.size() - 1, scratch, tile_bins);
        }
    }

    if (items.empty()) {
        wing_splat = Image<TSpectral>(0, 0, TSpectral{0});
        return received_power;
    }

    // Render tiles in parallel:
    const bool test_occlusion = unresolved_occlusion_ && !scene_view.primitives_.empty();
    const bool mask_available = occluder_mask_valid_ && occluder_mask_.width() == fb_width &&
                                occluder_mask_.height() == fb_height;

    int margin = splat_wings ? std::max(stamp_radius, 1) : stamp_radius;

    struct TileBuffer {
        Image<TSpectral> buf;
        Image<TSpectral> splat;
        int origin_x = 0;
        int origin_y = 0;
        int local_w = 0;
        int local_h = 0;
    };

    std::vector<TileBuffer> tile_buffers(static_cast<std::size_t>(num_tiles));

    tbb::parallel_for(
        tbb::blocked_range<int>(0, num_tiles), [&](const tbb::blocked_range<int>& range) {
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
                Image<TSpectral> local_splat(splat_wings ? local_w : 0, splat_wings ? local_h : 0);

                RandomSampler<float> sampler(static_cast<unsigned int>(tile_idx));

                for (const auto& proj : bin) {
                    const auto& item = items[proj.item_idx];
                    const Pixel& star_p = proj.projected;

                    float projected_area = camera->get_projected_aperture_area(proj.direction);
                    TSpectral power = proj.weight * proj.irradiance * projected_area;

                    if (test_occlusion) {
                        const int mx = std::clamp(static_cast<int>(star_p.x), 0, fb_width - 1);
                        const int my = std::clamp(static_cast<int>(star_p.y), 0, fb_height - 1);

                        if (!mask_available || occluder_mask_(mx, my) != 0) {
                            Ray<TSpectral> occlusion_ray(Vec3<float>{0.f, 0.f, 0.f},
                                                         glm::normalize(proj.direction));

                            TSpectral transmittance =
                                scene_view.evaluate_transmittance(occlusion_ray,
                                                                  proj.range,
                                                                  MediumStack<TSpectral>{},
                                                                  sampler,
                                                                  proj.time,
                                                                  AlphaMode::Expected);

                            if (transmittance.max() <= 0.f) {
                                continue;
                            }
                            power = power * transmittance;
                        }
                    }

                    if (splat_wings) {
                        const int bx = static_cast<int>(std::floor(star_p.x));
                        const int by = static_cast<int>(std::floor(star_p.y));
                        const float fx = star_p.x - static_cast<float>(bx);
                        const float fy = star_p.y - static_cast<float>(by);

                        const float w00 = (1.f - fx) * (1.f - fy);
                        const float w10 = fx * (1.f - fy);
                        const float w01 = (1.f - fx) * fy;
                        const float w11 = fx * fy;

                        auto splat_at = [&](int px, int py, float w) {
                            if (w > 0.f && px >= local_x0 && px < local_x1 && py >= local_y0 &&
                                py < local_y1) {
                                local_splat(px - local_x0, py - local_y0) += power * w;
                            }
                        };
                        splat_at(bx, by, w00);
                        splat_at(bx + 1, by, w10);
                        splat_at(bx, by + 1, w01);
                        splat_at(bx + 1, by + 1, w11);
                    }

                    if (stamp_radius > 0) {
                        float floor_x = std::floor(star_p.x);
                        float floor_y = std::floor(star_p.y);
                        float frac_x = star_p.x - floor_x;
                        float frac_y = star_p.y - floor_y;

                        int eff_r = item.effective_radius;

                        if (use_defocus) {
                            const Image<float>& kernel =
                                camera->aperture_->get_defocus_kernel(frac_x, frac_y);

                            int k_offset = camera->aperture_->get_defocus_half_extent() - eff_r;
                            int crop_dim = 2 * eff_r + 1;
                            int start_x = static_cast<int>(floor_x) - eff_r;
                            int start_y = static_cast<int>(floor_y) - eff_r;

                            // Clamp to local tile bounds
                            int kx_begin = std::max(0, local_x0 - start_x);
                            int kx_end = std::min(crop_dim, local_x1 - start_x);
                            int ky_begin = std::max(0, local_y0 - start_y);
                            int ky_end = std::min(crop_dim, local_y1 - start_y);

                            for (int ky = ky_begin; ky < ky_end; ++ky) {
                                int ly = start_y + ky - local_y0;
                                for (int kx = kx_begin; kx < kx_end; ++kx) {
                                    int lx = start_x + kx - local_x0;
                                    // Note: scalar kernel * spectral power
                                    local_buf(lx, ly) +=
                                        power * kernel(kx + k_offset, ky + k_offset);
                                }
                            }
                        } else {
                            const Image<TSpectral>& kernel = camera->get_psf_kernel(frac_x, frac_y);

                            int k_offset = stamp_radius - eff_r;
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
                    } else {
                        int px = static_cast<int>(std::round(star_p.x));
                        int py = static_cast<int>(std::round(star_p.y));
                        if (px >= local_x0 && px < local_x1 && py >= local_y0 && py < local_y1) {
                            local_buf(px - local_x0, py - local_y0) += power;
                        }
                    }
                }

                auto& tb = tile_buffers[static_cast<std::size_t>(tile_idx)];
                tb.buf = std::move(local_buf);
                tb.splat = std::move(local_splat);
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
        if (tb.local_w == 0) {
            continue;
        }
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
                                          if (val[c] != 0.0f) {
                                              nonzero = true;
                                              break;
                                          }
                                      }
                                      if (nonzero) {
                                          received_power(tb.origin_x + lx, y) += val;
                                      }
                                      if (splat_wings) {
                                          const TSpectral& sval = tb.splat(lx, ly);
                                          for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                                              if (sval[c] != 0.0f) {
                                                  wing_splat(tb.origin_x + lx, y) += sval;
                                                  break;
                                              }
                                          }
                                      }
                                  }
                              }
                          }
                      });

    if (use_defocus && camera->convolve_psf_) {
        const Image<TSpectral>& psf = camera->get_psf_convolution_kernel();
        this->convolve_cached_(received_power, psf, *camera, defocus_convolver_);
    }

    auto end_clock = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_clock - start_clock;
    HUIRA_LOG_INFO("Unresolved point source rendering completed in " +
                   std::to_string(elapsed.count()) + " seconds");

    return received_power;
}

/**
 * @brief Convolve an image with a camera kernel, reusing a cached kernel spectrum.
 *
 * Behaviour matches Image::convolve() exactly, including its small-kernel dispatch;
 * only the lifetime of the FFT plan and kernel spectrum differs.
 *
 * @param image Image convolved in place.
 * @param kernel Convolution kernel.
 * @param camera Camera the kernel came from, used to detect kernel changes.
 * @param cache Persistent convolver to reuse.
 */
template <IsSpectral TSpectral>
void Renderer<TSpectral>::convolve_cached_(Image<TSpectral>& image,
                                           const Image<TSpectral>& kernel,
                                           const CameraModel<TSpectral>& camera,
                                           ConvolverCache& cache)
{
    const int kw = kernel.width();
    const int kh = kernel.height();

    if (kw * kh <= 25) {
        image.convolve(kernel);
        return;
    }

    const std::uint64_t version = camera.psf_kernel_version();
    if (!cache.valid || cache.camera != static_cast<const void*>(&camera) ||
        cache.kernel_version != version || cache.image_width != image.width() ||
        cache.image_height != image.height() || cache.kernel_width != kw ||
        cache.kernel_height != kh) {
        cache.convolver.set_kernel(kernel, image.resolution());
        cache.camera = static_cast<const void*>(&camera);
        cache.kernel_version = version;
        cache.image_width = image.width();
        cache.image_height = image.height();
        cache.kernel_width = kw;
        cache.kernel_height = kh;
        cache.valid = true;
    }

    cache.convolver.apply(image);
}
} // namespace huira
