#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "embree4/rtcore.h"
#include "glm/glm.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/assets/lights/sphere_light.hpp"
#include "huira/core/physics.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/geometry/mesh.hpp"
#include "huira/handles/camera_handle.hpp"
#include "huira/render/shading_utils.hpp"
#include "huira/render/trajectory_arc.hpp"
#include "huira/scene/scene.hpp"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

namespace huira {
/**
 * @brief Construct a SceneView for a given scene, time, camera, and observation mode.
 *
 * Collects geometry, lights, unresolved objects, and stars for rendering.
 *
 * @param scene Scene to view
 * @param exposure_interval Exposure interval. Poses set by hand hold at its reference time.
 * @param camera_instance Camera instance handle
 * @param obs_mode Where each thing is seen: ABERRATED_STATE (the default), with light time and
 *        the aberration from the camera's velocity, as a camera really sees it;
 *        GEOMETRIC_STATE, with light time alone; or TRUE_STATE, where it is at the instant
 * @param num_temporal_samples The number of temporal samples every moving thing is given, evenly
 *        spaced through the exposure, or none (the default) to choose them for each thing:
 *        one for whatever does not move in view, and otherwise as many as keep the path it is
 *        drawn along within motion_tolerance pixels of its true path (see choose_fractions_()).
 *        One sample renders a still frame, at the middle of the exposure, as does turning the
 *        camera's motion blur off (see CameraModel::enable_motion_blur()), whatever number is
 *        given.
 * @param motion_tolerance How far, in pixels, the path a thing is drawn along may stray from its
 *        true path, when the samples are chosen. Positive.
 * @throws std::runtime_error if the interval ends before it starts, no temporal samples or more
 *         than Embree's 129 are asked for, or the tolerance is not positive and finite.
 */
template <IsSpectral TSpectral>
SceneView<TSpectral>::SceneView(const Scene<TSpectral>& scene,
                                const Interval& exposure_interval,
                                const InstanceHandle<TSpectral>& camera_instance,
                                ObservationMode obs_mode,
                                std::optional<std::size_t> num_temporal_samples,
                                double motion_tolerance)
    : exposure_interval_{exposure_interval}, requested_temporal_samples_{num_temporal_samples},
      motion_tolerance_{motion_tolerance}, obs_mode_{obs_mode}, device_{scene.device_}
{
    HUIRA_TRACE_SCOPE("SceneView::SceneView");
    HUIRA_LOG_INFO("Created over interval [" + std::to_string(exposure_interval.start.et()) +
                   ",  " + std::to_string(exposure_interval.end.et()) + "]");
    // Its fields can have been changed since it was made:
    exposure_interval_.check();

    if (num_temporal_samples && *num_temporal_samples < 1) {
        HUIRA_THROW_ERROR("SceneView - At least one temporal sample is needed");
    }
    if (num_temporal_samples && *num_temporal_samples > RTC_MAX_TIME_STEP_COUNT) {
        HUIRA_THROW_ERROR("SceneView - At most " + std::to_string(RTC_MAX_TIME_STEP_COUNT) +
                          " temporal samples can be used (Embree's limit): " +
                          std::to_string(*num_temporal_samples));
    }
    if (!(motion_tolerance > 0.0) || !std::isfinite(motion_tolerance)) {
        HUIRA_THROW_ERROR("SceneView - The motion tolerance must be positive and finite: " +
                          std::to_string(motion_tolerance) + " pixels");
    }

    // Get the camera model:
    auto camera_node = camera_instance.get();
    const auto& asset_var = camera_node->asset();
    if (!std::holds_alternative<CameraModel<TSpectral>*>(asset_var)) {
        HUIRA_THROW_ERROR(
            "SceneView received an Instance for the observer that does not contain a CameraModel!");
    }
    this->camera_model_ = std::get<CameraModel<TSpectral>*>(asset_var)->shared_from_this();
    camera_node_ = camera_node.get();

    // With the camera's motion blur off, a still frame:
    if (!camera_model_->motion_blur_enabled()) {
        if (requested_temporal_samples_ && *requested_temporal_samples_ > 1) {
            HUIRA_LOG_WARNING("SceneView - Motion blur is off for " + camera_model_->get_info() +
                              ": the " + std::to_string(*requested_temporal_samples_) +
                              " temporal samples asked for are not used, and a still frame is "
                              "rendered at the middle of the exposure");
        }
        requested_temporal_samples_ = 1;
    }

    // How many pixels a small angle spans, at most, anywhere in the frame: off the axis, a
    // rectilinear camera's pixels span smaller angles, by the square of the cosine of the angle
    // off axis, out to the frame's corners. (Lens distortion is not counted.)
    {
        const Mat3<float> k = camera_model_->intrinsic_matrix();
        const Resolution resolution = camera_model_->resolution();
        const double fx = static_cast<double>(k[0][0]);
        const double fy = static_cast<double>(k[1][1]);
        const double cx = static_cast<double>(k[2][0]);
        const double cy = static_cast<double>(k[2][1]);
        const double reach_x = std::max(cx, static_cast<double>(resolution.x) - cx) / fx;
        const double reach_y = std::max(cy, static_cast<double>(resolution.y) - cy) / fy;
        const double corner = std::atan(std::hypot(reach_x, reach_y));
        const double cos_corner = std::cos(corner);
        pixels_per_radian_ = std::max(fx, fy) / std::max(cos_corner * cos_corner, 0.1);
    }

    // The camera's pose at the middle of the exposure, which a still frame is seen from:
    camera_to_world_ = {static_cast<Transform<float>>(observer_at_(0.5).inverse)};

    // Copy the background radiance:
    background_ = scene.background_;

    // Collect geometry and lights by traversing the scene graph:
    traverse_and_collect_(scene.root_node_);
    HUIRA_LOG_INFO("SceneView collected " + std::to_string(primitives_.size()) +
                   " unique primitive batches and " + std::to_string(lights_.size()) +
                   " light instances.");
    if (has_motion_blur_) {
        HUIRA_LOG_INFO("SceneView: motion blur, with up to " +
                       std::to_string(max_temporal_samples_) + " temporal samples");
    }

    // Check for objects that were not collected. An object under a hidden node was
    // deliberately left out and is reported as such; anything else never reached the
    // scene graph at all, which is almost always a mistake.
    auto report_uncollected = [this](const std::string& label,
                                     std::uint64_t id,
                                     const std::string& name,
                                     const void* key) {
        const std::string subject = label + "[" + std::to_string(id) + "] '" + name + "' ";
        if (hidden_assets_.find(key) != hidden_assets_.end()) {
            HUIRA_LOG_INFO(subject + "is hidden and will not be rendered.");
        } else {
            HUIRA_LOG_WARNING(subject + "is unlinked in the scene graph and will not be rendered.");
        }
    };

    for (auto& primitive : scene.primitives_) {
        auto* key = primitive.get();
        if (batch_lookup_.find(key) == batch_lookup_.end()) {
            report_uncollected("Primitive", primitive->id(), primitive->name(), key);
        }
    }

    for (auto& light : scene.lights_) {
        bool found = false;
        for (const auto& instance : lights_) {
            if (instance.light->id() == light->id()) {
                found = true;
                break;
            }
        }
        if (!found) {
            report_uncollected("Light", light->id(), light->name(), light.get());
        }
    }

    for (auto& unresolved_object : scene.unresolved_objects_) {
        bool found = false;
        for (const auto& instance : unresolved_objects_) {
            if (instance.unresolved_object->id() == unresolved_object->id()) {
                found = true;
                break;
            }
        }
        if (!found) {
            report_uncollected("UnresolvedObject",
                               unresolved_object->id(),
                               unresolved_object->name(),
                               unresolved_object.get());
        }
    }

    // Copy stars in camera frame.
    //
    // This runs over the whole catalogue on every view, so it is written to touch the
    // heap a fixed number of times rather than once per star, and the observer
    // orientation is inverted once per temporal sample rather than once per star per
    // sample. The arithmetic per star is unchanged: aberrate in the SSB frame (when observing
    // in ABERRATED_STATE), rotate into camera coordinates, normalize in double precision, and
    // only then narrow to float - exactly what Star's constructor did.
    {
        const std::size_t n_stars = scene.stars_.size();

        // A star's catalog direction is where its light comes from as seen from the solar
        // system's barycenter, so its light time is already in it: TRUE_STATE and
        // GEOMETRIC_STATE leave it as it is, and ABERRATED_STATE aberrates it by the observer's
        // velocity, as it does everything else.
        const bool aberrate_stars = obs_mode_ == ObservationMode::ABERRATED_STATE;
        const auto apparent_direction = [aberrate_stars](const Vec3<double>& direction,
                                                         const Vec3<double>& observer_velocity) {
            return aberrate_stars ? compute_aberrated_direction(direction, observer_velocity)
                                  : direction;
        };

        // The stars all move together, as the camera turns (and as its velocity aberrates
        // them): one set of temporal samples serves them all. Their motion is followed by the
        // sky directions under the frame's center, corners and edge midpoints at mid-exposure.
        std::vector<double> fractions{0.5};
        if (n_stars > 0) {
            const Resolution resolution = camera_model_->resolution();
            const Rotation<double> middle = observer_at_(0.5).ssb.rotation;
            std::vector<Vec3<double>> sky;
            for (int j = 0; j <= 2; ++j) {
                for (int i = 0; i <= 2; ++i) {
                    const Pixel pixel{0.5f * static_cast<float>(i * resolution.x),
                                      0.5f * static_cast<float>(j * resolution.y)};
                    const Vec3<double> camera_direction =
                        glm::normalize(Vec3<double>{camera_model_->sensor_ray_(pixel).direction()});
                    sky.push_back(middle * camera_direction);
                }
            }
            std::map<double, std::vector<Vec3<double>>> star_probes;
            auto probes_at = [&](double fraction) -> const std::vector<Vec3<double>>& {
                auto [it, inserted] = star_probes.try_emplace(fraction);
                if (inserted) {
                    const Transform<double>& observer = observer_at_(fraction).ssb;
                    const Rotation<double> to_camera = observer.rotation.inverse();
                    for (const Vec3<double>& direction : sky) {
                        it->second.push_back(to_camera *
                                             apparent_direction(direction, observer.velocity));
                    }
                }
                return it->second;
            };
            auto turn_rate = [&]() {
                double rate = 0.0;
                for (double fraction : {0.0, 0.5, 1.0}) {
                    rate = std::max(rate, glm::length(observer_at_(fraction).ssb.angular_velocity));
                }
                return rate;
            };
            fractions = choose_fractions_(
                probes_at, turn_rate, DrawnHalfway{}, MAX_AUTOMATIC_STAR_SAMPLES, "The stars");
            max_temporal_samples_ = std::max(max_temporal_samples_, fractions.size());
        }
        const std::size_t n_samples = fractions.size();

        stars_.sample_count = n_samples;
        stars_.directions.resize(n_stars * n_samples);
        stars_.irradiances.resize(n_stars);

        // Hoisted: rotation.inverse() is a fixed property of each temporal sample.
        std::vector<Rotation<double>> inverse_rotations(n_samples);
        std::vector<Vec3<double>> observer_velocities(n_samples);
        for (std::size_t j = 0; j < n_samples; ++j) {
            const Transform<double>& observer = observer_at_(fractions[j]).ssb;
            inverse_rotations[j] = observer.rotation.inverse();
            observer_velocities[j] = observer.velocity;
        }

        tbb::parallel_for(tbb::blocked_range<std::size_t>(0, n_stars),
                          [&](const tbb::blocked_range<std::size_t>& r) {
                              for (std::size_t i = r.begin(); i != r.end(); ++i) {
                                  const Vec3<double> direction = scene.stars_[i].get_direction();
                                  stars_.irradiances[i] = scene.stars_[i].get_irradiance();

                                  Vec3<float>* out = stars_.directions.data() + i * n_samples;
                                  for (std::size_t j = 0; j < n_samples; ++j) {
                                      const Vec3<double> camera_direction =
                                          inverse_rotations[j] *
                                          apparent_direction(direction, observer_velocities[j]);
                                      out[j] = Vec3<float>(glm::normalize(camera_direction));
                                  }
                              }
                          });
    }

    compute_indirect_source_bounds_();

    build_tlas_();

    compute_occupancy_bounds_();

    // Resolve all unresolved objects now that the TLAS is built, so implementations
    // can cast occlusion/sampling rays through this view. Each object gets its own
    // deterministically seeded sampler for reproducible results.
    for (auto& unresolved_object : unresolved_objects_) {
        RandomSampler<float> sampler(
            static_cast<unsigned int>(unresolved_object.unresolved_object->id()));
        unresolved_object.unresolved_object->resolve_irradiance(
            unresolved_object.transforms, unresolved_object.times, *this, sampler);
    }
}

/**
 * @brief Destructor for SceneView, releases TLAS and clears geometry and lights.
 */
template <IsSpectral TSpectral>
SceneView<TSpectral>::~SceneView()
{
    HUIRA_TRACE_SCOPE("SceneView::~SceneView");
    if (tlas_) {
        rtcReleaseScene(tlas_);
    }
}

template <IsSpectral TSpectral>
struct RayContext : public RTCRayQueryContext {
    const SceneView<TSpectral>* scene_view;
};

/**
 * @brief Intersect a ray with the scene and return the hit record.
 * @param ray The ray to intersect.
 * @param time The time for motion blur.
 * @return The hit record.
 */
template <IsSpectral TSpectral>
HitRecord
SceneView<TSpectral>::intersect(const Ray<TSpectral>& ray, float time, unsigned int mask) const
{
    RTCRayHit rayhit{};
    rayhit.ray.org_x = ray.origin().x;
    rayhit.ray.org_y = ray.origin().y;
    rayhit.ray.org_z = ray.origin().z;
    rayhit.ray.dir_x = ray.direction().x;
    rayhit.ray.dir_y = ray.direction().y;
    rayhit.ray.dir_z = ray.direction().z;
    rayhit.ray.tnear = ray.tnear();
    rayhit.ray.tfar = std::numeric_limits<float>::infinity();
    rayhit.ray.time = time;
    rayhit.ray.mask = mask;
    rayhit.ray.flags = 0;
    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

    // Setup custom context
    RayContext<TSpectral> context;
    rtcInitRayQueryContext(&context); // Pass &context directly
    context.scene_view = this;

    // Wrap in Embree 4 arguments
    RTCIntersectArguments args;
    rtcInitIntersectArguments(&args);
    args.context = &context; // Pass &context directly

    // Pass the args into the trace call
    rtcIntersect1(tlas_, &rayhit, &args);

    HitRecord rec;
    if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        rec.t = rayhit.ray.tfar;
        rec.u = rayhit.hit.u;
        rec.v = rayhit.hit.v;
        rec.inst_id = rayhit.hit.instID[0];
        rec.geom_id = rayhit.hit.geomID;
        rec.prim_id = rayhit.hit.primID;
        rec.Ng = Vec3<float>{rayhit.hit.Ng_x, rayhit.hit.Ng_y, rayhit.hit.Ng_z};
        rec.time = time;
    }
    return rec;
}

/**
 * @brief Evaluate transmission along a ray
 * @param ray The ray to intersect.
 * @param time The time for motion blur.
 * @return The spectral transmission
 */
template <IsSpectral TSpectral>
TSpectral SceneView<TSpectral>::evaluate_transmittance(const Ray<TSpectral>& ray,
                                                       float t_far,
                                                       const MediumStack<TSpectral>& initial_stack,
                                                       RandomSampler<float>& sampler,
                                                       float time,
                                                       AlphaMode alpha_mode) const
{
    TSpectral transmittance{1.0f};
    MediumStack<TSpectral> stack = initial_stack;

    // March a SINGLE ray with an advancing tnear ("same-ray continuation")
    // instead of re-spawning at each crossed surface. The origin, direction,
    // and therefore the t parameterization stay bit-exact for the entire
    // march, so surfaces are always ordered by their true t and no
    // coordinate-space offset (and no scene-scale epsilon) is needed between
    // segments. hit.t is always the absolute distance from the original
    // origin, so it compares directly against t_far.
    Ray<TSpectral> occlusion_ray = ray;
    float t_start = ray.tnear();

    while (t_start < t_far) {
        HitRecord hit = this->intersect(occlusion_ray, time, MASK_GEOMETRY_);

        const bool surface_in_range = hit.hit() && hit.t < t_far;
        const float segment_end = surface_in_range ? hit.t : t_far;
        const float segment_length = segment_end - t_start;

        if (const Medium<TSpectral>* active = stack.top(); active != nullptr) {
            // Media only need an approximate segment start point; exactness
            // matters solely for intersection, which keeps the fixed origin.
            Ray<TSpectral> march_ray(occlusion_ray.at(t_start), occlusion_ray.direction());
            transmittance *= active->evaluate_transmittance(march_ray, segment_length, sampler);
            if (transmittance.max() <= 0.0f) {
                transmittance = TSpectral{0.0f};
                return transmittance;
            }
        }

        if (!surface_in_range) {
            break;
        }

        Interaction<TSpectral> isect = this->resolve_hit(occlusion_ray, hit);
        const auto& mapping = instance_mappings_[hit.inst_id];
        const auto& batch = primitives_[mapping.batch_index];
        const auto* material = batch.primitive->material.get();

        auto [params, shading_isect] = material->evaluate(isect);

        if (alpha_mode == AlphaMode::Stochastic) {
            const bool stochastic_pass_through =
                (params.opacity < 1.0f) && (sampler.get_1d() > params.opacity);

            if (!stochastic_pass_through) {
                TSpectral surface_transmission = params.transmission;
                if (surface_transmission.max() <= 0.0f) {
                    transmittance = TSpectral{0.0f};
                    return transmittance;
                }
                transmittance *= surface_transmission;
            }
        } else {
            // Expected value of the branch above, taken analytically rather than by
            // Russian roulette:
            //     E[T] = (1 - alpha) * 1 + alpha * transmission
            // Identical in the alpha == 1 and alpha == 0 limits, and noise-free in
            // between. Draws no random numbers, so the RNG stream is untouched.
            const float alpha = std::clamp(params.opacity, 0.0f, 1.0f);
            TSpectral surface_factor = TSpectral{1.0f - alpha} + params.transmission * alpha;
            if (surface_factor.max() <= 0.0f) {
                transmittance = TSpectral{0.0f};
                return transmittance;
            }
            transmittance *= surface_factor;
        }

        stack.toggle(batch.primitive.get());

        // Continue past this surface along the same parameterization:
        t_start = hit.t;
        occlusion_ray =
            Ray<TSpectral>(occlusion_ray.origin(), occlusion_ray.direction(), advance_ray_t(hit.t));
    }

    return transmittance;
}

/**
 * @brief Resolve a hit record into a full interaction, including position, normals, UVs, etc.
 * @param ray The ray that caused the hit.
 * @param hit The hit record to resolve.
 * @return The resolved interaction.
 */
template <IsSpectral TSpectral>
Interaction<TSpectral> SceneView<TSpectral>::resolve_hit(const Ray<TSpectral>& ray,
                                                         const HitRecord& hit) const
{
    Interaction<TSpectral> isect{};

    // Look up which mesh and instance this hit corresponds to:
    const auto& mapping = instance_mappings_[hit.inst_id];
    const auto& batch = primitives_[mapping.batch_index];

    // Get the hit:
    batch.primitive->geometry->compute_surface_interaction(hit, isect);
    isect.wo = -ray.direction();
    // The instance as Embree placed it when the ray hit it, at the ray's time in the exposure:
    // with motion blur, it moves and turns between temporal samples.
    const Transform<float> xf =
        instance_transform_at(batch.instances[mapping.instance_index], hit.time);

    // Position error accumulates in the primitive's LOCAL frame (surface
    // reconstruction / barycentric interpolation, rotation) and in the WORLD
    // frame (translation rounding), so the bound must scale with whichever
    // coordinate magnitude is larger. Note the local magnitude can dominate:
    // e.g. a camera in low orbit has small camera-relative coordinates but
    // planet-radius local coordinates.
    const Vec3<float> p_local = isect.position;
    isect.position = xf.apply_to_point(isect.position);

    const float local_mag =
        std::max({std::abs(p_local.x), std::abs(p_local.y), std::abs(p_local.z)});
    const float world_mag = std::max(
        {std::abs(isect.position.x), std::abs(isect.position.y), std::abs(isect.position.z)});
    isect.p_err = SPAWN_POINT_ERROR_SCALE * std::max(local_mag, world_mag);
    isect.normal_g = glm::normalize(xf.apply_to_direction(hit.Ng));
    isect.normal_g =
        glm::dot(ray.direction(), isect.normal_g) < 0.0f ? isect.normal_g : -isect.normal_g;
    isect.normal_s = glm::normalize(
        xf.apply_to_direction(isect.normal_s)); // TODO Do we need to invert this as well?
    if (glm::dot(isect.normal_s, isect.normal_g) < 0.0f) {
        isect.normal_s = -isect.normal_s;
    }

    isect.tangent = glm::normalize(xf.apply_to_direction(isect.tangent));
    isect.bitangent = glm::normalize(xf.apply_to_direction(isect.bitangent));
    if (glm::dot(glm::cross(isect.tangent, isect.bitangent), isect.normal_s) < 0.0f) {
        isect.bitangent = -isect.bitangent;
    }

    return isect;
}

/**
 * @brief Returns the indirect-source index of the instance a hit landed on.
 * @param hit The hit record to classify.
 * @return Index into indirect_sources(), or NO_INDIRECT_SOURCE if the hit is
 *         invalid or not on a designated indirect source.
 */
template <IsSpectral TSpectral>
[[nodiscard]] std::size_t SceneView<TSpectral>::indirect_source_index(const HitRecord& hit) const
{
    if (!hit.hit()) {
        return NO_INDIRECT_SOURCE;
    }
    return instance_mappings_[hit.inst_id].indirect_index;
}

/**
 * @brief Returns the IDs of the scene objects a hit landed on.
 * @param hit The hit record to identify.
 * @return The geometry, model, instance and part hit (see ObjectIds), or NO_OBJECT_ID for
 *         each if the hit is invalid or on a light.
 */
template <IsSpectral TSpectral>
ObjectIds SceneView<TSpectral>::object_ids(const HitRecord& hit) const
{
    if (!hit.hit() || hit.inst_id >= instance_mappings_.size()) {
        return ObjectIds{};
    }
    const auto& mapping = instance_mappings_[hit.inst_id];
    if (mapping.type != GeometryType::Primitive) {
        return ObjectIds{};
    }
    return primitives_[mapping.batch_index].ids[mapping.instance_index];
}

/**
 * @brief Intersect a batch of rays with the scene and return their hit records.
 * @param rays The rays to intersect.
 * @param time The time for motion blur.
 * @return A vector of hit records corresponding to each ray.
 */
template <IsSpectral TSpectral>
std::vector<HitRecord> SceneView<TSpectral>::intersect(const std::vector<Ray<TSpectral>>& rays,
                                                       float time) const
{
    std::vector<HitRecord> hits(rays.size());
    tbb::parallel_for(tbb::blocked_range<size_t>(0, rays.size()),
                      [&](const tbb::blocked_range<size_t>& range) {
                          for (size_t i = range.begin(); i < range.end(); ++i) {
                              hits[i] = intersect(rays[i], time);
                          }
                      });
    return hits;
}

/**
 * @brief Resolve a batch of hit records into full interactions.
 * @param rays The rays that caused the hits.
 * @param hits The hit records to resolve.
 * @return A vector of resolved interactions.
 */
template <IsSpectral TSpectral>
std::vector<Interaction<TSpectral>>
SceneView<TSpectral>::resolve_hits(const std::vector<Ray<TSpectral>>& rays,
                                   const std::vector<HitRecord>& hits) const
{
    std::vector<Interaction<TSpectral>> interactions(hits.size());
    tbb::parallel_for(tbb::blocked_range<size_t>(0, hits.size()),
                      [&](const tbb::blocked_range<size_t>& range) {
                          for (size_t i = range.begin(); i < range.end(); ++i) {
                              if (hits[i].hit()) {
                                  interactions[i] = resolve_hit(rays[i], hits[i]);
                              }
                          }
                      });
    return interactions;
}

/**
 * @brief Traverse the scene graph and collect assets.
 *
 * Recursively visits nodes and collects mesh, light, unresolved, and model instances, each with
 * its transforms in the camera's frame at its own temporal samples (see sample_instance_()).
 *
 * @param node Node to traverse
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::traverse_and_collect_(const std::shared_ptr<Node<TSpectral>>& node)
{
    // Hidden nodes and their descendants are not collected. Their assets are still
    // recorded, so the unlinked check can tell hidden apart from unattached.
    if (!node->is_visible()) {
        collect_hidden_(node);
        return;
    }

    if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
        const auto& asset_var = instance->asset();

        // A camera in the scene is not rendered, so it needs no samples:
        std::vector<Transform<float>> render_transforms;
        if (!std::holds_alternative<CameraModel<TSpectral>*>(asset_var)) {
            render_transforms = sample_instance_(*instance);
        }

        // Indirect-source designation brackets the asset dispatch (the asset is
        // validated at set-time to be a Primitive or a Model). Everything the
        // dispatch collects in between becomes a member of one source: the lone
        // primitive instance, or every primitive instance in a model's sub-graph.
        const bool designated = instance->is_indirect_source();
        if (designated) {
            begin_indirect_source_(render_transforms);
        }

        // Everything the dispatch collects was placed by this instance. Until a model's
        // graph says otherwise, it is also the part: a primitive placed directly.
        const ObjectIds outer_ids = open_ids_;
        open_ids_ = ObjectIds{};
        open_ids_.instance = instance->id();
        open_ids_.part = instance->id();

        std::visit([&](auto* raw_ptr) noexcept { handle_asset_ptr_(raw_ptr, render_transforms); },
                   asset_var);

        open_ids_ = outer_ids;

        if (designated) {
            end_indirect_source_(*instance);
        }
    }

    for (const auto& child : node->get_children()) {
        traverse_and_collect_(child);
    }
}

/**
 * @brief The camera's pose at a time in the exposure, evaluated once and kept.
 *
 * Poses set by hand hold at the interval's reference time, whatever the number of temporal
 * samples, so a still frame (one sample, at the middle of the exposure) shows things where a
 * motion-blurred one does, on average.
 *
 * @param fraction The time, as a fraction of the exposure from 0 to 1.
 */
template <IsSpectral TSpectral>
const typename SceneView<TSpectral>::ObserverPose&
SceneView<TSpectral>::observer_at_(double fraction)
{
    auto [it, inserted] = observer_poses_.try_emplace(fraction);
    if (inserted) {
        Transform<double> ssb = camera_node_->get_ssb_transform_(
            PoseContext{time_at_(fraction), exposure_interval_.reference, obs_mode_});
        ssb.rotation = ssb.rotation * camera_model_->sensor_orientation();
        it->second.ssb = ssb;
        it->second.inverse = ssb.inverse();
    }
    return it->second;
}

/// The time a fraction of the way through the exposure.
template <IsSpectral TSpectral>
Time SceneView<TSpectral>::time_at_(double fraction) const
{
    return exposure_interval_.start +
           units::Second(fraction * exposure_interval_.duration().value());
}

/// Evenly spaced fractions of the exposure: its middle for one, and otherwise from its start to
/// its end.
template <IsSpectral TSpectral>
std::vector<double> SceneView<TSpectral>::uniform_fractions_(std::size_t count)
{
    if (count <= 1) {
        return {0.5};
    }
    std::vector<double> fractions(count);
    for (std::size_t i = 0; i < count; ++i) {
        fractions[i] = static_cast<double>(i) / static_cast<double>(count - 1);
    }
    return fractions;
}

/**
 * @brief Choose the temporal samples for one thing in view.
 *
 * Between its samples the renderer draws a thing by interpolating its poses (geometry and
 * lights, which Embree moves along straight lines and turns along the shorter arc: see
 * drawn_halfway) or along a curve fitted through them (point sources: see TrajectoryArc). The
 * samples are evenly spaced, and as few as keep it within the motion tolerance of where it
 * truly is, as seen from the camera:
 *
 * - A thing whose probes (points standing for its motion; see local_probes_()) move less than
 *   the tolerance during the exposure, and that does not turn by more, is still: one sample, at
 *   the middle of the exposure.
 * - Otherwise, the samples start at the exposure's start and end, and as many more as keep it
 *   from turning more than 45 degrees between two, so that a turn is not mistaken for none (a
 *   full turn between two samples looks like no turn at all).
 * - Then, as long as it is drawn further than the tolerance from where it truly is at any point
 *   halfway between two samples, the samples are doubled: the halfway points, already
 *   evaluated, become samples. So choosing costs about two evaluations per sample chosen (the
 *   last halfway points checked are not kept). Something that only spins about its origin is
 *   drawn exactly, and takes only the samples that keep its turns under 45 degrees.
 *
 * A light is also followed in its distance, within distance_tolerance relative to it, since
 * its light falls off with it: a straight line between two samples passes nearer the camera than
 * a light swinging round it (as everything does when the camera turns), brightening it.
 *
 * The number of samples is capped, with a warning: something that moves in a way samples
 * cannot follow within the tolerance is drawn as near as the cap allows.
 *
 * @param probes_at The probes' positions in the camera's frame at a fraction of the exposure,
 *        each evaluated once (the reference returned must stay valid).
 * @param turn_rate How fast it turns, in rad/s, relative to the camera: asked for only when the
 *        samples are chosen.
 * @param drawn_halfway Where the renderer draws its probes halfway between two samples, or
 *        empty for a curve fitted through them.
 * @param max_samples The most samples it may be given.
 * @param what What it is, for the warning.
 * @param distance_tolerance How closely its probes' distances must be followed, relative to them
 *        (infinite, the default, for not at all).
 * @return The fractions of the exposure to sample it at.
 */
template <IsSpectral TSpectral>
std::vector<double> SceneView<TSpectral>::choose_fractions_(
    const std::function<const std::vector<Vec3<double>>&(double)>& probes_at,
    const std::function<double()>& turn_rate,
    const DrawnHalfway& drawn_halfway,
    std::size_t max_samples,
    const std::string& what,
    double distance_tolerance) const
{
    if (requested_temporal_samples_) {
        return uniform_fractions_(*requested_temporal_samples_);
    }

    // The tolerance as an angle. A direction in single precision, as the renderer holds them,
    // is good to about 1e-7 rad: no number of samples does better.
    constexpr double FLOAT_DIRECTION_PRECISION = 2.4e-7;
    const double tolerance =
        std::max(motion_tolerance_ / pixels_per_radian_, FLOAT_DIRECTION_PRECISION);
    const double duration = exposure_interval_.duration().value();

    // The largest angle, as seen from the camera, between two sets of the same probes, and the
    // largest difference in their distances, relative to the first's:
    auto largest_angle = [](const std::vector<Vec3<double>>& a,
                            const std::vector<Vec3<double>>& b) {
        double largest = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i) {
            largest = std::max(largest, angle_between_(a[i], b[i]));
        }
        return largest;
    };
    auto largest_distance_change = [](const std::vector<Vec3<double>>& a,
                                      const std::vector<Vec3<double>>& b) {
        double largest = 0.0;
        for (std::size_t i = 0; i < a.size(); ++i) {
            const double distance = glm::length(a[i]);
            if (distance > 0.0) {
                largest = std::max(largest, std::abs(glm::length(b[i]) - distance) / distance);
            }
        }
        return largest;
    };

    // Still, if nothing moves or turns by the tolerance. The turn is checked by its rate, times
    // how far the probes reach from the first, which stands at the center, as seen from the
    // camera. Something that does not move at all (the same at the exposure's start, middle and
    // end, and not turning) is still at once; otherwise the probes are also compared at its
    // quarters.
    const std::vector<Vec3<double>>& middle = probes_at(0.5);
    if (middle.empty()) {
        return {0.5};
    }
    double reach = 0.0;
    const double center_distance = glm::length(middle[0]);
    for (const Vec3<double>& probe : middle) {
        reach = std::max(reach, glm::length(probe - middle[0]));
    }
    const double turn = turn_rate() * duration;
    if (turn == 0.0 && probes_at(0.0) == middle && probes_at(1.0) == middle) {
        return {0.5};
    }
    double moved = 0.0;
    double approached = 0.0;
    for (double fraction : {0.0, 0.25, 0.75, 1.0}) {
        moved = std::max(moved, largest_angle(probes_at(fraction), middle));
        approached = std::max(approached, largest_distance_change(middle, probes_at(fraction)));
    }
    const double turn_seen = center_distance > 0.0 ? turn * reach / center_distance : turn;
    if (moved <= tolerance && turn_seen <= tolerance && approached <= distance_tolerance) {
        return {0.5};
    }

    // Enough samples that it turns no more than 45 degrees between two:
    std::size_t intervals = 1;
    while (static_cast<double>(intervals) * (0.25 * PI<double>()) < turn &&
           2 * intervals + 1 <= max_samples) {
        intervals *= 2;
    }

    while (true) {
        const double step = 1.0 / static_cast<double>(intervals);
        double error = 0.0;
        double distance_error = 0.0;
        if (drawn_halfway) {
            for (std::size_t j = 0; j < intervals; ++j) {
                const double a = static_cast<double>(j) * step;
                const std::vector<Vec3<double>>& halfway = probes_at(a + 0.5 * step);
                const std::vector<Vec3<double>> drawn = drawn_halfway(a, a + 0.5 * step, a + step);
                error = std::max(error, largest_angle(halfway, drawn));
                distance_error = std::max(distance_error, largest_distance_change(halfway, drawn));
            }
        } else {
            const std::size_t n_probes = middle.size();
            for (std::size_t i = 0; i < n_probes; ++i) {
                std::vector<Vec3<float>> directions(intervals + 1);
                for (std::size_t j = 0; j <= intervals; ++j) {
                    directions[j] =
                        Vec3<float>{glm::normalize(probes_at(static_cast<double>(j) * step)[i])};
                }
                const TrajectoryArc arc(directions);
                for (std::size_t j = 0; j < intervals; ++j) {
                    const double halfway = (static_cast<double>(j) + 0.5) * step;
                    const Vec3<double> drawn{arc.evaluate(static_cast<float>(halfway))};
                    error = std::max(error, angle_between_(probes_at(halfway)[i], drawn));
                }
            }
        }

        if (error <= tolerance && distance_error <= distance_tolerance) {
            break;
        }
        if (2 * intervals + 1 > max_samples) {
            HUIRA_LOG_WARNING(what + " moves in a way that " + std::to_string(intervals + 1) +
                              " temporal samples, the most it is given, follow only to within " +
                              std::to_string(error * pixels_per_radian_) + " pixels and " +
                              std::to_string(100.0 * distance_error) +
                              "% in distance. Give the SceneView a number of temporal samples, "
                              "or a larger motion tolerance.");
            break;
        }
        intervals *= 2;
    }
    return uniform_fractions_(intervals + 1);
}

/**
 * @brief Points whose motion stands for an instance's asset's.
 *
 * The renderer moves each of the asset's parts on its own (a model's, placed by its graph, or
 * the asset itself), interpolating the part's pose about its origin, so the points are kept by
 * part, in the part's frame: for geometry, the corners of its bounding box, where it is drawn
 * furthest from where it is (the gap is affine in a point of the part, so largest at a corner),
 * and for a light or a point source, its origin. The center of all of them stands for how far
 * they reach.
 */
template <IsSpectral TSpectral>
typename SceneView<TSpectral>::LocalProbes
SceneView<TSpectral>::local_probes_(const Instance<TSpectral>& instance) const
{
    LocalProbes probes;
    auto add_primitive = [&](const Primitive<TSpectral>& primitive,
                             const Transform<double>& placement) {
        RTCScene blas = primitive.geometry->blas();
        if (blas == nullptr) {
            return;
        }
        RTCBounds bounds{};
        rtcGetSceneBounds(blas, &bounds);
        if (!(bounds.upper_x >= bounds.lower_x) || !(bounds.upper_y >= bounds.lower_y) ||
            !(bounds.upper_z >= bounds.lower_z)) {
            return;
        }
        typename LocalProbes::Part part{placement, {}};
        for (std::size_t corner = 0; corner < 8; ++corner) {
            part.points.push_back(Vec3<double>{(corner & 1) ? bounds.upper_x : bounds.lower_x,
                                               (corner & 2) ? bounds.upper_y : bounds.lower_y,
                                               (corner & 4) ? bounds.upper_z : bounds.lower_z});
        }
        probes.parts.push_back(std::move(part));
    };

    // An asset placed in the instance's frame, and a model's parts, placed by its graph (which
    // does not move within the model):
    std::function<void(const std::shared_ptr<Node<TSpectral>>&, const Transform<double>&)>
        add_model;
    auto add_asset = [&](const Instantiable<TSpectral>& asset, const Transform<double>& placement) {
        std::visit(
            [&](auto* raw_ptr) {
                using T = std::remove_pointer_t<decltype(raw_ptr)>;
                if (raw_ptr == nullptr) {
                    return;
                }
                if constexpr (std::is_same_v<T, Primitive<TSpectral>>) {
                    add_primitive(*raw_ptr, placement);
                } else if constexpr (std::is_same_v<T, Model<TSpectral>>) {
                    add_model(raw_ptr->root_node_, placement);
                } else if constexpr (std::is_same_v<T, Light<TSpectral>> ||
                                     std::is_same_v<T, UnresolvedObject<TSpectral>>) {
                    probes.parts.push_back({placement, {Vec3<double>{0.0}}});
                }
            },
            asset);
    };
    add_model = [&](const std::shared_ptr<Node<TSpectral>>& node, const Transform<double>& parent) {
        const Transform<double> placement = parent * node->local_transform_;
        if (auto part = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
            add_asset(part->asset(), placement);
        }
        for (const auto& child : node->get_children()) {
            add_model(child, placement);
        }
    };
    add_asset(instance.asset(), Transform<double>{});

    // Nothing to draw (an empty model, say): its origin stands for it.
    if (probes.parts.empty()) {
        probes.parts.push_back({Transform<double>{}, {Vec3<double>{0.0}}});
    }

    Vec3<double> lower{std::numeric_limits<double>::infinity()};
    Vec3<double> upper{-std::numeric_limits<double>::infinity()};
    for (const typename LocalProbes::Part& part : probes.parts) {
        for (const Vec3<double>& point : part.points) {
            const Vec3<double> placed = part.placement.apply_to_point(point);
            lower = glm::min(lower, placed);
            upper = glm::max(upper, placed);
        }
    }
    if (std::isfinite(glm::dot(upper - lower, upper - lower))) {
        probes.center = 0.5 * (lower + upper);
    }
    return probes;
}

/**
 * @brief Where Embree draws something halfway between two of its poses.
 *
 * Its position and scale halfway along straight lines, and its rotation halfway along the
 * shorter arc between the two, as instance_transform_at() emulates for shading.
 */
template <IsSpectral TSpectral>
Transform<double> SceneView<TSpectral>::halfway_pose_(const Transform<double>& start,
                                                      const Transform<double>& end)
{
    Transform<double> pose;
    pose.position = 0.5 * (start.position + end.position);
    pose.scale = 0.5 * (start.scale + end.scale);
    // glm::slerp takes the shorter arc, as Embree does:
    pose.rotation = Rotation<double>::from_local_to_parent(
        glm::slerp(start.rotation.local_to_parent_quaternion(),
                   end.rotation.local_to_parent_quaternion(),
                   0.5));
    return pose;
}

/**
 * @brief An instance's transforms in the camera's frame, at its temporal samples.
 *
 * Each pose is evaluated once, at a time on a grid that halves (see choose_fractions_()), and
 * kept, so that choosing the samples costs about one evaluation per sample chosen.
 */
template <IsSpectral TSpectral>
std::vector<Transform<float>>
SceneView<TSpectral>::sample_instance_(const Instance<TSpectral>& instance)
{
    const LocalProbes local = local_probes_(instance);
    const bool point_source =
        std::holds_alternative<UnresolvedObject<TSpectral>*>(instance.asset());

    // The probes in the instance's frame: the center, then each part's points.
    std::vector<Vec3<double>> local_points{local.center};
    for (const typename LocalProbes::Part& part : local.parts) {
        for (const Vec3<double>& point : part.points) {
            local_points.push_back(part.placement.apply_to_point(point));
        }
    }

    struct Sample {
        Transform<double> local;          // In the camera's frame.
        std::vector<Vec3<double>> probes; // In the camera's frame.
        double turn_rate = 0.0;           // Relative to the camera, rad/s, at most.
    };
    std::map<double, Sample> samples;
    auto sample_at = [&](double fraction) -> const Sample& {
        auto [it, inserted] = samples.try_emplace(fraction);
        if (inserted) {
            const ObserverPose& observer = observer_at_(fraction);
            const Transform<double> ssb = instance.get_apparent_transform(
                obs_mode_, exposure_interval_.reference, time_at_(fraction), observer.ssb);
            Sample& sample = it->second;
            sample.local = observer.inverse * ssb;
            for (const Vec3<double>& point : local_points) {
                sample.probes.push_back(sample.local.apply_to_point(point));
            }
            // How fast it turns as seen from the camera: its turning relative to the camera's
            // (both rates are about the SSB frame's axes), so that something turning with the
            // camera is as still as it looks; and for a point, how fast its direction swings as
            // it passes the camera.
            sample.turn_rate =
                glm::length(ssb.angular_velocity - observer.ssb.angular_velocity) +
                (point_source ? glm::length(ssb.velocity - observer.ssb.velocity) /
                                    std::max(glm::length(ssb.position - observer.ssb.position),
                                             std::numeric_limits<double>::min())
                              : 0.0);
        }
        return it->second;
    };

    // Embree draws each part halfway between two samples where its pose, interpolated, puts it.
    // The center only stands for how far the probes reach, and is taken as drawn where it is:
    // within the bounds, a part is drawn furthest from where it is at a corner.
    DrawnHalfway drawn_halfway;
    if (!point_source) {
        drawn_halfway = [&](double start, double halfway, double end) {
            const Transform<double>& start_pose = sample_at(start).local;
            const Transform<double>& end_pose = sample_at(end).local;
            std::vector<Vec3<double>> drawn{sample_at(halfway).probes[0]};
            drawn.reserve(local_points.size());
            for (const typename LocalProbes::Part& part : local.parts) {
                const Transform<double> pose =
                    halfway_pose_(start_pose * part.placement, end_pose * part.placement);
                for (const Vec3<double>& point : part.points) {
                    drawn.push_back(pose.apply_to_point(point));
                }
            }
            return drawn;
        };
    }

    const std::vector<double> fractions = choose_fractions_(
        [&](double fraction) -> const std::vector<Vec3<double>>& {
            return sample_at(fraction).probes;
        },
        [&]() {
            double rate = 0.0;
            for (double fraction : {0.0, 0.5, 1.0}) {
                rate = std::max(rate, sample_at(fraction).turn_rate);
            }
            return rate;
        },
        drawn_halfway,
        MAX_AUTOMATIC_SAMPLES,
        instance.get_info(),
        std::holds_alternative<Light<TSpectral>*>(instance.asset())
            ? LIGHT_DISTANCE_TOLERANCE
            : std::numeric_limits<double>::infinity());

    std::vector<Transform<float>> transforms;
    transforms.reserve(fractions.size());
    for (double fraction : fractions) {
        // Down-cast to single precision once in the camera's frame:
        transforms.push_back(static_cast<Transform<float>>(sample_at(fraction).local));
    }
    max_temporal_samples_ = std::max(max_temporal_samples_, transforms.size());
    return transforms;
}

/**
 * @brief Record every asset under a hidden node without collecting it.
 *
 * Recurses through a hidden sub-graph, and through the internal graph of any Model it
 * finds, noting each asset it reaches. The constructor consults this to report hidden
 * assets separately from ones that were never attached to the scene graph.
 *
 * @param node Root of the hidden sub-graph
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::collect_hidden_(const std::shared_ptr<Node<TSpectral>>& node)
{
    if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
        std::visit(
            [&](auto* raw_ptr) {
                if constexpr (std::is_same_v<decltype(raw_ptr), Model<TSpectral>*>) {
                    if (raw_ptr) {
                        collect_hidden_(raw_ptr->root_node_);
                    }
                } else {
                    hidden_assets_.insert(static_cast<const void*>(raw_ptr));
                }
            },
            instance->asset());
    }

    for (const auto& child : node->get_children()) {
        collect_hidden_(child);
    }
}

/**
 * @brief Handle primitive asset pointer and add to geometry batch.
 * @param primitive Primitive pointer
 * @param xf Render transform
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::handle_asset_ptr_(
    Primitive<TSpectral>* primitive,
    const std::vector<Transform<float>>& instance_apparent_transforms)
{
    add_primitive_instance_(primitive->shared_from_this(), instance_apparent_transforms);
}

/**
 * @brief Handle light asset pointer and add to lights vector.
 * @param light Light pointer
 * @param xf Render transform
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::handle_asset_ptr_(
    Light<TSpectral>* light, const std::vector<Transform<float>>& instance_apparent_transforms)
{
    add_light_instance_(light->shared_from_this(), instance_apparent_transforms);
}

/**
 * @brief Handle camera model asset pointer (no-op).
 * @param camera CameraModel pointer
 * @param xf Render transform
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::handle_asset_ptr_(
    CameraModel<TSpectral>* camera,
    const std::vector<Transform<float>>& instance_apparent_transforms)
{
    (void)camera;
    (void)instance_apparent_transforms;
}

/**
 * @brief Handle unresolved object asset pointer and add to unresolved vector.
 * @param light UnresolvedObject pointer
 * @param xf Render transform
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::handle_asset_ptr_(
    UnresolvedObject<TSpectral>* light,
    const std::vector<Transform<float>>& instance_apparent_transforms)
{
    add_unresolved_instance_(light->shared_from_this(), instance_apparent_transforms);
}

/**
 * @brief Handle model asset pointer and traverse its scene graph.
 * @param model Model pointer
 * @param instance_apparent_transforms Render transform
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::handle_asset_ptr_(
    Model<TSpectral>* model, const std::vector<Transform<float>>& instance_apparent_transforms)
{
    if (!model) {
        return;
    }

    // The model recorded is the one the scene graph's instance placed: a model within that
    // model's graph is one of its parts.
    const ObjectIds outer_ids = open_ids_;
    if (open_ids_.model == NO_OBJECT_ID) {
        open_ids_.model = model->id();
    }

    std::shared_ptr<FrameNode<TSpectral>> model_graph_ptr = model->root_node_;
    traverse_model_graph_(model_graph_ptr, instance_apparent_transforms);

    open_ids_ = outer_ids;
}

/**
 * @brief Add a primitive instance to the geometry batch.
 * @param primitive Primitive pointer
 * @param render_transform Render transform
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::add_primitive_instance_(
    std::shared_ptr<Primitive<TSpectral>> primitive,
    const std::vector<Transform<float>>& instance_apparent_transforms)
{
    auto* key = primitive.get();
    auto it = batch_lookup_.find(key);

    ObjectIds ids = open_ids_;
    ids.geometry = primitive->geometry->id();
    has_motion_blur_ = has_motion_blur_ || instance_apparent_transforms.size() > 1;

    std::size_t batch_index = 0;
    if (it != batch_lookup_.end()) {
        batch_index = it->second;
        primitives_[batch_index].instances.push_back(instance_apparent_transforms);
        primitives_[batch_index].ids.push_back(ids);
    } else {
        PrimitiveBatch<TSpectral> batch;
        batch.primitive = primitive;
        batch.instances.push_back(instance_apparent_transforms);
        batch.ids.push_back(ids);

        primitives_.push_back(std::move(batch));
        batch_index = primitives_.size() - 1;
        batch_lookup_[key] = batch_index;
    }

    // If the traversal is currently inside a designated instance, this instance is
    // one of that source's members (see begin_indirect_source_).
    if (open_indirect_index_ != NO_INDIRECT_SOURCE) {
        IndirectSourceMember<TSpectral> member;
        member.primitive = primitive;
        member.batch_index = batch_index;
        member.instance_index = primitives_[batch_index].instances.size() - 1;

        indirect_sources_[open_indirect_index_].members.push_back(std::move(member));
    }
}

/**
 * @brief Add a light instance to the lights vector.
 * @param light Light pointer
 * @param render_transform Render transform
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::add_light_instance_(
    std::shared_ptr<Light<TSpectral>> light,
    const std::vector<Transform<float>>& instance_apparent_transforms)
{
    LightInstance<TSpectral> instance;
    instance.light = light;
    instance.transforms = instance_apparent_transforms;
    has_motion_blur_ = has_motion_blur_ || instance.transforms.size() > 1;

    lights_.push_back(std::move(instance));
}

/**
 * @brief Add an unresolved object instance to the unresolved vector.
 * @param unresolved_object UnresolvedObject pointer
 * @param render_transform Render transform
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::add_unresolved_instance_(
    std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object,
    const std::vector<Transform<float>>& instance_apparent_transforms)
{
    UnresolvedInstance<TSpectral> instance;
    instance.unresolved_object = unresolved_object;
    instance.transforms = instance_apparent_transforms;
    for (double fraction : uniform_fractions_(instance_apparent_transforms.size())) {
        instance.times.push_back(time_at_(fraction));
    }

    unresolved_objects_.push_back(std::move(instance));
}

/**
 * @brief Opens a designated indirect source spanning the asset about to be collected.
 *
 * Every primitive instance added between this call and the matching
 * end_indirect_source_() is recorded as a member of the new source: exactly one
 * for a designated Primitive, or one per Primitive in the sub-graph of a
 * designated Model.
 *
 * @param instance_apparent_transforms Render transforms of the designated instance.
 * @throws std::runtime_error if a source is already open (designations cannot nest).
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::begin_indirect_source_(
    const std::vector<Transform<float>>& instance_apparent_transforms)
{
    if (open_indirect_index_ != NO_INDIRECT_SOURCE) {
        HUIRA_THROW_ERROR("SceneView::begin_indirect_source_ - An indirect source is already "
                          "open; designated instances cannot nest");
    }

    IndirectSourceInstance<TSpectral> source;
    source.transforms = instance_apparent_transforms;

    indirect_sources_.push_back(std::move(source));
    open_indirect_index_ = indirect_sources_.size() - 1;
}

/**
 * @brief Closes the source opened by begin_indirect_source_().
 * @param instance The designated instance, for diagnostics.
 * @throws std::runtime_error if the designated instance collected no primitive geometry.
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::end_indirect_source_(const Instance<TSpectral>& instance)
{
    const bool empty = indirect_sources_[open_indirect_index_].members.empty();
    open_indirect_index_ = NO_INDIRECT_SOURCE;

    if (empty) {
        // A Model whose sub-graph holds no Primitive (or holds only lights, cameras
        // or unresolved objects) has no surface to reflect from, and would leave the
        // sampling proxy undefined:
        HUIRA_THROW_ERROR("SceneView::end_indirect_source_ - " + instance.get_info() +
                          " is designated as an indirect illumination source but contributes no "
                          "Primitive geometry to this scene view");
    }
}

/**
 * @brief Populates each indirect source's world-space bounding spheres.
 *
 * Produces one conservative bounding sphere per temporal sample, enclosing every
 * member of the source. Each member contributes the eight corners of its
 * primitive's committed BLAS AABB, transformed by the same render transform that
 * places that instance in the TLAS. The affine image of a box is the convex hull
 * of the images of its corners, so a sphere containing every transformed corner
 * contains all of the geometry: the bound stays conservative under any affine
 * transform (including non-uniform scale), and for a designated Model it spans the
 * whole sub-graph.
 *
 * Deriving the bound from each member's placed transform, rather than composing a
 * model-space union box and transforming that once, is deliberate: Transform's TRS
 * composition is not associative under non-uniform scale, so a separately composed
 * chain could disagree with where Embree actually places the geometry and silently
 * break containment (the estimator's one unbiasedness precondition).
 */

/**
 * @brief Build a conservative camera-space bounding sphere for every TLAS occupant.
 *
 * Mirrors compute_indirect_source_bounds_(): each primitive's local AABB is read from
 * its committed BLAS, the eight corners are pushed through the instance transform at
 * each temporal sample, and a sphere is fitted around them. Sphere lights are bounded
 * directly from their radius.
 *
 * With motion blur, a ray can hit an occupant at any time in the exposure, not only at the
 * temporal samples, so each bound covers everything the occupant passes through between two
 * consecutive samples (see swept_bound_()). A static occupant still gets one bound.
 *
 * Every hit that a ray can register lies inside one of these spheres, so a set of ray
 * directions that misses all of them cannot hit anything. Any occupant that cannot be
 * bounded clears occupancy_bounds_complete_, which disables culling entirely rather
 * than culling against an incomplete set.
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::compute_occupancy_bounds_()
{
    occupancy_bounds_.clear();
    occupancy_bounds_complete_ = true;

    if (tlas_empty_) {
        return;
    }

    auto box_corner = [](const Vec3<float>& lower, const Vec3<float>& upper, std::size_t corner) {
        return Vec3<float>{(corner & 1) ? upper.x : lower.x,
                           (corner & 2) ? upper.y : lower.y,
                           (corner & 4) ? upper.z : lower.z};
    };

    for (const auto& batch : primitives_) {
        RTCScene blas = batch.primitive->geometry->blas();
        if (blas == nullptr) {
            occupancy_bounds_complete_ = false;
            return;
        }

        RTCBounds bounds{};
        rtcGetSceneBounds(blas, &bounds);

        const Vec3<float> lower{bounds.lower_x, bounds.lower_y, bounds.lower_z};
        const Vec3<float> upper{bounds.upper_x, bounds.upper_y, bounds.upper_z};

        // An empty or non-finite BLAS bound cannot be trusted to enclose anything.
        if (!std::isfinite(lower.x) || !std::isfinite(lower.y) || !std::isfinite(lower.z) ||
            !std::isfinite(upper.x) || !std::isfinite(upper.y) || !std::isfinite(upper.z) ||
            upper.x < lower.x || upper.y < lower.y || upper.z < lower.z) {
            occupancy_bounds_complete_ = false;
            return;
        }

        std::array<Vec3<float>, 8> corners_local;
        for (std::size_t corner = 0; corner < 8; ++corner) {
            corners_local[corner] = box_corner(lower, upper, corner);
        }

        std::vector<SampleBound> sample_bounds;
        for (const auto& instance_transforms : batch.instances) {
            sample_bounds.clear();
            for (const Transform<float>& xf : instance_transforms) {
                Vec3<float> lo{std::numeric_limits<float>::infinity()};
                Vec3<float> hi{-std::numeric_limits<float>::infinity()};
                std::array<Vec3<float>, 8> corners_world;
                for (std::size_t corner = 0; corner < 8; ++corner) {
                    corners_world[corner] = xf.apply_to_point(corners_local[corner]);
                    lo = glm::min(lo, corners_world[corner]);
                    hi = glm::max(hi, corners_world[corner]);
                }

                const Vec3<float> center = 0.5f * (lo + hi);
                float radius_sq = 0.f;
                for (std::size_t corner = 0; corner < 8; ++corner) {
                    const Vec3<float> d = corners_world[corner] - center;
                    radius_sq = std::max(radius_sq, glm::dot(d, d));
                }

                const float radius = std::sqrt(radius_sq);
                if (!std::isfinite(radius) || !std::isfinite(glm::dot(center, center))) {
                    occupancy_bounds_complete_ = false;
                    return;
                }
                // The box's center is its transformed local center (an affine map keeps the
                // corners' central symmetry), so this is how far the rotation carries it:
                const float lever = glm::length(center - xf.position);
                sample_bounds.push_back(
                    {center, radius, lever, xf.rotation.local_to_parent_quaternion()});
            }
            if (!push_swept_bounds_(sample_bounds)) {
                occupancy_bounds_complete_ = false;
                return;
            }
        }
    }

    for (const auto& light_inst : lights_) {
        auto sphere_light = std::dynamic_pointer_cast<SphereLight<TSpectral>>(light_inst.light);
        if (!sphere_light) {
            // Only sphere lights are attached to the TLAS today. Anything else that
            // starts being attached must be bounded here too, so fail closed.
            occupancy_bounds_complete_ = false;
            return;
        }
        const float radius = sphere_light->radius().to_si_f();
        std::vector<SampleBound> sample_bounds;
        for (const Transform<float>& xf : light_inst.transforms) {
            // The sphere is placed at the instance origin and is not scaled by the
            // TLAS instancing, matching how build_tlas_() attaches it. Its center is the
            // instance's origin, so a rotation does not move it.
            sample_bounds.push_back(
                {xf.position, radius, 0.f, xf.rotation.local_to_parent_quaternion()});
        }
        if (!push_swept_bounds_(sample_bounds)) {
            occupancy_bounds_complete_ = false;
            return;
        }
    }
}

/**
 * @brief Add the bounds that hold an occupant over the whole exposure, from its bounds at each
 *        temporal sample.
 *
 * Between two temporal samples, Embree interpolates an instance's translation and scale
 * linearly, and its rotation along the shorter arc between the two (slerp). A point of the
 * geometry at a distance @c lever from the instance's origin follows the line between its
 * positions at the two samples, except that the rotation carries it off that line: by at most
 * @c lever times the angle turned, as a rotation by an angle moves a point at most that far.
 * So between two samples the occupant stays within a sphere around the middle of that line,
 * whose radius is half the line's length, plus that drift, plus the larger of the two radii
 * (the scale changes linearly, so the geometry never reaches further from the moving center
 * than it does at one end or the other).
 *
 * @param sample_bounds The occupant's bound at each temporal sample. A static occupant (one
 *        sample, or the same bound at every sample) gets one bound.
 * @return False if a bound is not finite.
 */
template <IsSpectral TSpectral>
bool SceneView<TSpectral>::push_swept_bounds_(const std::vector<SampleBound>& sample_bounds)
{
    const bool moves =
        std::any_of(sample_bounds.begin(), sample_bounds.end(), [&](const SampleBound& b) {
            return b.center != sample_bounds.front().center ||
                   b.radius != sample_bounds.front().radius ||
                   b.rotation != sample_bounds.front().rotation;
        });
    if (!moves) {
        occupancy_bounds_.push_back({sample_bounds.front().center, sample_bounds.front().radius});
        return true;
    }

    for (std::size_t i = 0; i + 1 < sample_bounds.size(); ++i) {
        const SampleBound& a = sample_bounds[i];
        const SampleBound& b = sample_bounds[i + 1];

        // The angle between the two rotations, along the shorter arc, as Embree turns:
        const float cos_half = std::min(1.f, std::abs(glm::dot(a.rotation, b.rotation)));
        const float angle = 2.f * std::acos(cos_half);

        const Vec3<float> center = 0.5f * (a.center + b.center);
        const float radius = 0.5f * glm::length(b.center - a.center) +
                             std::max(a.lever, b.lever) * angle + std::max(a.radius, b.radius);
        if (!std::isfinite(radius) || !std::isfinite(glm::dot(center, center))) {
            return false;
        }
        occupancy_bounds_.push_back({center, radius});
    }
    return true;
}

template <IsSpectral TSpectral>
void SceneView<TSpectral>::compute_indirect_source_bounds_()
{
    // Bit i of the corner index selects upper vs lower on axis i:
    auto box_corner = [](const Vec3<float>& lower, const Vec3<float>& upper, std::size_t corner) {
        return Vec3<float>{(corner & 1) ? upper.x : lower.x,
                           (corner & 2) ? upper.y : lower.y,
                           (corner & 4) ? upper.z : lower.z};
    };

    for (auto& source : indirect_sources_) {
        // Each member's local-space AABB corners, read once from its committed BLAS:
        std::vector<std::array<Vec3<float>, 8>> member_corners(source.members.size());
        for (std::size_t m = 0; m < source.members.size(); ++m) {
            RTCScene blas = source.members[m].primitive->geometry->blas();
            RTCBounds bounds{};
            rtcGetSceneBounds(blas, &bounds);

            const Vec3<float> lower{bounds.lower_x, bounds.lower_y, bounds.lower_z};
            const Vec3<float> upper{bounds.upper_x, bounds.upper_y, bounds.upper_z};
            for (std::size_t corner = 0; corner < 8; ++corner) {
                member_corners[m][corner] = box_corner(lower, upper, corner);
            }
        }

        const std::size_t N = source.transforms.size();
        source.bounding_centers.resize(N);
        source.bounding_radii.resize(N);

        std::vector<Vec3<float>> world_corners;
        world_corners.reserve(8 * source.members.size());

        for (std::size_t i = 0; i < N; ++i) {
            world_corners.clear();
            for (std::size_t m = 0; m < source.members.size(); ++m) {
                const IndirectSourceMember<TSpectral>& member = source.members[m];
                const std::vector<Transform<float>>& member_transforms =
                    primitives_[member.batch_index].instances[member.instance_index];
                const Transform<float>& xf =
                    member_transforms[std::min(i, member_transforms.size() - 1)];

                for (const Vec3<float>& corner_local : member_corners[m]) {
                    world_corners.push_back(xf.apply_to_point(corner_local));
                }
            }

            // Centre on the world AABB of the corners. For a single member this is
            // the transformed local box centre, since an affine map preserves the
            // corners' central symmetry: the lone-Primitive bound is unchanged.
            Vec3<float> lower = world_corners[0];
            Vec3<float> upper = world_corners[0];
            for (const Vec3<float>& corner_world : world_corners) {
                lower = glm::min(lower, corner_world);
                upper = glm::max(upper, corner_world);
            }
            const Vec3<float> center = 0.5f * (lower + upper);

            float radius = 0.0f;
            for (const Vec3<float>& corner_world : world_corners) {
                radius = std::max(radius, glm::length(corner_world - center));
            }

            source.bounding_centers[i] = center;
            source.bounding_radii[i] = radius;
        }
    }
}

/**
 * @brief Traverse a model's scene graph and collect instances.
 * @param node Node to traverse
 * @param parent_tf Parent transform
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::traverse_model_graph_(
    const std::shared_ptr<Node<TSpectral>> node,
    const std::vector<Transform<float>>& parent_transforms)
{
    // Node visibility is deliberately not consulted here. A Model's internal graph is
    // not reachable through ModelHandle, so nothing can set a flag on it. Honor it here
    // if those nodes are ever exposed.
    std::vector<Transform<float>> current_transforms(parent_transforms.size());
    for (std::size_t i = 0; i < parent_transforms.size(); ++i) {
        current_transforms[i] =
            parent_transforms[i] * static_cast<Transform<float>>(node->local_transform_);
    }

    if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
        // A node within the model is the part: the innermost instance that placed what it
        // holds.
        const std::uint64_t outer_part = open_ids_.part;
        open_ids_.part = instance->id();

        const auto& asset_var = instance->asset();
        std::visit([&](auto* raw_ptr) noexcept { handle_asset_ptr_(raw_ptr, current_transforms); },
                   asset_var);

        open_ids_.part = outer_part;
    }

    for (const auto& child : node->get_children()) {
        traverse_model_graph_(child, current_transforms);
    }
}

/**
 * @brief Gives an instance in the TLAS its poses at its temporal samples.
 *
 * Embree draws an instance between two poses by one of two interpolations, chosen by how the
 * poses are given: as quaternion decompositions, it moves the instance's origin in a straight
 * line and turns it rigidly (slerp), and as matrices, it interpolates the matrices, moving each
 * point of the instance in a straight line. For an instance whose rotation and scale are exactly
 * the same at every sample, the two are the same rigid motion, and matrices are given: Embree
 * builds their BVH an order of magnitude faster, since it bounds motion by matrices in closed
 * form, and quaternion motion with a root solver. Any other instance turns, and is given
 * quaternion decompositions, so that it turns rigidly. An instance with one pose has no motion,
 * and is given it as a quaternion decomposition, as before.
 *
 * @param geometry A new instance geometry.
 * @param transforms Its poses at its temporal samples, evenly spaced through the exposure.
 * @param still_given_two_poses Whether an instance with one pose is given it twice (see
 *        build_tlas_()).
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::set_instance_poses_(RTCGeometry geometry,
                                               const std::vector<Transform<float>>& transforms,
                                               bool still_given_two_poses) const
{
    const std::size_t n_samples = transforms.size();
    const std::size_t n_poses = (n_samples == 1 && still_given_two_poses) ? 2 : n_samples;
    rtcSetGeometryTimeStepCount(geometry, static_cast<unsigned int>(n_poses));
    if (n_poses > 1) {
        rtcSetGeometryTimeRange(geometry, 0.0f, 1.0f);
    }

    // One pose is given as it always was; several, as matrices if they do not turn:
    const Quaternion<float> rotation = transforms.front().rotation.local_to_parent_quaternion();
    bool quaternions = n_poses == 1;
    for (const Transform<float>& transform : transforms) {
        quaternions = quaternions || transform.rotation.local_to_parent_quaternion() != rotation ||
                      transform.scale != transforms.front().scale;
    }

    for (std::size_t i = 0; i < n_poses; ++i) {
        const Transform<float>& transform = transforms[std::min(i, n_samples - 1)];
        if (quaternions) {
            const RTCQuaternionDecomposition decomposition = transform.to_embree();
            rtcSetGeometryTransformQuaternion(
                geometry, static_cast<unsigned int>(i), &decomposition);
        } else {
            const std::array<float, 12> matrix = transform.to_embree_matrix();
            rtcSetGeometryTransform(geometry,
                                    static_cast<unsigned int>(i),
                                    RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR,
                                    matrix.data());
        }
    }
}

/**
 * @brief Builds the top-level acceleration structure (TLAS) for the scene.
 *
 * Embree keeps instances with one pose and instances with several (that move during the
 * exposure) in separate BVHs, and traces every ray through both. With a few still instances
 * among many moving ones (a still Sun, say, among moving bodies), the second traversal costs
 * more than the still ones cost in the moving BVH: rays were traced up to 2.4 times slower. So
 * when anything moves, still instances are given their pose twice, and every instance is in one
 * BVH. Where still instances greatly outnumber moving ones, a BVH of their own is faster (by
 * about 10% at 256 to 1), and they keep it beyond MOST_STILL_PER_MOVING_IN_ONE_BVH to 1.
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::build_tlas_()
{
    tlas_ = rtcNewScene(device_->get());
    const bool motion_blur = has_motion_blur_;

    std::size_t n_still = 0;
    std::size_t n_moving = 0;
    auto count = [&](const std::vector<Transform<float>>& transforms) {
        (transforms.size() > 1 ? n_moving : n_still) += 1;
    };
    for (const auto& batch : primitives_) {
        for (const auto& transforms : batch.instances) {
            count(transforms);
        }
    }
    for (const auto& light_inst : lights_) {
        if (std::dynamic_pointer_cast<SphereLight<TSpectral>>(light_inst.light) &&
            !light_inst.transforms.empty()) {
            count(light_inst.transforms);
        }
    }
    const bool still_given_two_poses =
        motion_blur && n_still <= MOST_STILL_PER_MOVING_IN_ONE_BVH * n_moving;
    // Robust traversal: required so BVH plane tests cannot drop marginal hits
    // at extreme coordinate ranges (planetary camera-relative scenes).
    RTCSceneFlags scene_flags = RTC_SCENE_FLAG_ROBUST;
    if (motion_blur) {
        scene_flags = static_cast<RTCSceneFlags>(scene_flags | RTC_SCENE_FLAG_DYNAMIC);
    }
    rtcSetSceneFlags(tlas_, scene_flags);

    // Reverse map from a primitive instance to the indirect source that owns it.
    // A source may own many instances (every Primitive of a designated Model), so
    // this is built once rather than scanned per instance.
    std::vector<std::vector<std::size_t>> indirect_lookup(primitives_.size());
    for (std::size_t batch_idx = 0; batch_idx < primitives_.size(); ++batch_idx) {
        indirect_lookup[batch_idx].assign(primitives_[batch_idx].instances.size(),
                                          NO_INDIRECT_SOURCE);
    }
    for (std::size_t src_idx = 0; src_idx < indirect_sources_.size(); ++src_idx) {
        for (const auto& member : indirect_sources_[src_idx].members) {
            indirect_lookup[member.batch_index][member.instance_index] = src_idx;
        }
    }

    tlas_empty_ = primitives_.empty() && lights_.empty();

    // Add Primitives
    for (std::size_t batch_idx = 0; batch_idx < primitives_.size(); ++batch_idx) {
        const auto& batch = primitives_[batch_idx];
        RTCScene blas = batch.primitive->geometry->blas();

        for (std::size_t inst_idx = 0; inst_idx < batch.instances.size(); ++inst_idx) {
            RTCGeometry inst_geom = rtcNewGeometry(device_->get(), RTC_GEOMETRY_TYPE_INSTANCE);
            rtcSetGeometryInstancedScene(inst_geom, blas);
            set_instance_poses_(inst_geom, batch.instances[inst_idx], still_given_two_poses);
            rtcSetGeometryMask(inst_geom, MASK_GEOMETRY_);

            rtcCommitGeometry(inst_geom);
            unsigned int geom_id = rtcAttachGeometry(tlas_, inst_geom);
            rtcReleaseGeometry(inst_geom);

            if (geom_id >= instance_mappings_.size()) {
                instance_mappings_.resize(geom_id + 1);
            }

            // Explicitly label this as a Mesh hit
            InstanceMapping mapping;
            mapping.type = GeometryType::Primitive;
            mapping.batch_index = batch_idx;
            mapping.instance_index = inst_idx;
            mapping.light_index = 0;
            mapping.indirect_index = indirect_lookup[batch_idx][inst_idx];
            instance_mappings_[geom_id] = mapping;
        }
    }

    // Add Sphere Lights
    for (std::size_t l_idx = 0; l_idx < lights_.size(); ++l_idx) {
        const auto& light_inst = lights_[l_idx];

        auto sphere_light = std::dynamic_pointer_cast<SphereLight<TSpectral>>(light_inst.light);
        if (!sphere_light) {
            continue;
        }

        std::size_t N = light_inst.transforms.size();
        if (N == 0) {
            continue;
        }

        // Create a BLAS containing a single static sphere at the origin
        RTCScene sphere_blas = rtcNewScene(device_->get());
        RTCGeometry geom = rtcNewGeometry(device_->get(), RTC_GEOMETRY_TYPE_SPHERE_POINT);
        rtcSetGeometryMask(geom, MASK_LIGHT_);

        if (!geom) {
            RTCError err = rtcGetDeviceError(device_->get());
            HUIRA_THROW_ERROR("Embree failed to create SPHERE_POINT. Ensure EMBREE_GEOMETRY_SPHERE "
                              "is enabled in your build. Error: " +
                              std::to_string(static_cast<int>(err)));
        }

        float* vertex = static_cast<float*>(rtcSetNewGeometryBuffer(
            geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT4, 4 * sizeof(float), 1));

        if (!vertex) {
            RTCError err = rtcGetDeviceError(device_->get());
            HUIRA_THROW_ERROR("Embree failed to allocate sphere buffer. Error: " +
                              std::to_string(static_cast<int>(err)));
        }

        // The sphere itself sits statically at the origin
        vertex[0] = 0.0f;
        vertex[1] = 0.0f;
        vertex[2] = 0.0f;
        vertex[3] = sphere_light->radius().to_si_f();

        rtcCommitGeometry(geom);
        rtcAttachGeometry(sphere_blas, geom);
        rtcReleaseGeometry(geom);
        rtcCommitScene(sphere_blas);

        // Instance the sphere BLAS into the TLAS
        RTCGeometry inst_geom = rtcNewGeometry(device_->get(), RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(inst_geom, sphere_blas);
        set_instance_poses_(inst_geom, light_inst.transforms, still_given_two_poses);
        rtcSetGeometryMask(inst_geom, MASK_LIGHT_);
        rtcCommitGeometry(inst_geom);
        unsigned int geom_id = rtcAttachGeometry(tlas_, inst_geom);
        rtcReleaseGeometry(inst_geom);

        // Embree reference counts scenes; the instance holds a reference, so we release ours.
        rtcReleaseScene(sphere_blas);

        // Map geom_id back to lights array
        if (geom_id >= instance_mappings_.size()) {
            instance_mappings_.resize(geom_id + 1);
        }

        InstanceMapping mapping;
        mapping.type = GeometryType::Light;
        mapping.batch_index = 0;
        mapping.instance_index = 0;
        mapping.light_index = l_idx;
        instance_mappings_[geom_id] = mapping;
    }

    rtcCommitScene(tlas_);
}

/**
 * @brief Evaluates one light's MIS-weighted NEE contribution at a shading point.
 *
 * Samples the light, performs the shadow/transmittance test, evaluates the BSDF,
 * and applies the power heuristic against the BSDF sampling PDF. Returns the
 * contribution WITHOUT any path throughput applied (the caller owns throughput).
 * Returns zero for occluded, back-facing (opaque), or unsampleable configurations.
 *
 * Templated on the material and its evaluated-parameter types so this header does
 * not need to name them; the Renderer and direct_lit_radiance() instantiate it
 * with the types produced by Material::evaluate().
 */
template <IsSpectral TSpectral>
template <typename TMaterial, typename TParams>
TSpectral
SceneView<TSpectral>::sample_light_contribution_(const LightInstance<TSpectral>& light_instance,
                                                 const Interaction<TSpectral>& isect,
                                                 const TMaterial* material,
                                                 const TParams& params,
                                                 const Interaction<TSpectral>& shading_isect,
                                                 const MediumStack<TSpectral>& medium_stack,
                                                 RandomSampler<float>& sampler,
                                                 float time) const
{
    Transform<float> current_transform = instance_transform_at(light_instance.transforms, time);

    auto sample = light_instance.light->sample_li(isect, current_transform, sampler);

    if (!sample) {
        return TSpectral{0};
    }
    const auto& ls = *sample;
    float light_dist = sample->distance;

    // Shadow test:
    if (params.transmission.max() <= 0.0f && glm::dot(ls.wi, isect.normal_g) <= 0.0f) {
        return TSpectral{0};
    }
    // Direction differs from the incident ray, so a normal-
    // direction offset (scaled by the propagated position
    // error bound) is required: it guarantees tangent-plane
    // clearance for any spawn direction, including grazing.
    Vec3<float> shadow_normal =
        (glm::dot(ls.wi, isect.normal_g) < 0.0f) ? -isect.normal_g : isect.normal_g;
    Vec3<float> shadow_origin = offset_spawn_point(isect.position, shadow_normal, isect.p_err);
    Ray<TSpectral> shadow_ray(shadow_origin, ls.wi);
    TSpectral transmittance =
        this->evaluate_transmittance(shadow_ray, light_dist, medium_stack, sampler, time);
    if (transmittance.max() <= 0.0f) {
        return TSpectral{0};
    }

    // Evaluate BSDF:
    TSpectral f = material->bsdf_eval(isect.wo, ls.wi, {params, shading_isect});
    float cos_theta = std::max(0.0f, glm::dot(shading_isect.normal_s, ls.wi));

    float bsdf_pdf = material->bsdf_pdf(isect.wo, ls.wi, {params, shading_isect});
    float mis_weight = power_heuristic(ls.pdf, bsdf_pdf);

    // Multiply the final direct lighting by the transmittance
    return (ls.Li / ls.pdf) * f * cos_theta * mis_weight * transmittance;
}

/**
 * @brief Evaluates the direct (next-event-estimated) radiance leaving a hit point.
 *
 * Resolves the hit, evaluates its material, and accumulates the MIS-weighted
 * contribution of every light in the view toward the hit point, returning the
 * outgoing radiance along -ray.direction(). Only primitive (non-light) geometry
 * is shaded; hits on light geometry return zero, as emission is the caller's
 * responsibility. Partial opacity at the hit point is not resampled here.
 *
 * @param ray The ray that produced the hit.
 * @param hit The hit record (must reference primitive geometry to shade).
 * @param sampler Random sampler for light sampling and transmittance estimation.
 * @param time Normalized time in [0, 1] for motion blur.
 * @param medium_stack The medium stack at the hit point (defaults to empty).
 * @return The direct-lit outgoing spectral radiance at the hit.
 */
template <IsSpectral TSpectral>
TSpectral
SceneView<TSpectral>::direct_lit_radiance(const Ray<TSpectral>& ray,
                                          const HitRecord& hit,
                                          RandomSampler<float>& sampler,
                                          float time,
                                          const MediumStack<TSpectral>& medium_stack) const
{
    if (!hit.hit()) {
        return TSpectral{0};
    }

    const auto& mapping = instance_mappings_[hit.inst_id];
    if (mapping.type != GeometryType::Primitive) {
        // Emission from light geometry is the caller's responsibility; this routine
        // only shades reflective (primitive) surfaces.
        return TSpectral{0};
    }

    // Resolve full shading data and look up the material:
    Interaction<TSpectral> isect = this->resolve_hit(ray, hit);
    const auto& batch = primitives_[mapping.batch_index];
    const auto* material = batch.primitive->material.get();

    auto [params, shading_isect] = material->evaluate(isect);

    TSpectral radiance{0};
    for (const auto& light_instance : lights_) {
        radiance += this->sample_light_contribution_(
            light_instance, isect, material, params, shading_isect, medium_stack, sampler, time);
    }
    return radiance;
}
} // namespace huira
