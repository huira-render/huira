#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "embree4/rtcore.h"
#include "huira/assets/lights/light.hpp"
#include "huira/core/physics.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/geometry/mesh.hpp"
#include "huira/handles/camera_handle.hpp"
#include "huira/render/shading_utils.hpp"
#include "huira/scene/scene.hpp"

namespace huira {
/**
 * @brief Construct a SceneView for a given scene, time, camera, and observation mode.
 *
 * Collects geometry, lights, unresolved objects, and stars for rendering.
 *
 * @param scene Scene to view
 * @param exposure_interval Exposure interval
 * @param camera_instance Camera instance handle
 * @param obs_mode Observation mode
 * @param num_temporal_samples Number of temporal samples for motion blur (default: 1)
 */
template <IsSpectral TSpectral>
SceneView<TSpectral>::SceneView(const Scene<TSpectral>& scene,
                                const Interval& exposure_interval,
                                const InstanceHandle<TSpectral>& camera_instance,
                                ObservationMode obs_mode,
                                std::size_t num_temporal_samples)
    : exposure_interval_{exposure_interval}, device_{scene.device_}
{
    HUIRA_TRACE_SCOPE("SceneView::SceneView");
    HUIRA_LOG_INFO("Created over interval [" + std::to_string(exposure_interval.start.et()) +
                   ",  " + std::to_string(exposure_interval.end.et()) + "]");

    // Create the temporal samples:
    if (num_temporal_samples < 1) {
        num_temporal_samples = 1;
    }
    temporal_samples_ = exposure_interval_.samples(num_temporal_samples);

    // Get the camera model:
    auto camera_node = camera_instance.get();
    const auto& asset_var = camera_node->asset();
    if (!std::holds_alternative<CameraModel<TSpectral>*>(asset_var)) {
        HUIRA_THROW_ERROR(
            "SceneView received an Instance for the observer that does not contain a CameraModel!");
    }
    this->camera_model_ = std::get<CameraModel<TSpectral>*>(asset_var)->shared_from_this();

    // Extract camera pose:
    std::vector<Transform<double>> observer_transforms(temporal_samples_.size());
    std::vector<Transform<double>> observer_inverses(temporal_samples_.size());
    camera_to_world_ = std::vector<Transform<float>>(temporal_samples_.size());
    for (std::size_t i = 0; i < temporal_samples_.size(); ++i) {
        Transform<double> obs_ssb =
            camera_node->get_ssb_transform_(temporal_samples_[0], temporal_samples_[i]);
        Rotation<double> sensor_rotation = camera_model_->sensor_rotation();
        obs_ssb.rotation = obs_ssb.rotation * sensor_rotation;

        observer_transforms[i] = obs_ssb;
        observer_inverses[i] = obs_ssb.inverse();
        camera_to_world_[i] = static_cast<Transform<float>>(observer_inverses[i]);
    }

    // Copy the background radiance:
    background_ = scene.background_;

    // Collect geometry and lights by traversing the scene graph:
    traverse_and_collect_(scene.root_node_, observer_transforms, observer_inverses, obs_mode);
    HUIRA_LOG_INFO("SceneView collected " + std::to_string(primitives_.size()) +
                   " unique primitive batches and " + std::to_string(lights_.size()) +
                   " light instances.");

    // Check for unlinked objects:
    for (auto& primitive : scene.primitives_) {
        auto* key = primitive.get();
        if (batch_lookup_.find(key) == batch_lookup_.end()) {
            HUIRA_LOG_WARNING("Primitive[" + std::to_string(primitive->id()) + "] '" +
                              primitive->name() +
                              "' is unlinked in the scene graph and will not be rendered.");
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
            HUIRA_LOG_WARNING("Light[" + std::to_string(light->id()) + "] '" + light->name() +
                              "' is unlinked in the scene graph and will not be rendered.");
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
            HUIRA_LOG_WARNING("UnresolvedObject[" + std::to_string(unresolved_object->id()) +
                              "] '" + unresolved_object->name() +
                              "' is unlinked in the scene graph and will not be rendered.");
        }
    }

    // Copy stars in camera frame:
    stars_ = std::vector<std::vector<Star<TSpectral>>>(scene.stars_.size());
    for (std::size_t i = 0; i < scene.stars_.size(); ++i) {
        Vec3<double> direction = scene.stars_[i].get_direction();
        TSpectral irradiance = scene.stars_[i].get_irradiance();

        std::vector<Star<TSpectral>> star_samples(temporal_samples_.size());
        for (std::size_t j = 0; j < temporal_samples_.size(); ++j) {
            // Compute stellar aberration:
            Vec3<double> aberrated_direction =
                compute_aberrated_direction(direction, observer_transforms[j].velocity);
            Vec3<double> apparent_direction =
                observer_transforms[j].rotation.inverse() * aberrated_direction;
            star_samples[j] = Star<TSpectral>(apparent_direction, irradiance);
        }

        stars_[i] = star_samples;
    }

    compute_indirect_source_bounds_();

    build_tlas_();

    // Resolve all unresolved objects now that the TLAS is built, so implementations
    // can cast occlusion/sampling rays through this view. Each object gets its own
    // deterministically seeded sampler for reproducible results.
    for (auto& unresolved_object : unresolved_objects_) {
        RandomSampler<float> sampler(
            static_cast<unsigned int>(unresolved_object.unresolved_object->id()));
        unresolved_object.unresolved_object->resolve_irradiance(
            unresolved_object.transforms, temporal_samples_, *this, sampler);
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
                                                       float time) const
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
    const auto& instance_transforms = batch.instances[mapping.instance_index];
    const Transform<float>& xf = instance_transforms[0];

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
 * @brief Traverse the scene graph and collect renderable objects.
 *
 * Recursively visits nodes and collects mesh, light, unresolved, and model instances.
 *
 * @param node Node to traverse
 * @param t_obs Observation time
 * @param obs_ssb Observer SSB transforms
 * @param obs_ssb Observer SSB inverse transforms
 * @param obs_mode Observation mode
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::traverse_and_collect_(
    const std::shared_ptr<Node<TSpectral>>& node,
    const std::vector<Transform<double>>& observer_transforms,
    const std::vector<Transform<double>>& observer_inverses,
    ObservationMode obs_mode)
{
    if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
        std::vector<Transform<float>> render_transforms(temporal_samples_.size());
        for (std::size_t i = 0; i < temporal_samples_.size(); ++i) {
            const Transform<double>& obs_ssb = observer_transforms[i];
            const Transform<double>& obs_inv = observer_inverses[i];

            Transform<double> instance_ssb = node->get_apparent_transform(
                obs_mode, temporal_samples_[0], temporal_samples_[i], obs_ssb);

            Transform<double> local_apparent = obs_inv * instance_ssb;

            // Down-cast to single precision once in local space:
            render_transforms[i] = static_cast<Transform<float>>(local_apparent);
        }

        // Indirect-source designation brackets the asset dispatch (the asset is
        // validated at set-time to be a Primitive or a Model). Everything the
        // dispatch collects in between becomes a member of one source: the lone
        // primitive instance, or every primitive instance in a model's sub-graph.
        const bool designated = instance->is_indirect_source();
        if (designated) {
            begin_indirect_source_(render_transforms);
        }

        const auto& asset_var = instance->asset();
        std::visit([&](auto* raw_ptr) noexcept { handle_asset_ptr_(raw_ptr, render_transforms); },
                   asset_var);

        if (designated) {
            end_indirect_source_(*instance);
        }
    }

    for (const auto& child : node->get_children()) {
        traverse_and_collect_(child, observer_transforms, observer_inverses, obs_mode);
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
    std::shared_ptr<FrameNode<TSpectral>> model_graph_ptr = model->root_node_;
    traverse_model_graph_(model_graph_ptr, instance_apparent_transforms);
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

    std::size_t batch_index = 0;
    if (it != batch_lookup_.end()) {
        batch_index = it->second;
        primitives_[batch_index].instances.push_back(instance_apparent_transforms);
    } else {
        PrimitiveBatch<TSpectral> batch;
        batch.primitive = primitive;
        batch.instances.push_back(instance_apparent_transforms);

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
    std::vector<Transform<float>> current_transforms(parent_transforms.size());
    for (std::size_t i = 0; i < parent_transforms.size(); ++i) {
        current_transforms[i] =
            parent_transforms[i] * static_cast<Transform<float>>(node->local_transform_);
    }

    if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
        const auto& asset_var = instance->asset();
        std::visit([&](auto* raw_ptr) noexcept { handle_asset_ptr_(raw_ptr, current_transforms); },
                   asset_var);
    }

    for (const auto& child : node->get_children()) {
        traverse_model_graph_(child, current_transforms);
    }
}

/**
 * @brief Builds the top-level acceleration structure (TLAS) for the scene.
 */
template <IsSpectral TSpectral>
void SceneView<TSpectral>::build_tlas_()
{
    tlas_ = rtcNewScene(device_->get());
    bool motion_blur = (temporal_samples_.size() != 1);
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

    // Add Primitives
    for (std::size_t batch_idx = 0; batch_idx < primitives_.size(); ++batch_idx) {
        const auto& batch = primitives_[batch_idx];
        RTCScene blas = batch.primitive->geometry->blas();

        for (std::size_t inst_idx = 0; inst_idx < batch.instances.size(); ++inst_idx) {
            std::size_t N = batch.instances[inst_idx].size();

            RTCGeometry inst_geom = rtcNewGeometry(device_->get(), RTC_GEOMETRY_TYPE_INSTANCE);
            rtcSetGeometryInstancedScene(inst_geom, blas);
            rtcSetGeometryTimeStepCount(inst_geom, static_cast<unsigned int>(N));

            if (motion_blur) {
                rtcSetGeometryTimeRange(inst_geom, 0.0f, 1.0f);
            }

            for (std::size_t t_idx = 0; t_idx < N; ++t_idx) {
                RTCQuaternionDecomposition decomp = batch.instances[inst_idx][t_idx].to_embree();
                rtcSetGeometryTransformQuaternion(
                    inst_geom, static_cast<unsigned int>(t_idx), &decomp);
            }

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
        rtcSetGeometryTimeStepCount(inst_geom, static_cast<unsigned int>(N));

        if (N > 1) {
            rtcSetGeometryTimeRange(inst_geom, 0.0f, 1.0f);
        }

        for (std::size_t t_idx = 0; t_idx < N; ++t_idx) {
            // Because we instance it, we get to use your existing Transform logic natively!
            RTCQuaternionDecomposition decomp = light_inst.transforms[t_idx].to_embree();
            rtcSetGeometryTransformQuaternion(inst_geom, static_cast<unsigned int>(t_idx), &decomp);
        }

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
    Transform<float> current_transform = interpolate_transform(light_instance.transforms, time);

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
