#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <type_traits>
#include <utility>
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

namespace huira {

namespace detail {

/// Whether every channel of a spectral value is finite: neither NaN nor infinite.
template <IsSpectral TSpectral>
[[nodiscard]] inline bool all_finite(const TSpectral& value)
{
    for (std::size_t c = 0; c < TSpectral::size(); ++c) {
        if (!std::isfinite(value[c])) {
            return false;
        }
    }
    return true;
}

/// Length of the arc of the circle of radius r about the origin that lies in x >= a, y >= b,
/// for a, b >= 0.
inline double corner_arc(double a, double b, double r)
{
    if (a * a + b * b >= r * r) {
        return 0.0;
    }
    return r * (std::acos(std::min(b / r, 1.0)) - std::asin(std::min(a / r, 1.0)));
}

/// Length of the circle of radius r about the origin inside the rectangle [x0, x1] x [y0, y1]:
/// the rectangle's parts in each quadrant, reflected into the first, each as four corner arcs.
inline double arc_in_rectangle(double x0, double x1, double y0, double y1, double r)
{
    const auto fold = [](double u0, double u1, std::array<std::array<double, 2>, 2>& spans) {
        if (u1 <= 0.0) {
            spans = {{{-u1, -u0}, {0.0, 0.0}}};
        } else if (u0 >= 0.0) {
            spans = {{{u0, u1}, {0.0, 0.0}}};
        } else {
            spans = {{{0.0, -u0}, {0.0, u1}}};
        }
    };
    std::array<std::array<double, 2>, 2> xs{};
    std::array<std::array<double, 2>, 2> ys{};
    fold(x0, x1, xs);
    fold(y0, y1, ys);
    double length = 0.0;
    for (const auto& [a0, a1] : xs) {
        for (const auto& [b0, b1] : ys) {
            if (a1 > a0 && b1 > b0) {
                length += corner_arc(a0, b0, r) - corner_arc(a1, b0, r) - corner_arc(a0, b1, r) +
                          corner_arc(a1, b1, r);
            }
        }
    }
    return length;
}

} // namespace detail

/**
 * @brief Sets how much of the sensor's read noise (of an electron without read noise) the
 * rounding error of convolving bodies with the PSF in single precision may put in a pixel's
 * readout, as FftConvolver::rounding_error() predicts it, before they are convolved in double
 * precision instead.
 *
 * Each FFT spreads rounding error over the whole frame in proportion to its brightest pixel, so
 * that in single precision, the dark sky beside a body many full wells bright carries a faint
 * pattern of it, positive and negative. At the default, DEFAULT_FFT_ROUNDING_TOLERANCE, it is
 * lost in the read noise of any one frame, but a square-root stretch of a noiseless frame to 30
 * electrons can show it. Single precision takes about 25% less memory for a 1920 x 1080 Visible8
 * frame, and about as long. 0 always convolves in double precision, whose rounding is about
 * 1e-9 of single's.
 *
 * @param fraction_of_read_noise Non-negative and finite.
 * @throws std::runtime_error if it is negative or not finite.
 */
template <IsSpectral TSpectral>
void Renderer<TSpectral>::set_fft_rounding_tolerance(double fraction_of_read_noise)
{
    if (!(fraction_of_read_noise >= 0.0) || !std::isfinite(fraction_of_read_noise)) {
        HUIRA_THROW_ERROR("Renderer::set_fft_rounding_tolerance - The fraction must be "
                          "non-negative and finite: " +
                          std::to_string(fraction_of_read_noise));
    }
    fft_rounding_tolerance_ = fraction_of_read_noise;
}

/**
 * @brief Set how far an unresolved source is drawn: until the light it puts in a pixel, as the
 * sensor reads it out, falls to this fraction of the read noise.
 *
 * With the aperture's diffraction pattern, still sources are drawn at their exact position, pixel
 * by pixel from the PSF's tables, and moving ones along their path, as its line integral (see
 * PsfTables::draw_line()), out to the distance r from the source or its path beyond which no
 * pixel reads out more than tau, this fraction of the sensor's read noise as configured (of an
 * electron for a sensor without read noise; the same whether or not noise is simulated, so that
 * a noiseless frame is the mean of noisy ones). From r to 2 r they fade smoothly to nothing.
 * The light left on the sensor beyond that is added evenly over the frame, so that none is lost
 * and no edge appears. A source's rings are followed only as far as they change a pixel by more
 * than tau; beyond, its light is averaged over them. No pixel is off by more than about 2 tau.
 * A source is always drawn out to where it holds 90% of its light. A source with a PSF set with
 * CameraModel::set_psf() or set_measured_psf(), still or moving, is drawn the same way from the
 * PSF's grid (see GridPsf), without rings to follow, as is the defocus blur, or a point in focus,
 * without a PSF; where moving, its faint wings are interpolated between pixels where that is off
 * by no more than half of tau (see GridPsf::draw_line()). RGB sensors read out each channel;
 * others, their sum.
 *
 * The default, DEFAULT_UNRESOLVED_TAPER, ends halos where a square-root stretch of a noiseless
 * frame to 30 electrons cannot show it. Larger values draw fewer pixels, and so take less time; 0
 * draws every source over the whole frame, which can take a long time.
 *
 * @param fraction_of_read_noise Non-negative and finite.
 * @throws std::runtime_error if it is negative or not finite.
 */
template <IsSpectral TSpectral>
void Renderer<TSpectral>::set_unresolved_taper(double fraction_of_read_noise)
{
    if (!(fraction_of_read_noise >= 0.0) || !std::isfinite(fraction_of_read_noise)) {
        HUIRA_THROW_ERROR("Renderer::set_unresolved_taper - The fraction must be non-negative and "
                          "finite: " +
                          std::to_string(fraction_of_read_noise));
    }
    if (fraction_of_read_noise == 0.0 && unresolved_taper_ != 0.0) {
        HUIRA_LOG_WARNING("Renderer::set_unresolved_taper - With no taper, every unresolved "
                          "source is drawn over the whole frame, which takes as long as drawing "
                          "a frame per source");
    }
    unresolved_taper_ = fraction_of_read_noise;
}

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

    // The optics kernels are built (or rebuilt) here if out of date, before anything is timed,
    // or this throws if the camera's auto precompute is disabled. See CameraModel::precompute().
    camera->precompute_for_render_();

    Image<TSpectral> ray_traced_power = this->path_trace_(scene_view, frame_buffer);

    Image<TSpectral> star_power = this->render_unresolved_(scene_view, frame_buffer);

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
 * @brief Build the cones subtending each TLAS occupant, as seen from the camera.
 *
 * Each occupant's bounding sphere is inflated by the aperture's bounding radius before
 * the cone is taken. Rays do not all leave the camera origin when depth of field is on;
 * they leave points on the aperture. Translating a line by at most R keeps it within R
 * of the original, so a line from any aperture point that meets a sphere of radius r
 * meets the sphere of radius r + R when re-based at the origin. Inflating the sphere
 * therefore makes the origin-centred cone cover every aperture ray's offset exactly.
 * With a finite focus distance those rays are also tilted relative to the pinhole rays;
 * tile_direction_cone_() widens the tile cones by that tilt.
 *
 * @return false when culling must be abandoned for this view.
 */
template <IsSpectral TSpectral>
bool Renderer<TSpectral>::build_occupancy_cones_(SceneView<TSpectral>& scene_view,
                                                 std::vector<DirectionCone>& cones) const
{
    cones.clear();

    if (!scene_view.occupancy_bounds_complete()) {
        return false;
    }

    const auto& camera = scene_view.camera_model_;
    // Only depth-of-field rays leave the origin; otherwise every ray starts at (0,0,0).
    const double aperture_radius = (camera->depth_of_field_ && camera->aperture_)
                                       ? camera->aperture_->get_bounding_radius().to_si()
                                       : 0.0;

    for (const auto& bound : scene_view.occupancy_bounds()) {
        const Vec3<double> center{bound.center};
        const double distance = glm::length(center);
        const double radius = static_cast<double>(bound.radius) + aperture_radius;

        // The camera is inside this occupant: it subtends every direction, so no tile
        // can be rejected and the whole test is worthless for this view.
        if (!(distance > radius)) {
            return false;
        }

        DirectionCone cone;
        cone.axis = center / distance;
        cone.set_half_angle(std::asin(std::clamp(radius / distance, 0.0, 1.0)));
        cones.push_back(cone);
    }

    return true;
}

/**
 * @brief Cone enclosing every ray direction a tile can produce.
 *
 * @param camera Camera model supplying the ray directions.
 * @param x0,y0,x1,y1 The tile's closed pixel rectangle. Jittered samples span exactly
 *                    this range: pixel x is sampled over [x, x + 1).
 */
template <IsSpectral TSpectral>
typename Renderer<TSpectral>::DirectionCone Renderer<TSpectral>::tile_direction_cone_(
    const CameraModel<TSpectral>& camera, float x0, float y0, float x1, float y1) const
{
    // A 5x5 grid over the tile. The interior points are not redundant: with lens
    // distortion the direction field bulges between the corners, and the widening
    // below is only as good as the sample spacing that measures it.
    constexpr int GRID = 5;
    std::array<Vec3<double>, GRID * GRID> dirs;

    for (int j = 0; j < GRID; ++j) {
        const float ty = static_cast<float>(j) / static_cast<float>(GRID - 1);
        for (int i = 0; i < GRID; ++i) {
            const float tx = static_cast<float>(i) / static_cast<float>(GRID - 1);
            const Pixel pixel{x0 + tx * (x1 - x0), y0 + ty * (y1 - y0)};
            dirs[static_cast<std::size_t>(j * GRID + i)] =
                glm::normalize(Vec3<double>{camera.sensor_ray_(pixel).direction()});
        }
    }

    Vec3<double> mean{0.0};
    for (const Vec3<double>& d : dirs) {
        mean += d;
    }

    DirectionCone cone;
    const double mean_length = glm::length(mean);
    if (mean_length <= 0.0) {
        // Degenerate: the tile spans more than a hemisphere. Accept everything.
        cone.axis = Vec3<double>{0.0, 0.0, 1.0};
        cone.set_half_angle(PI<double>());
        return cone;
    }
    cone.axis = mean / mean_length;

    double half_angle = 0.0;
    for (const Vec3<double>& d : dirs) {
        half_angle = std::max(half_angle, angle_between_(cone.axis, d));
    }

    // Widen by the largest gap between adjacent grid points. The true directions
    // between two samples cannot deviate from the straight interpolation by more than
    // roughly the gap itself for a smooth distortion field, so this covers the
    // curvature the grid cannot see. It is an estimate, not a bound - hence
    // set_region_cull_margin_scale() and set_region_cull_validation().
    double max_gap = 0.0;
    auto gap = [&](std::size_t a, std::size_t b) {
        max_gap = std::max(max_gap, angle_between_(dirs[a], dirs[b]));
    };
    for (int j = 0; j < GRID; ++j) {
        for (int i = 0; i + 1 < GRID; ++i) {
            gap(static_cast<std::size_t>(j * GRID + i), static_cast<std::size_t>(j * GRID + i + 1));
        }
    }
    for (int j = 0; j + 1 < GRID; ++j) {
        for (int i = 0; i < GRID; ++i) {
            gap(static_cast<std::size_t>(j * GRID + i),
                static_cast<std::size_t>((j + 1) * GRID + i));
        }
    }

    // Depth-of-field rays are not parallel to the pinhole rays sampled above: with a finite
    // focus distance d each one is tilted towards its focal point F (or, past infinity, away
    // from a virtual one behind the camera). Their origins are offset too, but the occupancy
    // cones already absorb that (see build_occupancy_cones_()); the tilt must be added here.
    // For an aperture point a, |a| <= R, the angle between F and F - a obeys
    // sin(tilt) <= |a| / |F - a| <= R / (|F| - R), and |F| >= |d| because F lies on the focal
    // plane at axial distance |d|. Close enough that this bound reaches a right angle, any
    // direction is possible.
    double dof_tilt = 0.0;
    if (camera.depth_of_field_ && camera.aperture_ && !std::isinf(camera.d_)) {
        const double aperture_radius = camera.aperture_->get_bounding_radius().to_si();
        const double focus_distance = std::abs(static_cast<double>(camera.d_));
        dof_tilt = (focus_distance > 2.0 * aperture_radius)
                       ? std::asin(aperture_radius / (focus_distance - aperture_radius))
                       : PI<double>();
    }

    cone.set_half_angle(
        std::min(PI<double>(),
                 half_angle + static_cast<double>(region_cull_margin_scale_) * max_gap + dof_tilt));
    return cone;
}

/**
 * @brief Path trace a scene view into a frame buffer.
 *
 * Traces camera rays through each pixel, evaluating direct lighting with
 * shadow rays and indirect illumination via recursive path tracing with
 * Russian roulette termination.
 *
 * The rendering is parallelized over tiles using TBB. Each tile accumulates
 * results from multiple samples per pixel (spp_) into the frame buffer: all of
 * them at once, or, with dynamic sampling, in rounds, as many as each pixel and
 * its neighbours need (see set_noise_threshold()).
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

    // Empty-scene fast path (when no geometry is loaded, skip tracing entirely). The frame is
    // nothing but a uniform sky, which the PSF convolution leaves as it is (see "PSF
    // convolution" below), so it is not convolved.
    if (scene_view.tlas_is_empty() && (background == nullptr || background->size() <= 1)) {
        const TSpectral env =
            (background == nullptr || background->size() == 0) ? TSpectral{0.f} : (*background)[0];

        occluder_mask_valid_ = true; // all zero: nothing can occlude anything

        const bool has_power = frame_buffer.has_received_power();
        const bool has_direct = frame_buffer.has_received_direct_power();
        const bool has_indirect = frame_buffer.has_received_indirect_power();
        const bool has_albedo = frame_buffer.has_albedo();
        const bool has_ids = frame_buffer.has_object_ids();
        const bool has_cam_n = frame_buffer.has_camera_normals();
        const bool has_world_n = frame_buffer.has_world_normals();

        // glm::normalize(vec3(0)) is undefined, and the general path feeds it
        // camera_normals * inv_spp, which is exactly zero when every sample misses.
        // Reproduce whatever that produced rather than substituting a "nicer" value.
        const Vec3<float> miss_normal = glm::normalize(Vec3<float>{0.f});

        tbb::parallel_for(
            tbb::blocked_range<int>(0, fb_height), [&](const tbb::blocked_range<int>& rows) {
                for (int y = rows.begin(); y < rows.end(); ++y) {
                    for (int x = 0; x < fb_width; ++x) {
                        if (has_power) {
                            received_power(x, y) = camera->pixel_radiance_to_power(x, y) * env;
                        }
                        if (has_direct) {
                            frame_buffer.received_direct_power()(x, y) =
                                camera->pixel_radiance_to_power(x, y) * env;
                        }
                        if (has_indirect) {
                            frame_buffer.received_indirect_power()(x, y) =
                                camera->pixel_radiance_to_power(x, y) * TSpectral{0.f};
                        }
                        if (has_albedo) {
                            frame_buffer.albedo()(x, y) = TSpectral{0.f};
                        }
                        if (has_ids) {
                            frame_buffer.set_object_ids(x, y, ObjectIds{});
                        }
                        if (has_cam_n) {
                            frame_buffer.camera_normals()(x, y) = miss_normal;
                        }
                        if (has_world_n) {
                            frame_buffer.world_normals()(x, y) =
                                scene_view.camera_to_world_[0].apply_to_direction(miss_normal);
                        }
                        // Depth is left untouched, matching the general path: it only
                        // writes when a finite hit distance was recorded.
                    }
                }
            });

        auto fast_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> fast_elapsed = fast_end - start_clock;
        HUIRA_LOG_INFO("Path tracing (empty scene fast path) completed in " +
                       std::to_string(fast_elapsed.count()) + " seconds");
        return received_power;
    }

    // Tile-based parallel rendering:
    constexpr int TILE_SIZE = 16;
    int tiles_x = (fb_width + TILE_SIZE - 1) / TILE_SIZE;
    int tiles_y = (fb_height + TILE_SIZE - 1) / TILE_SIZE;
    int num_tiles = tiles_x * tiles_y;
    const bool has_motion_blur = scene_view.has_motion_blur();

    // Regions of the frame that provably contain no geometry take the same miss path
    // the empty-scene case above uses. In space scenes the field of view is mostly
    // empty sky even when the scene itself is not, so this is the common case rather
    // than a corner case.
    //
    // Each tile is reduced to a cone of ray directions and tested against the cone
    // subtended by every TLAS occupant. Tiles whose cone is disjoint from all of them
    // cannot register a hit. Per-tile RNG seeding makes this safe: a tile's sampler is
    // seeded from its own index, so skipping one cannot perturb any other.
    //
    // Only worthwhile when the background is uniform. With an environment map the miss
    // path still has to sample it per jittered direction, which is most of the cost
    // that would be saved.
    std::vector<DirectionCone> occupancy_cones;
    const bool uniform_background = (background == nullptr || background->size() <= 1);
    const bool cull_regions = region_culling_ && uniform_background &&
                              build_occupancy_cones_(scene_view, occupancy_cones) &&
                              !occupancy_cones.empty();

    const TSpectral miss_radiance =
        (background == nullptr || background->size() == 0) ? TSpectral{0.f} : (*background)[0];
    const Vec3<float> miss_normal_tile = glm::normalize(Vec3<float>{0.f});

    // Whether a uniform sky is kept out of the PSF convolution, and so which pixels saw nothing
    // but sky (every ray through them missed): see "PSF convolution" below.
    const bool split_sky =
        camera->convolves_bodies_() && uniform_background && miss_radiance.max() > 0.f;
    Image<uint8_t> saw_only_sky(split_sky ? fb_width : 0, split_sky ? fb_height : 0, uint8_t{0});

    std::atomic<int> culled_tiles{0};
    std::atomic<int> validation_failures{0};

    // Samples whose radiance was not finite, and pixels left with none that was:
    std::atomic<std::size_t> dropped_samples{0};
    std::atomic<std::size_t> pixels_without_estimate{0};

    // What the frame buffer records, and where each pixel's samples are summed: in the images
    // that receive them, divided by the samples once the pixel is done (see finish_pixel).
    const bool has_power = frame_buffer.has_received_power();
    const bool has_direct = frame_buffer.has_received_direct_power();
    const bool has_indirect = frame_buffer.has_received_indirect_power();
    const bool has_albedo = frame_buffer.has_albedo();
    const bool has_ids = frame_buffer.has_object_ids();
    const bool has_depth = frame_buffer.has_depth();
    const bool has_camera_normals = frame_buffer.has_camera_normals();
    const bool has_world_normals = frame_buffer.has_world_normals();
    const bool has_normals = has_camera_normals || has_world_normals;
    Image<Vec3<float>> normal_sum(
        has_normals ? fb_width : 0, has_normals ? fb_height : 0, Vec3<float>{0.f});

    // A pixel's progress, kept from one round of samples to the next.
    struct PixelProgress {
        double time_start = 0.0; ///< Where its samples' times start (see trace_pixel).
        float mean = 0.f;        ///< Of its samples' total radiance, kept finite.
        float m2 = 0.f;          ///< Their sum of squared deviations from the mean.
        float closest_depth = std::numeric_limits<float>::infinity();
        int drawn = 0; ///< Samples traced.
        int taken = 0; ///< Those with a finite radiance.
        bool primary_occluder = false;
        bool hit_geometry = false; ///< By any of its rays.
    };

    // What a tile's samples found in one round:
    struct TileTally {
        bool hit_geometry = false;
        std::size_t dropped_samples = 0;
    };

    // Trace a pixel's samples from the next one it has not traced up to (but not including)
    // s_end, adding them to its sums.
    auto trace_pixel = [&](int x,
                           int y,
                           PixelProgress& px,
                           int s_end,
                           RandomSampler<float>& sampler,
                           std::vector<float>& prev_reflector_pdf,
                           TileTally& tally) {
        // With motion blur, each sample is traced at its own time in the exposure. A pixel's
        // samples step through the exposure by the golden ratio from a random start (an
        // additive recurrence), so however many are taken, as when dynamic sampling stops
        // early, they cover it evenly, where independent random times clump and leave gaps: an
        // object that crosses a pixel for a short part of the exposure is caught a steady number
        // of times rather than a random one. Each time is still uniform over the exposure, as
        // the start is random.
        constexpr double inv_golden_ratio = 0.6180339887498949;

        for (int s = px.drawn; s < s_end; ++s) {
            ++px.drawn;
            // Jittered sub-pixel sample:
            float sx = static_cast<float>(x) + sampler.get_1d();
            float sy = static_cast<float>(y) + sampler.get_1d();

            // Generate camera ray from pixel coordinates:
            Ray<TSpectral> ray = camera->sensor_ray_(Pixel{sx, sy}, sampler);

            // The sample's time, from 0 to 1 over the exposure. It is this
            // sample's own: every ray of its path (bounces, shadow rays) is traced
            // at it.
            const float time =
                has_motion_blur
                    ? static_cast<float>(
                          std::fmod(px.time_start + inv_golden_ratio * static_cast<double>(s), 1.0))
                    : 0.f;

            TSpectral throughput{1};
            TSpectral direct_radiance{0};
            TSpectral indirect_radiance{0};

            float prev_roughness = 0.0f;

            float prev_bsdf_pdf = 1.0f;
            Interaction<TSpectral> prev_isect;
            // prev_reflector_pdf, like these, is this path's: the tile keeps the vector only so
            // as not to allocate one for every sample.
            std::fill(prev_reflector_pdf.begin(), prev_reflector_pdf.end(), 0.0f);

            MediumStack<TSpectral> medium_stack;

            for (int bounce = 0; bounce < max_bounces_; ++bounce) {
                HitRecord hit = scene_view.intersect(ray, time);

                if (!medium_stack.is_empty()) {
                    const Medium<TSpectral>* current_medium = medium_stack.top();

                    const float t_seg_start = ray.tnear();
                    const Ray<TSpectral> march_ray(ray.at(t_seg_start), ray.direction());
                    const float t_seg = hit.t - t_seg_start;

                    auto opt_mi = current_medium->sample_free_path(march_ray, sampler);
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
                        throughput = throughput * props.scattering * Tr * (1.0f / pdf);

                        Interaction<TSpectral> vol_isect;
                        vol_isect.position = opt_mi->p;
                        vol_isect.wo = opt_mi->wo;
                        vol_isect.normal_g = Vec3<float>{0.f};
                        vol_isect.normal_s = Vec3<float>{0.f};

                        // As at a surface (see bsdf_ray_follows there), a ray sampled from the
                        // phase function here can find the lights too, and is traced only below
                        // the bounce limit.
                        const bool phase_ray_follows = bounce + 1 < max_bounces_;

                        for (const auto& light_instance : lights) {
                            Transform<float> current_transform =
                                instance_transform_at(light_instance.transforms, time);

                            auto sample = light_instance.light->sample_li(
                                vol_isect, current_transform, sampler);

                            if (!sample) {
                                continue;
                            }

                            const auto& ls = *sample;

                            Ray<TSpectral> shadow_ray(opt_mi->p, ls.wi);
                            TSpectral shadow_transmittance = scene_view.evaluate_transmittance(
                                shadow_ray, ls.distance, medium_stack, sampler, time);

                            if (shadow_transmittance.max() <= 0.0f) {
                                continue;
                            }
                            float phase_val = opt_mi->phase_function->evaluate(opt_mi->wo, ls.wi);
                            const float mis_weight =
                                phase_ray_follows
                                    ? power_heuristic(
                                          ls.pdf, opt_mi->phase_function->pdf(opt_mi->wo, ls.wi))
                                    : 1.0f;

                            TSpectral Ld = throughput * (ls.Li / ls.pdf) * phase_val *
                                           shadow_transmittance * mis_weight;

                            if (bounce == 0) {
                                direct_radiance += Ld;
                            } else {
                                indirect_radiance += Ld;
                            }
                        }

                        // The indirect sources, sampled as at a surface (see there), through
                        // what lets light through, and weighted against phase sampling as the
                        // lights are.
                        const auto& sources = scene_view.indirect_sources();
                        for (std::size_t k = 0; k < sources.size(); ++k) {
                            SphereConeSample cs =
                                sources[k].sample_toward(opt_mi->p, time, sampler);
                            if (cs.pdf <= 0.0f) {
                                continue;
                            }

                            Ray<TSpectral> probe_ray(opt_mi->p, cs.wi);
                            TSpectral Lr =
                                scene_view.indirect_source_radiance(probe_ray,
                                                                    k,
                                                                    medium_stack,
                                                                    sampler,
                                                                    time,
                                                                    bounce + 2 < max_bounces_);
                            if (Lr.max() <= 0.0f) {
                                continue;
                            }

                            const float phase_val =
                                opt_mi->phase_function->evaluate(opt_mi->wo, cs.wi);
                            const float w =
                                phase_ray_follows
                                    ? power_heuristic(
                                          cs.pdf, opt_mi->phase_function->pdf(opt_mi->wo, cs.wi))
                                    : 1.0f;

                            TSpectral Lr_contrib = throughput * phase_val * (Lr / cs.pdf) * w;
                            if (bounce == 0) {
                                direct_radiance += Lr_contrib;
                            } else {
                                indirect_radiance += Lr_contrib;
                            }
                        }

                        PhaseSample ps = opt_mi->phase_function->sample(opt_mi->wo, sampler);

                        float phase_eval = opt_mi->phase_function->evaluate(opt_mi->wo, ps.wi);
                        throughput = throughput * (phase_eval / ps.p);

                        ray = Ray<TSpectral>(opt_mi->p, ps.wi);

                        // A light or an indirect source this ray finds is weighted against their
                        // samples taken here, from this point.
                        prev_bsdf_pdf = ps.p;
                        prev_isect = vol_isect;
                        for (std::size_t k = 0; k < prev_reflector_pdf.size(); ++k) {
                            prev_reflector_pdf[k] = sources[k].pdf_toward(opt_mi->p, time, ps.wi);
                        }
                        continue;

                    } else if (std::isinf(t_seg)) {
                        // The ray leaves the scene without leaving the medium
                        // it entered, as it does after passing through a
                        // surface that encloses nothing (an alpha-cut panel,
                        // say): every primitive carries a medium, a vacuum by
                        // default. Over an infinite path the transmittance is 1
                        // in a vacuum and 0 otherwise; computed as below, it
                        // would be exp(-0 * inf) / exp(-0 * inf): NaN.
                        TSpectral Tr{0.f};
                        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                            Tr[c] = (ext[c] > 0.f) ? 0.f : 1.f;
                        }
                        throughput = throughput * Tr;
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
                    float u = 0.5f + std::atan2(d.z, d.x) * (0.5f * INV_PI<float>());
                    float v = 0.5f - std::asin(std::clamp(d.y, -1.0f, 1.0f)) * INV_PI<float>();

                    TSpectral env_radiance = background->sample_bilinear(u, v);

                    if (bounce == 0) {
                        direct_radiance += throughput * env_radiance;
                    } else {
                        indirect_radiance += throughput * env_radiance;
                    }
                    break;
                }

                tally.hit_geometry = true;
                px.hit_geometry = true;

                const auto& mapping = scene_view.instance_mappings_[hit.inst_id];

                if (bounce == 0 && mapping.type == GeometryType::Primitive) {
                    px.primary_occluder = true;
                }

                if (mapping.type == GeometryType::Light) {
                    const auto& light_instance = lights[mapping.light_index];

                    Vec3<float> hit_p = ray.origin() + ray.direction() * hit.t;
                    Vec3<float> emission_dir = -ray.direction();
                    TSpectral Le = light_instance.light->radiance(hit_p, emission_dir);

                    float mis_weight = 1.0f;

                    // Only apply MIS weighting if this wasn't the first camera ray,
                    // and if it wasn't a perfect mirror reflection (delta BSDF).
                    if (bounce > 0) {
                        Transform<float> current_transform =
                            instance_transform_at(light_instance.transforms, time);

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

                    // Resolve full shading data:
                    Interaction<TSpectral> isect = scene_view.resolve_hit(ray, hit);

                    // Look up mesh material:
                    const auto& batch = scene_view.primitives_[mapping.batch_index];
                    const auto* material = batch.primitive->material.get();

                    // Evaluate material textures to get the opacity parameter:
                    auto [params, shading_isect] = material->evaluate(isect);

                    if (params.opacity < 1.0f) {
                        if (sampler.get_1d() > params.opacity) {
                            ray =
                                Ray<TSpectral>(ray.origin(), ray.direction(), advance_ray_t(hit.t));

                            medium_stack.toggle(batch.primitive.get());

                            bounce--; // Don't count this towards bounce counts
                            continue;
                        }
                    }

                    // The surface's own emission along the ray, from either
                    // side. An emissive surface is not a light: nothing samples
                    // it (lights, nor reflector NEE, whose
                    // direct_lit_radiance() leaves emission out), so it is
                    // only ever reached here, and counted in full, whichever
                    // way the path goes on.
                    const TSpectral Le = throughput * params.emission;
                    if (bounce == 0) {
                        direct_radiance += Le;
                    } else {
                        indirect_radiance += Le;
                    }

                    // What the surface transmits passes straight through it, as shadow rays and
                    // rays toward indirect sources take it to. The path either passes through
                    // or scatters here by the BSDF: it always passes a surface whose BSDF
                    // reflects nothing (the null BSDF), and passes another with a probability
                    // in proportion to its transmission against the most a BSDF can reflect, 1.
                    // Its throughput is divided by the probability of the way it takes, so that
                    // it carries the light of both on average. Passing through, as past alpha,
                    // is not a bounce, and leaves the previous vertex as it was: what the path
                    // finds beyond is weighted against that vertex's samples of lights and
                    // indirect sources, which reach it through this surface too.
                    const float transmission = params.transmission.max();
                    if (transmission > 0.0f) {
                        const float pass_probability =
                            material->bsdf_reflects() ? transmission / (transmission + 1.0f) : 1.0f;
                        if (pass_probability >= 1.0f || sampler.get_1d() < pass_probability) {
                            throughput = throughput * params.transmission / pass_probability;
                            ray =
                                Ray<TSpectral>(ray.origin(), ray.direction(), advance_ray_t(hit.t));
                            medium_stack.toggle(batch.primitive.get());
                            bounce--;
                            continue;
                        }
                        throughput = throughput / (1.0f - pass_probability);
                    }

                    // Path regulatization
                    if (bounce > 0) {
                        params.roughness = std::max(params.roughness, prev_roughness);
                    }
                    prev_roughness = params.roughness;

                    // Record primary ray info:
                    if (bounce == 0) {
                        if (has_ids && hit.t < px.closest_depth) {
                            frame_buffer.set_object_ids(x, y, scene_view.object_ids(hit));
                        }
                        px.closest_depth = std::min(px.closest_depth, hit.t);
                        if (has_normals) {
                            normal_sum(x, y) += shading_isect.normal_s;
                        }
                        if (has_albedo) {
                            frame_buffer.albedo()(x, y) += params.albedo;
                        }
                    }

                    float reflector_nee_weight = 1.0f;
                    const std::size_t hit_source = scene_view.indirect_source_index(hit);
                    if (bounce > 0 && hit_source != SceneView<TSpectral>::NO_INDIRECT_SOURCE) {
                        reflector_nee_weight =
                            power_heuristic(prev_bsdf_pdf, prev_reflector_pdf[hit_source]);
                    }

                    // A ray sampled from the BSDF here is traced only below the bounce limit.
                    // Light it could find is weighted between it and the samples of lights and
                    // indirect sources taken here; at the limit those samples are all there is.
                    const bool bsdf_ray_follows = bounce + 1 < max_bounces_;

                    // Direct lighting (next event estimation):
                    for (const auto& light_instance : lights) {
                        TSpectral Ld = throughput * reflector_nee_weight *
                                       scene_view.sample_light_contribution_(light_instance,
                                                                             isect,
                                                                             material,
                                                                             params,
                                                                             shading_isect,
                                                                             medium_stack,
                                                                             sampler,
                                                                             time,
                                                                             bsdf_ray_follows);
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

                        SphereConeSample cs = source.sample_toward(isect.position, time, sampler);
                        if (cs.pdf <= 0.0f) {
                            continue;
                        }

                        // Evaluate the surface response first
                        TSpectral f = material->bsdf_eval(isect.wo, cs.wi, {params, shading_isect});
                        float cos_theta = std::max(0.0f, glm::dot(shading_isect.normal_s, cs.wi));
                        if (cos_theta <= 0.0f || f.max() <= 0.0f) {
                            continue;
                        }
                        // A surface that transmits nothing receives no light from below itself,
                        // though a shading normal tilted from the geometric one faces directions
                        // there. Light sampling refuses them too (sample_light_contribution_()).
                        if (params.transmission.max() <= 0.0f &&
                            glm::dot(cs.wi, isect.normal_g) <= 0.0f) {
                            continue;
                        }

                        // Trace toward the source's bounding proxy:
                        Vec3<float> probe_normal = (glm::dot(cs.wi, isect.normal_g) < 0.0f)
                                                       ? -isect.normal_g
                                                       : isect.normal_g;
                        Vec3<float> probe_origin =
                            offset_spawn_point(isect.position, probe_normal, isect.p_err);
                        Ray<TSpectral> probe_ray(probe_origin, cs.wi);

                        // Radiance reaching this vertex from the reflector, through whatever
                        // lies between that lets light through, by the direct light it reflects.
                        // A path that samples this direction from the BSDF instead reaches the
                        // reflector at the next bounce (passing what it passes) and shades it as
                        // any surface: its samples of the lights there are weighted against this
                        // sample (below), and the light its own BSDF ray finds is counted by that
                        // path alone. That share is left out here, unless the reflector's BSDF
                        // ray would be past the bounce limit, two bounces on, where nothing finds
                        // it.
                        TSpectral Lr = scene_view.indirect_source_radiance(
                            probe_ray, k, medium_stack, sampler, time, bounce + 2 < max_bounces_);
                        if (Lr.max() <= 0.0f) {
                            continue;
                        }

                        // MIS against BSDF sampling
                        float bsdf_pdf =
                            material->bsdf_pdf(isect.wo, cs.wi, {params, shading_isect});
                        float w = bsdf_ray_follows ? power_heuristic(cs.pdf, bsdf_pdf) : 1.0f;

                        TSpectral Lr_contrib = throughput * f * cos_theta * (Lr / cs.pdf) * w;
                        if (bounce == 0) {
                            direct_radiance += Lr_contrib;
                        } else {
                            indirect_radiance += Lr_contrib;
                        }
                    }

                    // Sample the BSDF:
                    float u1 = sampler.get_1d();
                    float u2 = sampler.get_1d();

                    BSDFSample<TSpectral> bs =
                        material->bsdf_sample(isect.wo, {params, shading_isect}, u1, u2);

                    if (!bs.is_valid()) {
                        break;
                    }

                    // Nor does it reflect light below itself. A BSDF samples directions around
                    // the shading normal, and where a normal map, or normals interpolated across
                    // a mesh, tilt that from the geometric normal, some are below the geometric
                    // surface: following one would trace the path on the far side of an opaque
                    // surface, through it if it is thin. The path ends there instead.
                    if (params.transmission.max() <= 0.0f &&
                        glm::dot(bs.wi, isect.normal_g) <= 0.0f) {
                        break;
                    }

                    // Direction changes: offset along the geometric normal by
                    // the propagated position-error bound (see interaction.hpp).
                    Vec3<float> bounce_normal =
                        (glm::dot(bs.wi, isect.normal_g) < 0.0f) ? -isect.normal_g : isect.normal_g;
                    Vec3<float> bounce_origin =
                        offset_spawn_point(isect.position, bounce_normal, isect.p_err);

                    prev_bsdf_pdf = bs.is_delta ? 0.0f : bs.pdf;
                    prev_isect = shading_isect;

                    for (std::size_t k = 0; k < prev_reflector_pdf.size(); ++k) {
                        prev_reflector_pdf[k] = sources[k].pdf_toward(isect.position, time, bs.wi);
                    }

                    throughput = throughput * bs.value;

                    // Russian roulette (after a few bounces):
                    if (bounce >= 3) {
                        float p_continue = std::clamp(throughput.max(), 0.05f, 0.95f);
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
                indirect_radiance *= (indirect_clamp_threshold_ / current_indirect_max);
            }

            TSpectral sample_radiance = direct_radiance + indirect_radiance;

            // A sample whose radiance is not finite, in any channel, is left out of the pixel's
            // average, and counted (see the warning below).
            if (!detail::all_finite(sample_radiance)) {
                ++tally.dropped_samples;
                continue;
            }

            ++px.taken;

            // The mean and spread of the samples' total radiance (Welford's online update), by
            // which dynamic sampling judges the pixel's noise:
            const float total = sample_radiance.total();
            const float delta = total - px.mean;
            px.mean += delta / static_cast<float>(px.taken);
            px.m2 += delta * (total - px.mean);

            if (has_direct) {
                frame_buffer.received_direct_power()(x, y) += direct_radiance;
            }
            if (has_indirect) {
                frame_buffer.received_indirect_power()(x, y) += indirect_radiance;
            }
            if (has_power) {
                received_power(x, y) += sample_radiance;
            }
        }
    };

    // Start a pixel: clear its sums, which the frame buffer can hold from an earlier render, and
    // draw where its samples' times start.
    auto start_pixel = [&](int x, int y, PixelProgress& px, RandomSampler<float>& sampler) {
        px = PixelProgress{};
        px.time_start = has_motion_blur ? sampler.get_1d() : 0.0;
        if (has_direct) {
            frame_buffer.received_direct_power()(x, y) = TSpectral{0};
        }
        if (has_indirect) {
            frame_buffer.received_indirect_power()(x, y) = TSpectral{0};
        }
        if (has_power) {
            received_power(x, y) = TSpectral{0};
        }
        if (has_albedo) {
            frame_buffer.albedo()(x, y) = TSpectral{0};
        }
        if (has_ids) {
            frame_buffer.set_object_ids(x, y, ObjectIds{});
        }
        if (has_normals) {
            normal_sum(x, y) = Vec3<float>{0.f};
        }
    };

    // Finish a pixel: average its sums. The radiance is averaged over the samples kept; a pixel
    // with none has no estimate of it, and is NaN, which the sensor's readout reports. What the
    // rays hit first (albedo, normals) is averaged over every sample.
    auto finish_pixel =
        [&](int x, int y, const PixelProgress& px, std::size_t& pixels_with_no_estimate) {
            const bool has_estimate = px.taken > 0;
            if (!has_estimate) {
                ++pixels_with_no_estimate;
            }
            const float inv_spp = has_estimate ? 1.0f / static_cast<float>(px.taken) : 0.0f;
            const float inv_drawn = 1.0f / static_cast<float>(px.drawn);
            const TSpectral no_estimate{std::numeric_limits<float>::quiet_NaN()};
            const auto average_power = [&](TSpectral& sum) {
                sum = camera->pixel_radiance_to_power(x, y) *
                      (has_estimate ? sum * inv_spp : no_estimate);
            };

            if (px.primary_occluder) {
                occluder_mask_(x, y) = uint8_t{1};
                any_occluder.store(true, std::memory_order_relaxed);
            }

            if (split_sky && !px.hit_geometry) {
                saw_only_sky(x, y) = uint8_t{1};
            }

            if (has_depth && px.closest_depth < std::numeric_limits<float>::infinity()) {
                frame_buffer.depth()(x, y) = px.closest_depth;
            }

            if (has_albedo) {
                frame_buffer.albedo()(x, y) = frame_buffer.albedo()(x, y) * inv_drawn;
            }

            if (has_normals) {
                const Vec3<float> avg_camera_normals = glm::normalize(normal_sum(x, y) * inv_drawn);
                if (has_camera_normals) {
                    frame_buffer.camera_normals()(x, y) = avg_camera_normals;
                }
                if (has_world_normals) {
                    frame_buffer.world_normals()(x, y) =
                        scene_view.camera_to_world_[0].apply_to_direction(avg_camera_normals);
                }
            }

            if (has_direct) {
                average_power(frame_buffer.received_direct_power()(x, y));
            }
            if (has_indirect) {
                average_power(frame_buffer.received_indirect_power()(x, y));
            }
            if (has_power) {
                average_power(received_power(x, y));
            }
        };

    // Dynamic sampling. The pixels are traced in rounds: first min_spp samples each, then, while
    // any is still noisier than the noise threshold, twice as many as before, up to spp, for the
    // pixels near one. A pixel's noise is the standard error of its mean radiance, relative to
    // that mean: it falls as samples accumulate, and does not depend on how bright the scene is.
    // A pixel samples on while it, or any of its eight neighbours, is too noisy. Judged alone, a
    // pixel whose samples have all happened to find no light (as in a shadow that only light from
    // nearby surfaces reaches) would stop, black, since samples with no spread look exact; among
    // neighbours that found some, it samples on with them. An area that truly gets no light, or
    // nothing but a uniform sky, has no spread anywhere, and stops after the first round.
    //
    // Each tile's samples in a round come from a sampler seeded by the tile and the round, so the
    // image does not depend on how the tiles are shared among threads. Without dynamic sampling
    // there is one round, of spp samples, seeded by the tile alone.
    const bool dynamic = dynamic_sampling_;
    const int first_round_samples = dynamic ? std::min(min_spp_, spp_) : spp_;
    const auto pixel_index = [fb_width](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(fb_width) +
               static_cast<std::size_t>(x);
    };
    std::vector<PixelProgress> progress(dynamic ? pixel_index(0, fb_height) : 0);
    const std::size_t n_tiles = static_cast<std::size_t>(num_tiles);
    std::vector<uint8_t> tile_skipped(n_tiles, uint8_t{0});
    std::vector<uint8_t> tile_was_culled(n_tiles, uint8_t{0});
    std::vector<uint8_t> tile_hit_geometry(n_tiles, uint8_t{0});

    tbb::parallel_for(
        tbb::blocked_range<int>(0, num_tiles), [&](const tbb::blocked_range<int>& range) {
            for (int tile_idx = range.begin(); tile_idx < range.end(); ++tile_idx) {
                const std::size_t tile = static_cast<std::size_t>(tile_idx);
                int tile_y = tile_idx / tiles_x;
                int tile_x = tile_idx % tiles_x;

                int x0 = tile_x * TILE_SIZE;
                int y0 = tile_y * TILE_SIZE;
                int x1 = std::min(x0 + TILE_SIZE, fb_width);
                int y1 = std::min(y0 + TILE_SIZE, fb_height);
                bool tile_culled = false;
                if (cull_regions) {
                    // The jittered sample domain is the closed rectangle [x0, x1] x
                    // [y0, y1]: pixel x1 - 1 is sampled over [x1 - 1, x1).
                    const DirectionCone tile_cone = tile_direction_cone_(*camera,
                                                                         static_cast<float>(x0),
                                                                         static_cast<float>(y0),
                                                                         static_cast<float>(x1),
                                                                         static_cast<float>(y1));

                    tile_culled = true;
                    for (const DirectionCone& occ : occupancy_cones) {
                        if (tile_cone.overlaps(occ)) {
                            tile_culled = false;
                            break;
                        }
                    }
                }

                if (tile_culled) {
                    culled_tiles.fetch_add(1, std::memory_order_relaxed);
                }
                tile_was_culled[tile] = tile_culled ? uint8_t{1} : uint8_t{0};

                // In validation mode nothing is actually skipped; the tile is traced
                // normally and checked afterwards against the decision that was made.
                const bool skip_tile = tile_culled && !region_cull_validation_;

                if (skip_tile) {
                    for (int y = y0; y < y1; ++y) {
                        for (int x = x0; x < x1; ++x) {
                            if (frame_buffer.has_received_power()) {
                                received_power(x, y) =
                                    camera->pixel_radiance_to_power(x, y) * miss_radiance;
                            }
                            if (frame_buffer.has_received_direct_power()) {
                                frame_buffer.received_direct_power()(x, y) =
                                    camera->pixel_radiance_to_power(x, y) * miss_radiance;
                            }
                            if (frame_buffer.has_received_indirect_power()) {
                                frame_buffer.received_indirect_power()(x, y) =
                                    camera->pixel_radiance_to_power(x, y) * TSpectral{0.f};
                            }
                            if (frame_buffer.has_albedo()) {
                                frame_buffer.albedo()(x, y) = TSpectral{0.f};
                            }
                            if (frame_buffer.has_object_ids()) {
                                frame_buffer.set_object_ids(x, y, ObjectIds{});
                            }
                            if (frame_buffer.has_camera_normals()) {
                                frame_buffer.camera_normals()(x, y) = miss_normal_tile;
                            }
                            if (frame_buffer.has_world_normals()) {
                                frame_buffer.world_normals()(x, y) =
                                    scene_view.camera_to_world_[0].apply_to_direction(
                                        miss_normal_tile);
                            }
                            if (split_sky) {
                                saw_only_sky(x, y) = uint8_t{1};
                            }
                            // Depth and the occluder mask keep their cleared values,
                            // matching a tile in which every sample missed.
                        }
                    }
                    tile_skipped[tile] = uint8_t{1};
                    continue;
                }

                // Per-tile RNG seeded from tile index for reproducibility:
                RandomSampler<float> sampler(static_cast<unsigned int>(tile_idx));

                // BSDF-side MIS counterweight: for each designated indirect source,
                // the reflector-NEE PDF of the most recent BSDF-sampled direction.
                std::vector<float> prev_reflector_pdf(scene_view.indirect_sources().size(), 0.0f);

                TileTally tally;
                std::size_t tile_pixels_without_estimate = 0;
                PixelProgress this_pixel{};
                for (int y = y0; y < y1; ++y) {
                    for (int x = x0; x < x1; ++x) {
                        PixelProgress& px = dynamic ? progress[pixel_index(x, y)] : this_pixel;
                        start_pixel(x, y, px, sampler);
                        trace_pixel(
                            x, y, px, first_round_samples, sampler, prev_reflector_pdf, tally);
                        if (!dynamic) {
                            finish_pixel(x, y, px, tile_pixels_without_estimate);
                        }
                    }
                }

                tile_hit_geometry[tile] = tally.hit_geometry ? uint8_t{1} : uint8_t{0};
                if (tally.dropped_samples > 0) {
                    dropped_samples.fetch_add(tally.dropped_samples, std::memory_order_relaxed);
                }
                if (tile_pixels_without_estimate > 0) {
                    pixels_without_estimate.fetch_add(tile_pixels_without_estimate,
                                                      std::memory_order_relaxed);
                }
            }
        });

    if (dynamic) {
        const auto in_skipped_tile = [&](int x, int y) {
            return tile_skipped[static_cast<std::size_t>((y / TILE_SIZE) * tiles_x +
                                                         x / TILE_SIZE)] != 0;
        };

        // Whether a pixel's mean is noisier than the threshold allows: whether the variance of
        // the mean (the samples' variance over their number) exceeds the square of the threshold
        // times the mean. Samples that are all the same have no noise to reduce.
        const auto too_noisy = [&](const PixelProgress& px) {
            if (!(noise_threshold_ > 0.f)) {
                return true; // 0 never stops early
            }
            if (px.taken < 2) {
                return true; // no spread to judge by
            }
            if (!(px.m2 > 0.f)) {
                return false;
            }
            const float n = static_cast<float>(px.taken);
            const float variance_of_mean = px.m2 / (n * (n - 1.f));
            const float allowed = noise_threshold_ * px.mean;
            return variance_of_mean > allowed * allowed;
        };

        std::vector<uint8_t> noisy(progress.size(), uint8_t{0});
        std::vector<uint8_t> active(progress.size(), uint8_t{0});
        std::vector<uint8_t> tile_active(n_tiles, uint8_t{0});
        const auto for_each_row = [&](const auto& row) {
            tbb::parallel_for(tbb::blocked_range<int>(0, fb_height),
                              [&](const tbb::blocked_range<int>& rows) {
                                  for (int y = rows.begin(); y < rows.end(); ++y) {
                                      row(y);
                                  }
                              });
        };

        int samples = first_round_samples;
        for (unsigned int round = 1; samples < spp_; ++round) {
            const int next = samples > spp_ / 2 ? spp_ : 2 * samples;

            for_each_row([&](int y) {
                for (int x = 0; x < fb_width; ++x) {
                    const std::size_t i = pixel_index(x, y);
                    noisy[i] =
                        !in_skipped_tile(x, y) && too_noisy(progress[i]) ? uint8_t{1} : uint8_t{0};
                }
            });

            // A pixel samples on if it is short of this round's samples and it or a neighbour is
            // too noisy:
            for_each_row([&](int y) {
                const int ny0 = std::max(0, y - 1);
                const int ny1 = std::min(fb_height - 1, y + 1);
                for (int x = 0; x < fb_width; ++x) {
                    const std::size_t i = pixel_index(x, y);
                    active[i] = uint8_t{0};
                    if (in_skipped_tile(x, y) || progress[i].drawn >= next) {
                        continue;
                    }
                    const int nx0 = std::max(0, x - 1);
                    const int nx1 = std::min(fb_width - 1, x + 1);
                    for (int ny = ny0; ny <= ny1 && active[i] == 0; ++ny) {
                        for (int nx = nx0; nx <= nx1; ++nx) {
                            if (noisy[pixel_index(nx, ny)] != 0) {
                                active[i] = uint8_t{1};
                                break;
                            }
                        }
                    }
                }
            });

            std::atomic<bool> any_active{false};
            tbb::parallel_for(
                tbb::blocked_range<int>(0, num_tiles), [&](const tbb::blocked_range<int>& range) {
                    for (int tile_idx = range.begin(); tile_idx < range.end(); ++tile_idx) {
                        const int x0 = (tile_idx % tiles_x) * TILE_SIZE;
                        const int y0 = (tile_idx / tiles_x) * TILE_SIZE;
                        const int x1 = std::min(x0 + TILE_SIZE, fb_width);
                        const int y1 = std::min(y0 + TILE_SIZE, fb_height);
                        uint8_t any = 0;
                        for (int y = y0; y < y1 && any == 0; ++y) {
                            for (int x = x0; x < x1; ++x) {
                                if (active[pixel_index(x, y)] != 0) {
                                    any = 1;
                                    break;
                                }
                            }
                        }
                        tile_active[static_cast<std::size_t>(tile_idx)] = any;
                        if (any != 0) {
                            any_active.store(true, std::memory_order_relaxed);
                        }
                    }
                });
            if (!any_active.load()) {
                break;
            }

            tbb::parallel_for(
                tbb::blocked_range<int>(0, num_tiles), [&](const tbb::blocked_range<int>& range) {
                    for (int tile_idx = range.begin(); tile_idx < range.end(); ++tile_idx) {
                        const std::size_t tile = static_cast<std::size_t>(tile_idx);
                        if (tile_active[tile] == 0) {
                            continue;
                        }
                        const int x0 = (tile_idx % tiles_x) * TILE_SIZE;
                        const int y0 = (tile_idx / tiles_x) * TILE_SIZE;
                        const int x1 = std::min(x0 + TILE_SIZE, fb_width);
                        const int y1 = std::min(y0 + TILE_SIZE, fb_height);

                        RandomSampler<float> sampler(round * static_cast<unsigned int>(num_tiles) +
                                                     static_cast<unsigned int>(tile_idx));
                        std::vector<float> prev_reflector_pdf(scene_view.indirect_sources().size(),
                                                              0.0f);
                        TileTally tally;
                        for (int y = y0; y < y1; ++y) {
                            for (int x = x0; x < x1; ++x) {
                                const std::size_t i = pixel_index(x, y);
                                if (active[i] != 0) {
                                    trace_pixel(x,
                                                y,
                                                progress[i],
                                                next,
                                                sampler,
                                                prev_reflector_pdf,
                                                tally);
                                }
                            }
                        }
                        if (tally.hit_geometry) {
                            tile_hit_geometry[tile] = uint8_t{1};
                        }
                        if (tally.dropped_samples > 0) {
                            dropped_samples.fetch_add(tally.dropped_samples,
                                                      std::memory_order_relaxed);
                        }
                    }
                });

            samples = next;
        }

        tbb::parallel_for(
            tbb::blocked_range<int>(0, num_tiles), [&](const tbb::blocked_range<int>& range) {
                for (int tile_idx = range.begin(); tile_idx < range.end(); ++tile_idx) {
                    if (tile_skipped[static_cast<std::size_t>(tile_idx)] != 0) {
                        continue;
                    }
                    const int x0 = (tile_idx % tiles_x) * TILE_SIZE;
                    const int y0 = (tile_idx / tiles_x) * TILE_SIZE;
                    const int x1 = std::min(x0 + TILE_SIZE, fb_width);
                    const int y1 = std::min(y0 + TILE_SIZE, fb_height);
                    std::size_t tile_pixels_without_estimate = 0;
                    for (int y = y0; y < y1; ++y) {
                        for (int x = x0; x < x1; ++x) {
                            finish_pixel(
                                x, y, progress[pixel_index(x, y)], tile_pixels_without_estimate);
                        }
                    }
                    if (tile_pixels_without_estimate > 0) {
                        pixels_without_estimate.fetch_add(tile_pixels_without_estimate,
                                                          std::memory_order_relaxed);
                    }
                }
            });
    }

    for (std::size_t tile = 0; tile < n_tiles; ++tile) {
        if (tile_was_culled[tile] != 0 && tile_hit_geometry[tile] != 0) {
            validation_failures.fetch_add(1, std::memory_order_relaxed);
        }
    }

    const std::size_t dropped = dropped_samples.load();
    if (dropped > 0) {
        const std::size_t empty = pixels_without_estimate.load();
        HUIRA_LOG_WARNING(
            "Renderer::render - " + std::to_string(dropped) +
            " sample(s) had a radiance that is not finite (NaN or infinite), and were left out "
            "of their pixels' averages" +
            (empty > 0
                 ? ", and " + std::to_string(empty) + " pixel(s) had no other sample, so are NaN"
                 : std::string{}) +
            ". This points to a problem upstream, such as a material or light returning a "
            "non-finite radiance.");
    }

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

    // PSF convolution, of resolved bodies with the PSF and scattered light, unless
    // CameraModel::enable_psf_convolution(false).
    //
    // Each pixel's light is spread over the whole frame, as the PSF gives a still source's (see
    // PsfConvolver), so a bright body's halo reaches every pixel. Only what is traced in the
    // frame is spread: a body just beyond its edges adds no halo.
    //
    // The FFTs that convolve run in single precision, unless the rounding error they would
    // spread across the frame, in proportion to its brightest pixel, reaches
    // fft_rounding_tolerance() of the read noise in a pixel's readout (see
    // FftConvolver::rounding_error()); then in double precision, where it is about 1e-9 of that
    // again. Every component is convolved in the same precision, chosen for all of them, so that
    // the total stays the sum of the direct and indirect light.
    //
    // Write R for the frame as traced, and S for the frame the sky alone would give: each
    // pixel's radiance-to-power factor times the sky's radiance L. Convolution (written *) is
    // linear, so convolving R with the PSF P gives
    //
    //     P * R  =  P * S  +  P * (R - S).
    //
    // The convolution only sees the frame: everything beyond its edges counts as black. That is
    // right for R - S, the light that geometry in the frame adds to the sky (or blocks from it,
    // where R - S is negative). It is wrong for S. The sky continues beyond the frame, and a
    // uniform sky convolved with a PSF that sums to one gives the sky back, but computed on the
    // frame alone, P * S darkens toward the frame's edges instead: by 18% in the corners with
    // scattered light that reaches half the frame. So with a uniform sky, the frame is
    // convolved as
    //
    //     S  +  P * (R - S),
    //
    // which takes the known result for the sky, and convolves the rest as before.
    //
    // Pixels that geometry covers only partly need no separate coverage (alpha) channel. A ray
    // that misses brings L, and contributes L - L = 0 to R - S; a ray that hits brings the
    // radiance of what it hit, and contributes that minus L. So where k of a pixel's n rays hit
    // things of mean radiance B, R - S is (k / n) (B - L) times the pixel's factor: weighted by
    // the fraction covered.
    //
    // Where every ray missed, R - S is zero, and it is set to exactly zero, which averaging and
    // then subtracting in floating point only nearly gives. With nothing but sky in view it is
    // zero everywhere, and the convolution is skipped (see step 2).
    //
    // The sky is direct light, so the direct component is split the same way; the indirect
    // component holds none of it. A background image is not uniform, so a frame with one is
    // convolved as it is, and the image darkens toward the frame's edges.
    if (camera->convolves_bodies_()) {
        // Calls f(x, y, s) for every pixel, in parallel, with s the pixel's value in S:
        auto for_each_pixel_with_sky = [&](auto&& f) {
            tbb::parallel_for(
                tbb::blocked_range<int>(0, fb_height), [&](const tbb::blocked_range<int>& rows) {
                    for (int y = rows.begin(); y < rows.end(); ++y) {
                        for (int x = 0; x < fb_width; ++x) {
                            f(x, y, camera->pixel_radiance_to_power(x, y) * miss_radiance);
                        }
                    }
                });
        };

        // With a uniform sky, R becomes R - S:
        if (split_sky) {
            for_each_pixel_with_sky([&](int x, int y, const TSpectral& sky) {
                const bool only_sky = saw_only_sky(x, y) != 0;
                if (has_power) {
                    received_power(x, y) = only_sky ? TSpectral{0.f} : received_power(x, y) - sky;
                }
                if (has_direct) {
                    TSpectral& direct = frame_buffer.received_direct_power()(x, y);
                    direct = only_sky ? TSpectral{0.f} : direct - sky;
                }
            });
        }

        // The components to convolve, each on its own, which keeps total == direct + indirect:
        std::vector<Image<TSpectral>*> components;
        if (has_power && !image_is_zero_(received_power)) {
            components.push_back(&received_power);
        }
        if (has_direct && !image_is_zero_(frame_buffer.received_direct_power())) {
            components.push_back(&frame_buffer.received_direct_power());
        }
        if (frame_buffer.has_received_indirect_power() &&
            !image_is_zero_(frame_buffer.received_indirect_power())) {
            components.push_back(&frame_buffer.received_indirect_power());
        }

        // The most single precision's rounding would put in a pixel's readout, in electrons, for
        // any of them:
        const SensorModel<TSpectral>& sensor = *camera->sensor_;
        const double exposure = scene_view.duration().to_si();
        const TSpectral photon_energies = TSpectral::photon_energies();
        const TSpectral qe = sensor.quantum_efficiency();
        double rounding = 0.0;
        for (const Image<TSpectral>* component : components) {
            const auto error = camera->psf_convolution_error_(*component, FftPrecision::Single);
            double readout = 0.0;
            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                if (!(photon_energies[c] > 0.f)) {
                    continue;
                }
                const double electrons = error[c] * exposure * static_cast<double>(qe[c]) /
                                         static_cast<double>(photon_energies[c]);
                readout = std::is_same_v<TSpectral, RGB> ? std::max(readout, electrons)
                                                         : readout + electrons;
            }
            rounding = std::max(rounding, readout);
        }
        const double read_noise = static_cast<double>(sensor.read_noise());
        const double tolerance = fft_rounding_tolerance_ * (read_noise > 0.0 ? read_noise : 1.0);
        const FftPrecision precision =
            tolerance > 0.0 && rounding <= tolerance ? FftPrecision::Single : FftPrecision::Double;
        const auto convolve_start = std::chrono::steady_clock::now();
        for (Image<TSpectral>* component : components) {
            camera->apply_psf_convolution_(*component, precision, fft_planning_);
        }
        if (!components.empty()) {
            const double seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - convolve_start)
                    .count();
            HUIRA_LOG_INFO("Convolved bodies with the PSF in " + std::to_string(seconds) +
                           " s, in " + (precision == FftPrecision::Single ? "single" : "double") +
                           " precision: single precision's rounding would put up to " +
                           std::to_string(rounding) + " electrons in a pixel's readout, against " +
                           std::to_string(tolerance));
        }

        // With a uniform sky, P * (R - S) becomes S + P * (R - S):
        if (split_sky) {
            for_each_pixel_with_sky([&](int x, int y, const TSpectral& sky) {
                if (has_power) {
                    received_power(x, y) += sky;
                }
                if (has_direct) {
                    frame_buffer.received_direct_power()(x, y) += sky;
                }
            });
        }
    }

    const int culled = culled_tiles.load(std::memory_order_relaxed);
    if (culled > 0) {
        HUIRA_LOG_INFO("Region culling skipped " + std::to_string(culled) + " of " +
                       std::to_string(num_tiles) + " tiles");
    }
    const int failures = validation_failures.load(std::memory_order_relaxed);
    if (failures > 0) {
        HUIRA_LOG_ERROR(
            "Region culling validation FAILED: " + std::to_string(failures) +
            " tile(s) marked empty actually contained geometry. Increase the margin via "
            "Renderer::set_region_cull_margin_scale(), or disable culling entirely with "
            "Renderer::set_region_culling(false).");
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
               std::vector<float> set_range)
        : arc(std::move(set_arc)), irradiance(std::move(set_irradiance)),
          range(std::move(set_range))
    {
    }

    TrajectoryArc arc;
    std::vector<TSpectral> irradiance;

    /// Distance to the source at each temporal sample
    std::vector<float> range;

    static constexpr float RANGE_AT_INFINITY = std::numeric_limits<float>::max();

    /// The PSF's tables it is drawn from, with the diffraction pattern: a star's, or for its own
    /// blur (see CameraModel::tables_at_()).
    const PsfTables<TSpectral>* tables = nullptr;

    /// The grid its core is drawn from, without the diffraction pattern: a star's, or for its own
    /// blur (see CameraModel::grids_at_()); none for a point, in focus without a PSF.
    const GridPsf<TSpectral>* grid = nullptr;

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
 * Each source is drawn from the PSF's tables or grid, at its exact position if it stays put
 * during the exposure and along its path as line integrals if it moves, tapered off where its
 * light fades below set_unresolved_taper()'s threshold, with the light beyond spread over the
 * frame.
 *
 * The rendering pipeline:
 * 1. Collects all stars and unresolved objects, each with the tables and grid for its blur
 * 2. Projects sources to screen space, including those outside the image whose light reaches
 *    into it, and lists each in every tile its light reaches
 * 3. Renders the tiles in parallel, each writing only its own pixels of the frame
 * 4. Spreads the light each left beyond its drawn radius evenly over the frame
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 * @param scene_view The scene view containing stars and unresolved objects
 * @param frame_buffer The frame buffer to render into
 */
template <IsSpectral TSpectral>
Image<TSpectral> Renderer<TSpectral>::render_unresolved_(SceneView<TSpectral>& scene_view,
                                                         FrameBuffer<TSpectral>& frame_buffer)
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

    // Each source's light is drawn in up to two parts, render() having brought the camera's
    // kernels up to date. The tables' part (PsfTables): with the aperture's diffraction pattern,
    // all of it, diffraction and scattered light; with a PSF set in its place, or neither, the
    // scattered light's share, f, if any. The core's part, the rest: a PSF that was set, on its
    // grid (see GridPsf), or without one the defocus blur alone, or in focus a point, drawn
    // exactly (see GridPsf::disc()). Each part's tables or grid hold the source's own blur. A
    // source that stays put during the exposure is drawn from them at its exact position, and a
    // moving one along its path, as line integrals (see PsfTables::draw_line() and
    // GridPsf::draw_line()).
    const bool aperture = camera->uses_diffraction_();
    const PsfTables<TSpectral>* tables =
        aperture || camera->scatter_enabled_ ? camera->render_tables_.get() : nullptr;
    const bool use_tables = tables != nullptr;
    const float tables_share = aperture ? 1.f : use_tables ? camera->scatter_fraction_ : 0.f;
    const float core_share = 1.f - tables_share;
    const bool has_core = core_share > 0.f;
    const GridPsf<TSpectral>* grid = !aperture ? camera->grid_psf_.get() : nullptr;

    const auto& star_field = scene_view.stars_;

    if (star_field.empty() && scene_view.unresolved_objects_.empty()) {
        return received_power;
    }

    std::vector<RenderItem<TSpectral>> items;

    // Sources are rendered in screen-space tiles, in parallel. Each source is listed in every
    // tile its light reaches, and each tile writes only its own pixels, straight into the
    // frame: memory is the frame and the lists, however far sources reach.
    constexpr int TILE_SIZE = 64;

    int tiles_x = (fb_width + TILE_SIZE - 1) / TILE_SIZE;
    int tiles_y = (fb_height + TILE_SIZE - 1) / TILE_SIZE;
    int num_tiles = tiles_x * tiles_y;

    // Unresolved objects (separate from stars). Their irradiances are found one object at a
    // time, before anything is binned, so that the frustum reaches as far as their PSFs.
    //
    // Each is blurred by the defocus at its own depth, its distance along the axis, as path
    // tracing blurs bodies: from a star's tables or grid if that is a star's blur, within
    // PsfTables::BLUR_TOLERANCE, as it is for anything far enough away, and otherwise from tables
    // or a grid for its blur, built the first time it is needed. A moving object takes the blur
    // at its mean inverse depth over the exposure, while in front of the camera. The tables and
    // grids are held here until the render is done.
    //
    // Only objects whose light can reach the image are kept, before any tables are built for
    // them: one behind the camera for the whole exposure is never seen, and one whose blurred
    // light lands nowhere near the image needs none. (A nearby object beside or behind the
    // camera has a large blur for its depth, which would take long to build tables for, and
    // longer the nearer it is.)
    std::vector<std::shared_ptr<const PsfTables<TSpectral>>> object_tables;
    std::vector<std::shared_ptr<const GridPsf<TSpectral>>> object_grids;
    std::vector<RenderItem<TSpectral>> objects;
    if (!scene_view.unresolved_objects_.empty()) {
        objects.reserve(scene_view.unresolved_objects_.size());
        std::vector<double> inverse_depths;
        inverse_depths.reserve(scene_view.unresolved_objects_.size());
        // How far beyond its blur an object's light reaches, in pixel widths: the tables' sources
        // are considered out to a frame's diagonal (see wide_frustum below), and a grid's to its
        // reach.
        const double object_margin =
            std::max(use_tables
                         ? std::hypot(static_cast<double>(fb_width), static_cast<double>(fb_height))
                         : 0.0,
                     grid != nullptr ? grid->reach() : 0.0) +
            2.0;
        const float depth_sign = camera->blender_convention_ ? -1.f : 1.f;
        for (const auto& instance : scene_view.unresolved_objects_) {
            std::vector<Vec3<float>> directions(instance.transforms.size());
            std::vector<TSpectral> irradiances(instance.transforms.size());
            std::vector<float> ranges(instance.transforms.size());
            double inverse_depth = 0.0;
            std::size_t in_front = 0;
            for (std::size_t i = 0; i < instance.transforms.size(); ++i) {
                const Vec3<float>& position = instance.transforms[i].position;
                directions[i] = glm::normalize(position);
                irradiances[i] = (instance.irradiances.size() == 1) ? instance.irradiances.front()
                                                                    : instance.irradiances[i];
                ranges[i] = glm::length(position);
                const double depth = static_cast<double>(depth_sign * position.z);
                if (depth > 0.0) {
                    inverse_depth += 1.0 / depth;
                    ++in_front;
                }
            }
            if (in_front == 0) {
                continue;
            }
            inverse_depth /= static_cast<double>(in_front);
            const double blur = camera->source_blur_(inverse_depth);
            const Frustum<TSpectral> object_frustum = camera->view_frustum_with_margin_(
                static_cast<float>(std::min(blur + object_margin, 1e9)));
            if (std::none_of(directions.begin(), directions.end(), [&](const Vec3<float>& d) {
                    return object_frustum.contains(d);
                })) {
                continue;
            }
            TrajectoryArc arc(directions);
            RenderItem<TSpectral> item(std::move(arc), std::move(irradiances), std::move(ranges));
            inverse_depths.push_back(inverse_depth);
            objects.push_back(std::move(item));
        }
        if (use_tables) {
            object_tables = camera->tables_at_(inverse_depths);
            for (std::size_t i = 0; i < objects.size(); ++i) {
                objects[i].tables = object_tables[i].get();
            }
        }
        if (!aperture) {
            object_grids = camera->grids_at_(inverse_depths);
            for (std::size_t i = 0; i < objects.size(); ++i) {
                objects[i].grid = object_grids[i].get();
            }
        }
    }

    // The pixel's height over its width:
    const Vec2<float> pitch = camera->sensor_->pixel_pitch();
    const double aspect = static_cast<double>(pitch.y) / static_cast<double>(pitch.x);

    // How far the grids drawn from reach, in pixel widths:
    double grid_reach = grid != nullptr ? grid->reach() : 0.0;
    for (const auto& object_grid : object_grids) {
        if (object_grid != nullptr) {
            grid_reach = std::max(grid_reach, object_grid->reach());
        }
    }

    // A source's light reaches its grid's reach, and at least a pixel; sources that far outside
    // the image still light its edge. The frustum they are culled against is widened by that
    // much.
    const int reach =
        std::max(1, static_cast<int>(std::ceil(grid_reach / std::min(aspect, 1.0)))) + 1;
    const Frustum<TSpectral> frustum = camera->view_frustum_with_margin_(static_cast<float>(reach));

    // A source drawn from the tables reaches as far as its own light shows, which for a bright
    // one can be across the frame: so those up to a frame's diagonal outside it are considered
    // too.
    const auto diagonal = static_cast<float>(
        std::ceil(std::hypot(static_cast<double>(fb_width), static_cast<double>(fb_height))));
    const Frustum<TSpectral> wide_frustum =
        use_tables ? camera->view_frustum_with_margin_(diagonal) : frustum;

    // Occlusion of unresolved sources by resolved geometry:
    const bool test_occlusion = unresolved_occlusion_ && !scene_view.primitives_.empty();
    const bool mask_available = occluder_mask_valid_ && occluder_mask_.width() == fb_width &&
                                occluder_mask_.height() == fb_height;

    /// One sample of a source's (possibly motion-blurred) arc, with the power it delivers. A
    /// still source drawn from the tables at its exact position is tapered: drawn in full out to
    /// taper pixel widths, fading to nothing at twice that, or to draw if taper is infinite. Its
    /// rings are followed out to rings pixel widths, and averaged over beyond.
    ///
    /// A moving source drawn from the tables is a line: one straight piece of its path, from
    /// start to end in sensor coordinates, with density light per unit length along it, power
    /// in all, drawn as reach says and tapered as a still source is, by the distance from the
    /// piece.
    struct Sample {
        std::size_t item_idx;
        Pixel projected; ///< Sensor coordinates.
        TSpectral power;
        double taper = 0.0;
        double draw = 0.0;
        double rings = 0.0;
        bool line = false;
        std::array<double, 2> start{};
        std::array<double, 2> end{};
        TSpectral density{0.f};
        typename PsfTables<TSpectral>::LineReach reach{};
        /// Drawn at its exact position, or as a line, from its item's grid rather than its tables,
        /// with the grid's scale (see GridPsf::scale()).
        bool from_grid = false;
        /// For a line drawn from its item's grid, whether from the tables for its direction
        /// rather than directly (see GridPsf::draw_line_direct()), and the share of half of tau
        /// interpolating its faint wings may be off by: its path's lines overlap there, and
        /// their errors add.
        bool from_tables = false;
        float tolerance_share = 1.f;
        std::array<double, TSpectral::size()> scale{};
    };

    // A source that moves less than this during the exposure is drawn once, at its mean
    // position, which is then within half of it of everywhere the source was:
    constexpr float STILL_TOLERANCE = 0.01f; // pixels

    float max_pixel_step = 0.75f; // TODO Make this configurable

    /// A sample of a moving source's path drawn from the tables: where it was at time t (as a
    /// fraction of the exposure), and the light it delivered per unit time; invalid if it was
    /// occluded or cannot be placed.
    struct Vertex {
        double x = 0.0;
        double y = 0.0;
        double t = 0.0;
        TSpectral rate{0.f};
        bool valid = false;
    };

    // Per-thread working storage for the cull-and-bin pass below. Every buffer here is reused
    // across sources, so the pass allocates a bounded amount of memory regardless of catalogue
    // size. The samples a pass makes, and the (tile, sample) pairs that list them in every tile
    // they reach, go into its batch.
    struct BinScratch {
        std::vector<Vec3<float>> directions;
        TrajectoryArc arc;
        typename Frustum<TSpectral>::ClipScratch clip;
        std::vector<float> params;
        std::vector<Pixel> pixels;
        std::vector<float> next_params;
        std::vector<Pixel> next_pixels;
        std::vector<Vertex> path;
        std::vector<double> piece_length;
        std::vector<TSpectral> piece_light;
        std::vector<std::array<double, TSpectral::size()>> piece_density;
    };
    struct Batch {
        std::vector<RenderItem<TSpectral>> items;
        std::vector<Sample> samples;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> tile_samples;
    };

    // How far a still source drawn from the tables reaches. The sensor reads out each channel
    // for RGB, and the channels' sum otherwise; the light a pixel reads out, against tau, decides
    // how far a source is drawn. tau is a fraction of the read noise as configured, whether or
    // not noise is simulated, so that a noiseless frame is the mean of noisy ones, or of an
    // electron for a sensor without read noise. See set_unresolved_taper().
    constexpr bool PER_CHANNEL_READOUT = std::is_same_v<TSpectral, RGB>;
    std::array<double, TSpectral::size()> electrons_per_power{};
    double tau = 0.0;
    {
        const SensorModel<TSpectral>& sensor = *camera->sensor_;
        const double read_noise = static_cast<double>(sensor.read_noise());
        tau = unresolved_taper_ * (read_noise > 0.0 ? read_noise : 1.0);
        const double exposure = scene_view.duration().to_si();
        const TSpectral photon_energies = TSpectral::photon_energies();
        const TSpectral qe = sensor.quantum_efficiency();
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            if (photon_energies[c] > 0.f) {
                electrons_per_power[c] =
                    exposure * static_cast<double>(qe[c]) / static_cast<double>(photon_energies[c]);
            }
        }
    }

    // However faint, a source is drawn out to where it holds 90% of its light, and a line to
    // where it holds 90% of its light per unit length. That is the core's when the tables hold
    // only the scattered light, which is then drawn only as far as it shows: it holds 90% of its
    // own light only far out (50 px for a slope of 2.5 with a half-pixel shoulder), which would
    // draw every faint source that far.
    auto floor_radius = [&](const PsfTables<TSpectral>& source_tables) {
        double radius = 0.0;
        if (!aperture) {
            return radius;
        }
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            radius = std::max(radius, source_tables.radius_holding_90(c));
        }
        return radius;
    };
    auto line_floor = [&](const PsfTables<TSpectral>& source_tables) {
        double radius = 0.0;
        if (!aperture) {
            return radius;
        }
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            radius = std::max(radius, source_tables.line_holding_90(c));
        }
        return radius;
    };

    // The smallest taper radius the searches below resolve, in pixel widths: a source tapered
    // to this puts nothing in any pixel but one it is centered on.
    constexpr double MIN_TAPER = 1e-6;

    // Full out to the taper radius, then falling smoothly (with two continuous derivatives) to
    // nothing at twice it; full everywhere with no taper.
    auto taper_weight = [](double r, double taper) {
        if (r <= taper) {
            return 1.0;
        }
        const double t = std::min((r - taper) / taper, 1.0);
        return 1.0 - t * t * t * (10.0 - 15.0 * t + 6.0 * t * t);
    };

    // The distance from a position to the frame's farthest corner, in pixel widths.
    auto farthest_corner = [&](const Pixel& p) {
        const double x = std::max(static_cast<double>(p.x), fb_width - static_cast<double>(p.x));
        const double y = std::max(static_cast<double>(p.y), fb_height - static_cast<double>(p.y));
        return std::hypot(x, aspect * y) + 1.0;
    };

    // The distance beyond which no pixel reads out more than tau from a source of this power,
    // drawn from these tables with its rings followed within rings (see PsfTables::envelope()),
    // found by bisection; infinite with no taper, or if the source is bright enough to stay
    // above tau across the frame.
    auto taper_radius =
        [&](const TSpectral& power, double rings, const PsfTables<TSpectral>& source_tables) {
            if (!(tau > 0.0)) {
                return std::numeric_limits<double>::infinity();
            }
            std::array<double, TSpectral::size()> electrons{};
            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                electrons[c] = static_cast<double>(power[c]) * electrons_per_power[c];
            }
            const auto readout = [&](double r) {
                double value = 0.0;
                for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                    const double channel = electrons[c] * source_tables.envelope(c, r, rings);
                    value = PER_CHANNEL_READOUT ? std::max(value, channel) : value + channel;
                }
                return value;
            };
            const double limit = 2.0 * static_cast<double>(diagonal);
            const double floor = floor_radius(source_tables);
            double hi = std::max(floor, 1.0);
            while (readout(hi) > tau) {
                if (hi > limit) {
                    return std::numeric_limits<double>::infinity();
                }
                hi *= 2.0;
            }
            // To a thousandth of the radius; a source whose brightest pixel is under tau has
            // none to speak of, and the search stops at MIN_TAPER rather than halving its way
            // down to the smallest double (over a thousand steps per source).
            double lo = 0.0;
            while (hi - lo > 1e-3 * hi && hi > MIN_TAPER) {
                const double mid = 0.5 * (lo + hi);
                (readout(mid) > tau ? lo : hi) = mid;
            }
            return std::max(hi, floor);
        };

    // The same for a source drawn from a PSF's grid, which reaches no further than the grid:
    // infinite if it stays above tau that far.
    auto grid_taper_radius = [&](const TSpectral& power, const GridPsf<TSpectral>& source_grid) {
        if (!(tau > 0.0)) {
            return std::numeric_limits<double>::infinity();
        }
        std::array<double, TSpectral::size()> electrons{};
        double floor = 0.0;
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            electrons[c] = static_cast<double>(power[c]) * electrons_per_power[c];
            floor = std::max(floor, source_grid.radius_holding_90(c));
        }
        const auto readout = [&](double r) {
            double value = 0.0;
            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                const double channel = electrons[c] * source_grid.envelope(c, r);
                value = PER_CHANNEL_READOUT ? std::max(value, channel) : value + channel;
            }
            return value;
        };
        const double limit = source_grid.reach();
        if (readout(limit) > tau) {
            return std::numeric_limits<double>::infinity();
        }
        double lo = 0.0;
        double hi = limit;
        while (hi - lo > 1e-3 * hi && hi > MIN_TAPER) {
            const double mid = 0.5 * (lo + hi);
            (readout(mid) > tau ? lo : hi) = mid;
        }
        return std::max(hi, floor);
    };

    // How far a line drawn from a PSF's grid with the given light per unit length is drawn, as
    // for a still source: no further than the grid reaches, where it is drawn exactly all
    // along, and to where a still source holds 90% of its light however faint, which is at least
    // as far as a line in any direction holds 90% of its light per unit length.
    auto grid_line_taper = [&](const TSpectral& density, const GridPsf<TSpectral>& source_grid) {
        if (!(tau > 0.0)) {
            return std::numeric_limits<double>::infinity();
        }
        std::array<double, TSpectral::size()> electrons{};
        double floor = 0.0;
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            electrons[c] = static_cast<double>(density[c]) * electrons_per_power[c];
            floor = std::max(floor, source_grid.radius_holding_90(c));
        }
        const auto readout = [&](double r) {
            double value = 0.0;
            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                const double channel = electrons[c] * source_grid.line_envelope(c, r);
                value = PER_CHANNEL_READOUT ? std::max(value, channel) : value + channel;
            }
            return value;
        };
        const double limit = source_grid.reach();
        if (readout(limit) > tau) {
            return std::numeric_limits<double>::infinity();
        }
        double lo = 0.0;
        double hi = limit;
        while (hi - lo > 1e-3 * hi && hi > MIN_TAPER) {
            const double mid = 0.5 * (lo + hi);
            (readout(mid) > tau ? lo : hi) = mid;
        }
        return std::max(hi, floor);
    };

    // The distance beyond which following the source's rings changes no pixel's readout by more
    // than tau (see PsfTables::ring_deviation()), found by bisection to a twentieth of a pixel;
    // infinite with no taper.
    auto rings_radius = [&](const TSpectral& power, const PsfTables<TSpectral>& source_tables) {
        if (!(tau > 0.0)) {
            return std::numeric_limits<double>::infinity();
        }
        std::array<double, TSpectral::size()> electrons{};
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            electrons[c] = static_cast<double>(power[c]) * electrons_per_power[c];
        }
        const auto readout = [&](double r) {
            double value = 0.0;
            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                if (electrons[c] > 0.0) {
                    const double channel = electrons[c] * source_tables.ring_deviation(c, r);
                    value = PER_CHANNEL_READOUT ? std::max(value, channel) : value + channel;
                }
            }
            return value;
        };
        // Beyond the projected integrals' reach the rings are never followed, and the readout
        // is 0.
        double lo = 0.0;
        double hi = 1.0;
        while (readout(hi) > tau) {
            lo = hi;
            hi *= 2.0;
        }
        while (hi - lo > 0.05) {
            const double mid = 0.5 * (lo + hi);
            (readout(mid) > tau ? lo : hi) = mid;
        }
        return hi;
    };

    // List a sample in the tiles its light reaches: drawn at its exact position from its item's
    // tables or grid, out to its drawn radius, and kept even if that misses the frame, for the
    // light it puts on it beyond.
    enum class Draw { Tables, Grid };
    auto emit_sample = [&](const RenderItem<TSpectral>& item,
                           std::size_t item_index,
                           const Pixel& p,
                           const TSpectral& power,
                           Draw how,
                           Batch& batch) {
        int x_lo = 0;
        int y_lo = 0;
        int x_hi = 0;
        int y_hi = 0;
        Sample sample{item_index, p, power};
        if (how == Draw::Grid) {
            sample.from_grid = true;
            sample.scale = item.grid->scale(static_cast<double>(p.x), static_cast<double>(p.y));
            sample.taper = grid_taper_radius(power, *item.grid);
            sample.draw =
                std::min(std::isinf(sample.taper) ? farthest_corner(p) : 2.0 * sample.taper,
                         item.grid->reach());
        } else {
            sample.rings = rings_radius(power, *item.tables);
            sample.taper = taper_radius(power, sample.rings, *item.tables);
            sample.draw = std::isinf(sample.taper) ? farthest_corner(p) : 2.0 * sample.taper;
        }
        const double reach_x = sample.draw;
        const double reach_y = sample.draw / aspect;
        x_lo = static_cast<int>(std::floor(std::max(static_cast<double>(p.x) - reach_x, -1.0)));
        x_hi = static_cast<int>(std::floor(
            std::min(static_cast<double>(p.x) + reach_x, static_cast<double>(fb_width))));
        y_lo = static_cast<int>(std::floor(std::max(static_cast<double>(p.y) - reach_y, -1.0)));
        y_hi = static_cast<int>(std::floor(
            std::min(static_cast<double>(p.y) + reach_y, static_cast<double>(fb_height))));
        x_lo = std::max(x_lo, 0);
        y_lo = std::max(y_lo, 0);
        x_hi = std::min(x_hi, fb_width - 1);
        y_hi = std::min(y_hi, fb_height - 1);
        const bool reaches_image = x_lo <= x_hi && y_lo <= y_hi;

        const auto sample_index = static_cast<std::uint32_t>(batch.samples.size());
        batch.samples.push_back(sample);
        if (!reaches_image) {
            return;
        }
        for (int ty = y_lo / TILE_SIZE; ty <= y_hi / TILE_SIZE; ++ty) {
            for (int tx = x_lo / TILE_SIZE; tx <= x_hi / TILE_SIZE; ++tx) {
                batch.tile_samples.emplace_back(static_cast<std::uint32_t>(ty * tiles_x + tx),
                                                sample_index);
            }
        }
    };

    // How far a line (see Sample) with the given light per unit length, per channel, is drawn,
    // and how closely, against tau as for a still source: its taper radius (infinite with no
    // taper, or if it stays above tau across the frame), and where its line spread's rings
    // stop mattering, near the line and near its ends (see PsfTables::LineReach).
    struct LineRadii {
        typename PsfTables<TSpectral>::LineReach reach;
        double taper = 0.0;
    };
    auto line_radii = [&](const TSpectral& density, const PsfTables<TSpectral>& source_tables) {
        LineRadii radii;
        if (!(tau > 0.0)) {
            radii.taper = std::numeric_limits<double>::infinity();
            radii.reach.ends_exact_within = source_tables.near_radius();
            return radii;
        }
        std::array<double, TSpectral::size()> electrons{};
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            electrons[c] = static_cast<double>(density[c]) * electrons_per_power[c];
        }
        const auto readout = [&](const auto& bound, double r) {
            double value = 0.0;
            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                if (electrons[c] > 0.0) {
                    const double channel = electrons[c] * bound(c, r);
                    value = PER_CHANNEL_READOUT ? std::max(value, channel) : value + channel;
                }
            }
            return value;
        };
        // The smallest r from start out at which the readout falls to tau, to a twentieth of a
        // pixel; infinite if it stays above tau to limit.
        const auto first_below = [&](const auto& bound, double start, double limit) {
            double lo = 0.0;
            double hi = std::max(start, 0.5);
            while (readout(bound, hi) > tau) {
                if (hi > limit) {
                    return std::numeric_limits<double>::infinity();
                }
                lo = hi;
                hi *= 2.0;
            }
            while (hi - lo > 0.05) {
                const double mid = 0.5 * (lo + hi);
                (readout(bound, mid) > tau ? lo : hi) = mid;
            }
            return hi;
        };
        const double limit = 2.0 * static_cast<double>(diagonal);
        const auto envelope = [&](std::size_t c, double r) {
            return source_tables.line_envelope(c, r);
        };
        const auto half = [&](std::size_t c, double r) {
            return 0.5 * source_tables.line_envelope(c, r);
        };
        const auto rings = [&](std::size_t c, double r) {
            return source_tables.line_deviation(c, r);
        };
        const auto ends = [&](std::size_t c, double r) {
            return source_tables.end_deviation(c, r);
        };
        const double floor = line_floor(source_tables);
        radii.taper = std::max(first_below(envelope, floor, limit), floor);
        radii.reach.exact_within = first_below(rings, 0.5, limit);
        radii.reach.ends_exact_within =
            std::min(first_below(ends, 0.5, limit), source_tables.near_radius());
        radii.reach.ends_within = first_below(half, 0.5, limit);
        return radii;
    };

    // List a line, with its power spread evenly along it, in the tiles its light reaches: those
    // within its drawn radius of it. A line whose light misses the frame is still kept, for the
    // light it puts on it beyond.
    auto emit_line = [&](std::size_t item_index,
                         const std::array<double, 2>& start,
                         const std::array<double, 2>& end,
                         double length,
                         const TSpectral& power,
                         const LineRadii& radii,
                         const GridPsf<TSpectral>* source_grid,
                         Batch& batch) {
        Sample sample{item_index,
                      Pixel{static_cast<float>(0.5 * (start[0] + end[0])),
                            static_cast<float>(0.5 * (start[1] + end[1]))},
                      power};
        sample.line = true;
        sample.start = start;
        sample.end = end;
        sample.density = power * static_cast<float>(1.0 / length);
        sample.taper = radii.taper;
        sample.reach = radii.reach;
        if (std::isinf(radii.taper)) {
            const Pixel a{static_cast<float>(start[0]), static_cast<float>(start[1])};
            const Pixel b{static_cast<float>(end[0]), static_cast<float>(end[1])};
            sample.draw = std::max(farthest_corner(a), farthest_corner(b));
        } else {
            sample.draw = 2.0 * radii.taper;
        }
        if (source_grid != nullptr) {
            // Drawn from a grid, which reaches no further:
            sample.from_grid = true;
            sample.draw = std::min(sample.draw, source_grid->reach());
        }
        sample.reach.reach = sample.draw;

        const double reach_x = sample.draw;
        const double reach_y = sample.draw / aspect;
        const double x_min = std::min(start[0], end[0]) - reach_x;
        const double x_max = std::max(start[0], end[0]) + reach_x;
        const double y_min = std::min(start[1], end[1]) - reach_y;
        const double y_max = std::max(start[1], end[1]) + reach_y;
        const int x_lo = std::max(0, static_cast<int>(std::floor(std::max(x_min, -1.0))));
        const int x_hi =
            std::min(fb_width - 1,
                     static_cast<int>(std::floor(std::min(x_max, static_cast<double>(fb_width)))));
        const int y_lo = std::max(0, static_cast<int>(std::floor(std::max(y_min, -1.0))));
        const int y_hi =
            std::min(fb_height - 1,
                     static_cast<int>(std::floor(std::min(y_max, static_cast<double>(fb_height)))));
        const auto sample_index = static_cast<std::uint32_t>(batch.samples.size());
        batch.samples.push_back(sample);
        if (x_lo > x_hi || y_lo > y_hi) {
            return;
        }
        // A tile is listed if its center is within the drawn radius plus its own half diagonal
        // of the piece, in pixel widths.
        const double span_x = end[0] - start[0];
        const double span_y = aspect * (end[1] - start[1]);
        const double length2 = span_x * span_x + span_y * span_y;
        const double tile_half = 0.5 * std::hypot(1.0, aspect) * static_cast<double>(TILE_SIZE);
        for (int ty = y_lo / TILE_SIZE; ty <= y_hi / TILE_SIZE; ++ty) {
            for (int tx = x_lo / TILE_SIZE; tx <= x_hi / TILE_SIZE; ++tx) {
                const double cx = (static_cast<double>(tx) + 0.5) * TILE_SIZE - start[0];
                const double cy = aspect * ((static_cast<double>(ty) + 0.5) * TILE_SIZE - start[1]);
                const double along =
                    length2 > 0.0 ? std::clamp((cx * span_x + cy * span_y) / length2, 0.0, 1.0)
                                  : 0.0;
                const double distance = std::hypot(cx - along * span_x, cy - along * span_y);
                if (distance <= sample.draw + tile_half) {
                    batch.tile_samples.emplace_back(static_cast<std::uint32_t>(ty * tiles_x + tx),
                                                    sample_index);
                }
            }
        }
    };

    // A moving source's path through its samples as lines: the samples' light per unit time
    // (rate), with the times between them, joined into straight pieces wherever the path stays
    // within LINE_TOLERANCE of a chord and the light per unit length within DENSITY_TOLERANCE of
    // where the line starts. A sample that is occluded, or cannot be placed, breaks the path.
    constexpr double LINE_TOLERANCE = 1e-3;    // pixels
    constexpr double DENSITY_TOLERANCE = 1e-3; // relative
    auto emit_path = [&](std::size_t item_index,
                         const PsfTables<TSpectral>* source_tables,
                         const GridPsf<TSpectral>* source_grid,
                         float share,
                         BinScratch& scratch,
                         Batch& batch) {
        const std::vector<Vertex>& path = scratch.path;
        const std::size_t count = path.size();
        // Each piece's length, in pixel widths, light and light per unit length; a piece with
        // none, or between vertices that are not both valid, cannot be drawn.
        scratch.piece_length.assign(count, 0.0);
        scratch.piece_light.assign(count, TSpectral{0.f});
        scratch.piece_density.assign(count, {});
        for (std::size_t k = 0; k + 1 < count; ++k) {
            if (!path[k].valid || !path[k + 1].valid) {
                continue;
            }
            const double dx = path[k + 1].x - path[k].x;
            const double dy = aspect * (path[k + 1].y - path[k].y);
            const double length = std::sqrt(dx * dx + dy * dy);
            const double dt = path[k + 1].t - path[k].t;
            TSpectral& light = scratch.piece_light[k];
            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                light[c] = static_cast<float>(0.5 * dt * static_cast<double>(share) *
                                              (static_cast<double>(path[k].rate[c]) +
                                               static_cast<double>(path[k + 1].rate[c])));
                scratch.piece_density[k][c] =
                    length > 0.0 ? static_cast<double>(light[c]) / length : 0.0;
            }
            scratch.piece_length[k] = light.max() > 0.f ? length : 0.0;
        }
        const auto usable = [&](std::size_t k) { return scratch.piece_length[k] > 0.0; };
        // Whether vertex v is within LINE_TOLERANCE of the chord from vertex a to vertex b.
        const auto near_chord = [&](std::size_t a, std::size_t b, std::size_t v) {
            const double cx = path[b].x - path[a].x;
            const double cy = aspect * (path[b].y - path[a].y);
            const double vx = path[v].x - path[a].x;
            const double vy = aspect * (path[v].y - path[a].y);
            const double cross = cx * vy - cy * vx;
            return cross * cross <= LINE_TOLERANCE * LINE_TOLERANCE * (cx * cx + cy * cy);
        };
        constexpr float RADII_TOLERANCE = 0.01f;
        const std::size_t first_sample = batch.samples.size();
        LineRadii radii;
        TSpectral radii_density{0.f};
        bool have_radii = false;
        // A line is no brighter at a distance than a still source with all of its light there,
        // which for a short one is the tighter bound: the bound for an endless line at its light
        // per unit length is about 2 r / length too high at r, which drew a source moving just
        // over STILL_TOLERANCE hundreds of pixels out. Found again once the line's light changes
        // by RADII_TOLERANCE.
        double point_taper = 0.0;
        TSpectral point_power{0.f};
        bool have_point_taper = false;
        std::size_t k = 0;
        while (k + 1 < count) {
            if (!usable(k)) {
                ++k;
                continue;
            }
            // Extend the line from piece k while the path stays straight, checking the vertices
            // a quarter, half and three quarters along and the last, and its density steady.
            const std::size_t first = k;
            std::size_t last = k;
            const auto& first_density = scratch.piece_density[first];
            while (last + 2 < count && usable(last + 1)) {
                const std::size_t end_vertex = last + 2;
                bool straight = true;
                for (const std::size_t v : {first + (end_vertex - first) / 4,
                                            first + (end_vertex - first) / 2,
                                            first + 3 * (end_vertex - first) / 4,
                                            end_vertex - 1}) {
                    if (v > first && v < end_vertex && !near_chord(first, end_vertex, v)) {
                        straight = false;
                    }
                }
                if (!straight) {
                    break;
                }
                // A line is drawn with the same light per unit length all along, so it ends
                // where that changes:
                const auto& next_density = scratch.piece_density[last + 1];
                bool steady = true;
                for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                    const double scale = std::max({first_density[c], next_density[c], 1e-300});
                    if (std::abs(next_density[c] - first_density[c]) > DENSITY_TOLERANCE * scale) {
                        steady = false;
                    }
                }
                if (!steady) {
                    break;
                }
                ++last;
            }
            // The line from vertex first to vertex last + 1, with the pieces' light:
            TSpectral power{0.f};
            for (std::size_t piece = first; piece <= last; ++piece) {
                power += scratch.piece_light[piece];
            }
            const std::array<double, 2> start{path[first].x, path[first].y};
            const std::array<double, 2> end{path[last + 1].x, path[last + 1].y};
            const double dx = end[0] - start[0];
            const double dy = aspect * (end[1] - start[1]);
            const double length = std::sqrt(dx * dx + dy * dy);
            if (length > 0.0) {
                // The lines of a path have much the same light per unit length, and so how far
                // they are drawn, found again only once it changes by RADII_TOLERANCE:
                const TSpectral density = power * static_cast<float>(1.0 / length);
                bool same = have_radii;
                for (std::size_t c = 0; same && c < TSpectral::size(); ++c) {
                    same = std::abs(density[c] - radii_density[c]) <=
                           RADII_TOLERANCE * std::max(density[c], radii_density[c]);
                }
                if (!same) {
                    if (source_grid != nullptr) {
                        radii = LineRadii{};
                        radii.taper = grid_line_taper(density, *source_grid);
                    } else {
                        radii = line_radii(density, *source_tables);
                    }
                    radii_density = density;
                    have_radii = true;
                }
                bool same_power = have_point_taper;
                for (std::size_t c = 0; same_power && c < TSpectral::size(); ++c) {
                    same_power = std::abs(power[c] - point_power[c]) <=
                                 RADII_TOLERANCE * std::max(power[c], point_power[c]);
                }
                if (!same_power) {
                    point_taper = source_grid != nullptr
                                      ? grid_taper_radius(power, *source_grid)
                                      : taper_radius(power,
                                                     std::numeric_limits<double>::infinity(),
                                                     *source_tables);
                    point_power = power;
                    have_point_taper = true;
                }
                LineRadii line_radii_here = radii;
                line_radii_here.taper = std::min(radii.taper, point_taper);
                emit_line(
                    item_index, start, end, length, power, line_radii_here, source_grid, batch);
            }
            k = last + 1;
        }
        // The path's lines drawn from a grid share the tolerance for interpolating their wings:
        if (source_grid != nullptr && batch.samples.size() > first_sample + 1) {
            const float lines = static_cast<float>(batch.samples.size() - first_sample);
            for (std::size_t i = first_sample; i < batch.samples.size(); ++i) {
                batch.samples[i].tolerance_share = 1.f / lines;
            }
        }
    };

    // Sample one source's visible arc, find the power each sample delivers, and list it in the
    // tiles it reaches. Stars and unresolved objects share exactly this code path.
    auto bin_item = [&](const RenderItem<TSpectral>& item,
                        std::size_t item_index,
                        BinScratch& scratch,
                        RandomSampler<float>& sampler,
                        Batch& batch) {
        const auto& arc = item.arc;

        // Clip arc to frustum, which reaches as far as the grids do; drawn from the tables, a
        // source's light can reach across the frame (see wide_frustum).
        const auto& visible_intervals =
            (use_tables ? wide_frustum : frustum).clip_arc(arc, scratch.clip);
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
                pixels[k] = camera->project_to_sensor_(dir);
            }

            // Adaptive subdivision: bisect intervals where pixel distance > threshold, each pass
            // into fresh arrays, so that a pass takes time in proportion to the samples.
            constexpr int MAX_SUBDIVISIONS = 12; // safety limit
            for (int pass = 0; pass < MAX_SUBDIVISIONS; ++pass) {
                bool subdivided = false;
                std::vector<float>& next_params = scratch.next_params;
                std::vector<Pixel>& next_pixels = scratch.next_pixels;
                next_params.clear();
                next_pixels.clear();
                next_params.push_back(params[0]);
                next_pixels.push_back(pixels[0]);
                for (std::size_t k = 1; k < params.size(); ++k) {
                    float dx = pixels[k].x - pixels[k - 1].x;
                    float dy = pixels[k].y - pixels[k - 1].y;
                    float dist = std::sqrt(dx * dx + dy * dy);

                    if (dist > max_pixel_step) {
                        float t_mid = (params[k - 1] + params[k]) / 2.0f;
                        Vec3<float> dir_mid = arc.evaluate(t_mid);
                        next_params.push_back(t_mid);
                        next_pixels.push_back(camera->project_to_sensor_(dir_mid));
                        subdivided = true;
                    }
                    next_params.push_back(params[k]);
                    next_pixels.push_back(pixels[k]);
                }
                if (!subdivided) {
                    break;
                }
                params.swap(next_params);
                pixels.swap(next_pixels);
            }

            // A source that stays put during the exposure is drawn once at its mean position,
            // each part with its share: its samples' light is added up.
            bool still = true;
            for (std::size_t k = 1; still && k < pixels.size(); ++k) {
                still = std::abs(pixels[k].x - pixels[0].x) < STILL_TOLERANCE &&
                        std::abs(pixels[k].y - pixels[0].y) < STILL_TOLERANCE;
            }
            TSpectral still_power{0.f};
            double still_x = 0.0;
            double still_y = 0.0;
            double still_weight = 0.0;

            // Otherwise its samples are the vertices of its path, drawn as lines from the tables
            // and its grid:
            const bool path = !still && (use_tables || has_core);
            if (path) {
                scratch.path.assign(params.size(), Vertex{});
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
                const Pixel& p = pixels[k];

                // A source whose light cannot reach the image is dropped here, before its
                // position is converted to pixel indices, which it may not fit: it can be far
                // beyond the image, or NaN, where the lens images nothing (past the pole of a
                // rational distortion model). The margin is beyond the furthest a grid
                // reaches, or, from the tables, the frame's diagonal.
                const float margin = use_tables ? diagonal + 1.f : static_cast<float>(reach + 1);
                if (!(p.x > -margin && p.x < static_cast<float>(fb_width) + margin &&
                      p.y > -margin && p.y < static_cast<float>(fb_height) + margin)) {
                    continue;
                }

                // Interpolate irradiance at this parameter value:
                TSpectral irrad = item.interpolate_irradiances(params[k]);
                Vec3<float> dir = arc.evaluate(params[k]);
                float projected_area = camera->projected_aperture_area(dir);
                // The light delivered per unit time, and over the sample's share of it:
                TSpectral rate = irrad * projected_area;

                if (test_occlusion) {
                    // The occluder mask covers the image; beyond it, always test.
                    const bool on_image = p.x >= 0.f && p.x < static_cast<float>(fb_width) &&
                                          p.y >= 0.f && p.y < static_cast<float>(fb_height);
                    const bool maybe_occluded =
                        !mask_available || !on_image ||
                        occluder_mask_(std::clamp(static_cast<int>(p.x), 0, fb_width - 1),
                                       std::clamp(static_cast<int>(p.y), 0, fb_height - 1)) != 0;
                    if (maybe_occluded) {
                        Ray<TSpectral> occlusion_ray(Vec3<float>{0.f, 0.f, 0.f},
                                                     glm::normalize(dir));

                        // params[k] is the arc parameter over the full exposure on [0, 1],
                        // which is the same parameterization the intersectors take as their
                        // motion-blur time, so the occlusion query for this sample sees the
                        // scene as it was at the instant this piece of the streak was laid
                        // down - not a union of the occluder over the whole exposure.
                        TSpectral transmittance =
                            scene_view.evaluate_transmittance(occlusion_ray,
                                                              item.interpolate_ranges(params[k]),
                                                              MediumStack<TSpectral>{},
                                                              sampler,
                                                              params[k],
                                                              AlphaMode::Expected);

                        if (transmittance.max() <= 0.f) {
                            continue;
                        }
                        rate = rate * transmittance;
                    }
                }
                const TSpectral power = weight * rate;

                if (still) {
                    still_power += power;
                    still_x += static_cast<double>(weight) * static_cast<double>(p.x);
                    still_y += static_cast<double>(weight) * static_cast<double>(p.y);
                    still_weight += static_cast<double>(weight);
                    continue;
                }
                if (path) {
                    scratch.path[k] = Vertex{static_cast<double>(p.x),
                                             static_cast<double>(p.y),
                                             static_cast<double>(params[k]),
                                             rate,
                                             true};
                }
            }
            if (path && use_tables) {
                emit_path(item_index, item.tables, nullptr, tables_share, scratch, batch);
            }
            if (path && has_core) {
                emit_path(item_index, nullptr, item.grid, core_share, scratch, batch);
            }
            if (still && still_weight > 0.0) {
                const Pixel mean{static_cast<float>(still_x / still_weight),
                                 static_cast<float>(still_y / still_weight)};
                if (use_tables) {
                    emit_sample(
                        item, item_index, mean, still_power * tables_share, Draw::Tables, batch);
                }
                if (has_core) {
                    emit_sample(
                        item, item_index, mean, still_power * core_share, Draw::Grid, batch);
                }
            }
        }
    };

    std::vector<Batch> batches;

    if (!star_field.empty()) {
        const std::size_t n_stars = star_field.size();
        const std::size_t n_samples = star_field.sample_count;

        constexpr std::size_t CHUNK = 4096;
        const std::size_t num_chunks = (n_stars + CHUNK - 1) / CHUNK;
        batches.resize(num_chunks);

        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(0, num_chunks),
            [&](const tbb::blocked_range<std::size_t>& range) {
                BinScratch scratch;
                scratch.directions.resize(n_samples);

                for (std::size_t c = range.begin(); c != range.end(); ++c) {
                    Batch& batch = batches[c];
                    // Seeded by chunk, so that occlusion tests do not depend on scheduling:
                    RandomSampler<float> sampler(static_cast<unsigned int>(c));

                    const std::size_t begin = c * CHUNK;
                    const std::size_t end = std::min(begin + CHUNK, n_stars);

                    for (std::size_t i = begin; i < end; ++i) {
                        const Vec3<float>* dirs = star_field.directions_for(i);
                        std::copy(dirs, dirs + n_samples, scratch.directions.begin());
                        scratch.arc.reset(scratch.directions);

                        // Cheap rejection before anything is allocated. This is the
                        // same frustum test the binning pass performs; running it here
                        // only decides whether a RenderItem is worth building.
                        if (wide_frustum.clip_arc(scratch.arc, scratch.clip).empty()) {
                            continue;
                        }

                        // A catalogue star's irradiance is constant over the exposure and
                        // its range is fixed at infinity, so both collapse to a single
                        // entry.
                        RenderItem<TSpectral> item(
                            scratch.arc,
                            std::vector<TSpectral>{star_field.irradiances[i]},
                            std::vector<float>{RenderItem<TSpectral>::RANGE_AT_INFINITY});
                        item.tables = tables;
                        item.grid = grid;

                        batch.items.push_back(std::move(item));
                        bin_item(
                            batch.items.back(), batch.items.size() - 1, scratch, sampler, batch);
                    }
                }
            });
    }

    // Unresolved objects (separate from stars), prepared above, are binned in parallel, one
    // batch each.
    if (!objects.empty()) {
        const std::size_t first_batch = batches.size();
        batches.resize(first_batch + objects.size());
        tbb::parallel_for(tbb::blocked_range<std::size_t>(0, objects.size()),
                          [&](const tbb::blocked_range<std::size_t>& range) {
                              BinScratch scratch;
                              for (std::size_t i = range.begin(); i != range.end(); ++i) {
                                  Batch& batch = batches[first_batch + i];
                                  RandomSampler<float> sampler(
                                      static_cast<unsigned int>(first_batch + i));
                                  batch.items.push_back(std::move(objects[i]));
                                  bin_item(batch.items.back(), 0, scratch, sampler, batch);
                              }
                          });
    }

    // Merge the batches in order, and sort their (tile, sample) pairs into one list per tile
    // (offsets into a single index array), keeping the order sources were binned in:
    std::vector<Sample> samples;
    std::vector<std::size_t> tile_offsets(static_cast<std::size_t>(num_tiles) + 1, 0);
    {
        std::size_t total_items = 0;
        std::size_t total_samples = 0;
        for (const Batch& batch : batches) {
            total_items += batch.items.size();
            total_samples += batch.samples.size();
            for (const auto& [tile, sample] : batch.tile_samples) {
                ++tile_offsets[tile + 1];
            }
        }
        items.reserve(total_items);
        samples.reserve(total_samples);
        for (std::size_t t = 0; t < static_cast<std::size_t>(num_tiles); ++t) {
            tile_offsets[t + 1] += tile_offsets[t];
        }
    }
    std::vector<std::uint32_t> tile_sample_indices(tile_offsets.back());
    {
        std::vector<std::size_t> fill(tile_offsets.begin(), tile_offsets.end() - 1);
        for (Batch& batch : batches) {
            const std::size_t item_base = items.size();
            const auto sample_base = static_cast<std::uint32_t>(samples.size());
            for (auto& item : batch.items) {
                items.push_back(std::move(item));
            }
            for (Sample& sample : batch.samples) {
                sample.item_idx += item_base;
                samples.push_back(sample);
            }
            for (const auto& [tile, sample] : batch.tile_samples) {
                tile_sample_indices[fill[tile]++] = sample_base + sample;
            }
            batch = Batch{}; // release as we go
        }
    }

    if (samples.empty()) {
        return received_power;
    }

    // Lines drawn from a PSF's grid are drawn from tables for their direction where enough of them
    // share it that drawing them directly would take longer than making the tables (as few as
    // fit in GridPsf::LINE_CACHE_BUDGET, most wanted first), and directly otherwise: a camera
    // that turns about its boresight sends each source its own way. The choice is the frame's
    // alone, not what earlier ones left made, so a frame comes out the same however it is drawn.
    {
        using Grid = GridPsf<TSpectral>;
        std::map<std::pair<const Grid*, int>, double> wanted;
        const auto direction_of = [](const Sample& sample) {
            return Grid::line_directions(sample.end[0] - sample.start[0],
                                         sample.end[1] - sample.start[1]);
        };
        for (const Sample& sample : samples) {
            if (!sample.line || !sample.from_grid) {
                continue;
            }
            const Grid* line_grid = items[sample.item_idx].grid;
            const auto directions = direction_of(sample);
            const double length =
                std::hypot(sample.end[0] - sample.start[0], sample.end[1] - sample.start[1]);
            const double work = line_grid->line_direct_work(length, sample.draw);
            wanted[{line_grid, directions.below}] += work;
            if (directions.fraction > 0.0) {
                wanted[{line_grid, directions.above}] += work;
            }
        }
        std::vector<std::pair<double, std::pair<const Grid*, int>>> worth;
        for (const auto& [key, work] : wanted) {
            if (work > key.first->line_table_work()) {
                worth.emplace_back(work, key);
            }
        }
        std::sort(worth.begin(), worth.end(), [](const auto& a, const auto& b) {
            return a.first > b.first || (a.first == b.first && a.second < b.second);
        });
        std::map<const Grid*, std::size_t> bytes;
        std::vector<std::pair<const Grid*, int>> made;
        for (const auto& [work, key] : worth) {
            std::size_t& used = bytes[key.first];
            const std::size_t more = key.first->line_table_bytes();
            if (used + more <= Grid::LINE_CACHE_BUDGET) {
                used += more;
                made.push_back(key);
            }
        }
        std::sort(made.begin(), made.end());
        tbb::parallel_for(std::size_t{0}, made.size(), [&](std::size_t k) {
            made[k].first->prepare_line(made[k].second);
        });
        const auto have = [&](const Grid* of, int direction) {
            return std::binary_search(made.begin(), made.end(), std::make_pair(of, direction));
        };
        for (Sample& sample : samples) {
            if (!sample.line || !sample.from_grid) {
                continue;
            }
            const Grid* line_grid = items[sample.item_idx].grid;
            const auto directions = direction_of(sample);
            sample.from_tables = have(line_grid, directions.below) &&
                                 (directions.fraction == 0.0 || have(line_grid, directions.above));
        }
    }

    // The light each source or line drawn from the tables puts in each tile, per unit power: one
    // entry per (tile, sample) pair, so that the tiles write their own.
    std::vector<std::array<double, TSpectral::size()>> entry_light(tile_sample_indices.size());

    // Render the tiles in parallel, each into its own pixels of the frame:
    tbb::parallel_for(
        tbb::blocked_range<int>(0, num_tiles), [&](const tbb::blocked_range<int>& range) {
            for (int tile_idx = range.begin(); tile_idx < range.end(); ++tile_idx) {
                const std::size_t first = tile_offsets[static_cast<std::size_t>(tile_idx)];
                const std::size_t last = tile_offsets[static_cast<std::size_t>(tile_idx) + 1];
                if (first == last) {
                    continue;
                }

                const int tile_x0 = (tile_idx % tiles_x) * TILE_SIZE;
                const int tile_y0 = (tile_idx / tiles_x) * TILE_SIZE;
                const int tile_x1 = std::min(fb_width, tile_x0 + TILE_SIZE);
                const int tile_y1 = std::min(fb_height, tile_y0 + TILE_SIZE);

                for (std::size_t k = first; k < last; ++k) {
                    const Sample& sample = samples[tile_sample_indices[k]];
                    const auto& item = items[sample.item_idx];
                    const Pixel& star_p = sample.projected;
                    const TSpectral& power = sample.power;

                    if (sample.line && sample.from_grid) {
                        // A piece of a moving source's path, drawn from its grid, its faint wings
                        // interpolated where that is off by no more than its share of half of
                        // tau:
                        const auto weight = [&](double r) { return taper_weight(r, sample.taper); };
                        typename GridPsf<TSpectral>::LineTolerance tolerance;
                        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                            tolerance.electrons[c] =
                                static_cast<double>(power[c]) * electrons_per_power[c];
                        }
                        tolerance.electrons_off =
                            0.5 * tau * static_cast<double>(sample.tolerance_share);
                        tolerance.per_channel = PER_CHANNEL_READOUT;
                        entry_light[k] = sample.from_tables
                                             ? item.grid->draw_line(received_power,
                                                                    tile_x0,
                                                                    tile_x1,
                                                                    tile_y0,
                                                                    tile_y1,
                                                                    sample.start,
                                                                    sample.end,
                                                                    power,
                                                                    sample.draw,
                                                                    weight,
                                                                    tolerance)
                                             : item.grid->draw_line_direct(received_power,
                                                                           tile_x0,
                                                                           tile_x1,
                                                                           tile_y0,
                                                                           tile_y1,
                                                                           sample.start,
                                                                           sample.end,
                                                                           power,
                                                                           sample.draw,
                                                                           weight,
                                                                           tolerance);
                    } else if (sample.line) {
                        // A piece of a moving source's path, drawn from the tables (which keep
                        // to the pixels within its reach), with the light drawn added up as for
                        // a still source, per unit power:
                        const std::array<double, TSpectral::size()> drawn = item.tables->draw_line(
                            received_power,
                            tile_x0,
                            tile_x1,
                            tile_y0,
                            tile_y1,
                            sample.start,
                            sample.end,
                            sample.density,
                            sample.reach,
                            [&](double r) { return taper_weight(r, sample.taper); });
                        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                            entry_light[k][c] =
                                power[c] > 0.f ? drawn[c] / static_cast<double>(power[c]) : 0.0;
                        }
                    } else {
                        // Drawn from the tables or a grid at its exact position, tapered, and
                        // the light drawn added up per (tile, sample), for the light left to
                        // spread:
                        const double px = static_cast<double>(star_p.x);
                        const double py = static_cast<double>(star_p.y);
                        const int x_begin =
                            std::max(tile_x0, static_cast<int>(std::floor(px - sample.draw)));
                        const int x_end =
                            std::min(tile_x1, static_cast<int>(std::floor(px + sample.draw)) + 1);
                        const int y_begin = std::max(
                            tile_y0, static_cast<int>(std::floor(py - sample.draw / aspect)));
                        const int y_end = std::min(
                            tile_y1, static_cast<int>(std::floor(py + sample.draw / aspect)) + 1);
                        const auto weight = [&](double r) { return taper_weight(r, sample.taper); };
                        entry_light[k] = sample.from_grid ? item.grid->draw(received_power,
                                                                            x_begin,
                                                                            x_end,
                                                                            y_begin,
                                                                            y_end,
                                                                            px,
                                                                            py,
                                                                            power,
                                                                            sample.scale,
                                                                            sample.draw,
                                                                            weight)
                                                          : item.tables->draw(received_power,
                                                                              x_begin,
                                                                              x_end,
                                                                              y_begin,
                                                                              y_end,
                                                                              px,
                                                                              py,
                                                                              power,
                                                                              sample.rings,
                                                                              sample.draw,
                                                                              weight);
                    }
                }
            }
        });

    // The light sources drawn from the tables or a grid put on the sensor beyond what was drawn,
    // spread evenly over it.
    {
        // The light each source drew, per unit power: its entries' sums, in a fixed order.
        std::vector<std::array<double, TSpectral::size()>> drawn(samples.size());
        for (std::size_t k = 0; k < tile_sample_indices.size(); ++k) {
            auto& sum = drawn[tile_sample_indices[k]];
            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                sum[c] += entry_light[k][c];
            }
        }

        // The frame, in pixel widths, and the radii at which a circle about a source meets its
        // edges and corners, where the length of the circle on the frame has kinks.
        const double width = static_cast<double>(fb_width);
        const double height = aspect * static_cast<double>(fb_height);
        using Light = std::array<double, TSpectral::size()>;

        // The light a line puts on the frame, as a fraction of its own: the light a still source
        // puts on it (PsfTables::light_in_rectangle()) averaged along the line, by
        // Gauss-Legendre quadrature. That changes sharply only where the line is
        // within a few pixels of an edge, so the line is split where its distance from each edge
        // is 0 or 2^k / 64 px, up to near_radius(), either side; on the panels within that of an
        // edge, four points are taken, and elsewhere two (or one for a panel much shorter), on
        // panels as long as the distance along the line to the nearest edge, over which the
        // light beyond the edges changes by about a factor of two.
        const auto line_on_frame = [&](const Sample& sample) {
            const PsfTables<TSpectral>* source_tables = items[sample.item_idx].tables;
            const GridPsf<TSpectral>* source_grid =
                sample.from_grid ? items[sample.item_idx].grid : nullptr;
            const std::array<double, TSpectral::size()> line_scale =
                source_grid != nullptr ? source_grid->line_scale()
                                       : std::array<double, TSpectral::size()>{};
            const double near_edge = source_grid != nullptr ? std::min(source_grid->reach(), 8.0)
                                                            : source_tables->near_radius();
            constexpr std::array<double, 4> GL4_NODES{
                -0.8611363115940526, -0.3399810435848563, 0.3399810435848563, 0.8611363115940526};
            constexpr std::array<double, 4> GL4_WEIGHTS{
                0.3478548451374538, 0.6521451548625461, 0.6521451548625461, 0.3478548451374538};
            constexpr std::array<double, 2> GL2_NODES{-0.5773502691896258, 0.5773502691896258};
            constexpr std::array<double, 2> GL2_WEIGHTS{1.0, 1.0};
            constexpr std::array<double, 1> GL1_NODES{0.0};
            constexpr std::array<double, 1> GL1_WEIGHTS{2.0};
            constexpr double FINEST = 1.0 / 64.0;
            const double ax = sample.start[0];
            const double ay = aspect * sample.start[1];
            const double bx = sample.end[0];
            const double by = aspect * sample.end[1];
            const double length = std::hypot(bx - ax, by - ay);
            Light total{};
            if (!(length > 0.0)) {
                return total;
            }
            const double tx = (bx - ax) / length;
            const double ty = (by - ay) / length;
            // Each edge's distance along the line, positive on the frame's side, and its rate of
            // change:
            const std::array<double, 4> side_start{ax, width - ax, ay, height - ay};
            const std::array<double, 4> side_rate{tx, -tx, ty, -ty};
            std::vector<double> cuts{0.0, length};
            const auto cut_at = [&](std::size_t i, double level) {
                const double u = (level - side_start[i]) / side_rate[i];
                if (u > 0.0 && u < length) {
                    cuts.push_back(u);
                }
            };
            for (std::size_t i = 0; i < 4; ++i) {
                if (side_rate[i] != 0.0) {
                    cut_at(i, 0.0);
                    for (double level = FINEST; level < near_edge; level *= 2.0) {
                        cut_at(i, level);
                        cut_at(i, -level);
                    }
                    cut_at(i, near_edge);
                    cut_at(i, -near_edge);
                }
            }
            std::sort(cuts.begin(), cuts.end());
            const auto add = [&](double u0, double u1, const auto& nodes, const auto& weights) {
                const double mid = 0.5 * (u0 + u1);
                const double half = 0.5 * (u1 - u0);
                for (std::size_t n = 0; n < nodes.size(); ++n) {
                    const double u = mid + half * nodes[n];
                    const double px = ax + u * tx;
                    const double py = ay + u * ty;
                    const Light light =
                        source_grid != nullptr
                            ? source_grid->light_on_frame(
                                  px, py / aspect, fb_width, fb_height, line_scale)
                            : source_tables->light_in_rectangle(-px, width - px, -py, height - py);
                    for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                        total[c] += half * weights[n] * light[c] / length;
                    }
                }
            };
            for (std::size_t k = 0; k + 1 < cuts.size(); ++k) {
                const double u0 = cuts[k];
                const double u1 = cuts[k + 1];
                if (!(u1 > u0)) {
                    continue;
                }
                const double mid = 0.5 * (u0 + u1);
                bool near = false;
                for (std::size_t i = 0; i < 4; ++i) {
                    near = near || std::abs(side_start[i] + mid * side_rate[i]) < near_edge;
                }
                if (near) {
                    add(u0, u1, GL4_NODES, GL4_WEIGHTS);
                    continue;
                }
                for (double u = u0; u < u1;) {
                    double scale = std::numeric_limits<double>::infinity();
                    for (std::size_t i = 0; i < 4; ++i) {
                        if (side_rate[i] != 0.0) {
                            scale = std::min(scale,
                                             std::abs(side_start[i] + u * side_rate[i]) /
                                                 std::abs(side_rate[i]));
                        }
                    }
                    const double next = std::min(u1, u + std::max(scale, near_edge));
                    // A panel a quarter of that or less is taken at its middle, within about
                    // 0.5% of the light beyond the edges.
                    if (next - u <= 0.25 * scale) {
                        add(u, next, GL1_NODES, GL1_WEIGHTS);
                    } else {
                        add(u, next, GL2_NODES, GL2_WEIGHTS);
                    }
                    u = next;
                }
            }
            return total;
        };

        std::vector<std::array<double, TSpectral::size()>> excess(samples.size());
        tbb::parallel_for(std::size_t{0}, samples.size(), [&](std::size_t i) {
            const Sample& sample = samples[i];
            if (std::isinf(sample.taper)) {
                return; // drawn over the whole frame: nothing left on it
            }
            if (sample.line) {
                // What falls on the frame, less what was drawn.
                const Light on_frame = line_on_frame(sample);
                for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                    const double power = static_cast<double>(sample.power[c]);
                    excess[i][c] = power > 0.0 ? on_frame[c] - drawn[i][c] : 0.0;
                }
                return;
            }
            if (sample.from_grid) {
                // What falls on the frame, less what was drawn:
                const Light on_frame = items[sample.item_idx].grid->light_on_frame(
                    static_cast<double>(sample.projected.x),
                    static_cast<double>(sample.projected.y),
                    fb_width,
                    fb_height,
                    sample.scale);
                for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                    excess[i][c] = on_frame[c] - drawn[i][c];
                }
                return;
            }
            const PsfTables<TSpectral>& source_tables = *items[sample.item_idx].tables;
            const double px = static_cast<double>(sample.projected.x);
            const double py = aspect * static_cast<double>(sample.projected.y);
            const double x0 = -px;
            const double x1 = width - px;
            const double y0 = -py;
            const double y1 = height - py;
            const double farthest = std::hypot(std::max(-x0, x1), std::max(-y0, y1));
            std::vector<double> breaks{std::abs(x0),
                                       x1,
                                       std::abs(y0),
                                       y1,
                                       std::hypot(x0, y0),
                                       std::hypot(x1, y0),
                                       std::hypot(x0, y1),
                                       std::hypot(x1, y1),
                                       sample.taper,
                                       2.0 * sample.taper};
            const auto on_frame = [&](double r) {
                return detail::arc_in_rectangle(x0, x1, y0, y1, r);
            };
            const double nearest_edge = std::min({px, width - px, py, height - py});
            std::array<double, TSpectral::size()>& left = excess[i];
            if (nearest_edge >= sample.draw) {
                // Drawn wholly on the frame: what was not drawn is what drawing left out,
                // exactly, less what falls off the frame, all of it beyond the nearest edge.
                std::array<double, TSpectral::size()> off =
                    source_tables.integrate(nearest_edge, farthest, breaks, [&](double r) {
                        return 2.0 * PI<double>() * r - on_frame(r);
                    });
                for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                    off[c] += source_tables.smooth_light_beyond(c, farthest);
                    left[c] = 1.0 - drawn[i][c] - off[c];
                }
            } else {
                // Crossing the frame's edge, or beyond it: the light left on the frame, from
                // the profile.
                left = source_tables.integrate(sample.taper, farthest, breaks, [&](double r) {
                    return (1.0 - taper_weight(r, sample.taper)) * on_frame(r);
                });
            }
        });

        std::array<double, TSpectral::size()> spread{};
        for (std::size_t i = 0; i < samples.size(); ++i) {
            for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                spread[c] += static_cast<double>(samples[i].power[c]) * excess[i][c];
            }
        }
        TSpectral per_pixel{0.f};
        bool any = false;
        const double pixels = static_cast<double>(fb_width) * static_cast<double>(fb_height);
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            per_pixel[c] = static_cast<float>(spread[c] / pixels);
            any = any || per_pixel[c] != 0.f;
        }
        if (any) {
            tbb::parallel_for(std::size_t{0}, received_power.size(), [&](std::size_t i) {
                received_power[i] += per_pixel;
            });
        }
    }

    auto end_clock = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_clock - start_clock;
    HUIRA_LOG_INFO("Unresolved point source rendering completed in " +
                   std::to_string(elapsed.count()) + " seconds");

    return received_power;
}

/**
 * @brief True when every channel of every pixel is exactly zero.
 *
 * @param image Image to test.
 */
template <IsSpectral TSpectral>
bool Renderer<TSpectral>::image_is_zero_(const Image<TSpectral>& image)
{
    if (image.size() == 0) {
        return true;
    }

    std::atomic<bool> nonzero{false};
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, image.size()),
                      [&](const tbb::blocked_range<std::size_t>& range) {
                          // Another block already found a non-zero pixel.
                          if (nonzero.load(std::memory_order_relaxed)) {
                              return;
                          }
                          for (std::size_t i = range.begin(); i != range.end(); ++i) {
                              for (std::size_t c = 0; c < TSpectral::size(); ++c) {
                                  if (image[i][c] != 0.0f) {
                                      nonzero.store(true, std::memory_order_relaxed);
                                      return;
                                  }
                              }
                          }
                      });

    return !nonzero.load(std::memory_order_relaxed);
}

} // namespace huira
