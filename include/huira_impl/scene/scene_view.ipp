#include <memory>
#include <vector>
#include <limits>

#include "embree4/rtcore.h"

#include "huira/assets/lights/light.hpp"
#include "huira/geometry/mesh.hpp"

#include "huira/core/physics.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/handles/camera_handle.hpp"
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
        : exposure_interval_{ exposure_interval },
        device_{ scene.device_ }
    {
        HUIRA_TRACE_SCOPE("SceneView::SceneView");
        HUIRA_LOG_INFO("Created over interval [" +
            std::to_string(exposure_interval.start.et()) + ",  " +
            std::to_string(exposure_interval.end.et()) + "]");

        // Create the temporal samples:
        if (num_temporal_samples < 1) {
            num_temporal_samples = 1;
        }
        temporal_samples_ = exposure_interval_.samples(num_temporal_samples);

        // Get the camera model:
        auto camera_node = camera_instance.get();
        const auto& asset_var = camera_node->asset();
        if (!std::holds_alternative<CameraModel<TSpectral>*>(asset_var)) {
            HUIRA_THROW_ERROR("SceneView received an Instance for the observer that does not contain a CameraModel!");
        }
        this->camera_model_ = std::get<CameraModel<TSpectral>*>(asset_var)->shared_from_this();

        // Extract camera pose:
        std::vector<Transform<double>> observer_transforms(temporal_samples_.size());
        std::vector<Transform<double>> observer_inverses(temporal_samples_.size());
        camera_to_world_ = std::vector<Transform<float>>(temporal_samples_.size());
        for (std::size_t i = 0; i < temporal_samples_.size(); ++i) {
            Transform<double> obs_ssb = camera_node->get_ssb_transform_(temporal_samples_[0], temporal_samples_[i]);
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
        HUIRA_LOG_INFO("SceneView collected " + std::to_string(primitives_.size()) + " unique primitive batches and " +
            std::to_string(lights_.size()) + " light instances.");

        // Check for unlinked objects:
        for (auto& primitive : scene.primitives_) {
            auto* key = primitive.get();
            if (batch_lookup_.find(key) == batch_lookup_.end()) {
                HUIRA_LOG_WARNING("Primitive[" + std::to_string(primitive->id()) + "] '" + primitive->name() +
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
                HUIRA_LOG_WARNING("UnresolvedObject[" + std::to_string(unresolved_object->id()) + "] '" + unresolved_object->name() +
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
                Vec3<double> aberrated_direction = compute_aberrated_direction(direction, observer_transforms[j].velocity);
                Vec3<double> apparent_direction = observer_transforms[j].rotation.inverse() * aberrated_direction;
                star_samples[j] = Star<TSpectral>(apparent_direction, irradiance);
            }

            stars_[i] = star_samples;
        }
        
        
        // Resolve all unresolved objects now that we have light positions
        for (auto& unresolved_object : unresolved_objects_) {
            // NOTE: Here we use the irradiance computed using the transform at the start of the interval
            unresolved_object.unresolved_object->resolve_irradiance(
                unresolved_object.transforms[0],
                lights_
            );
        }

        build_tlas_();
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
    HitRecord SceneView<TSpectral>::intersect(const Ray<TSpectral>& ray, float time) const
    {
        RTCRayHit rayhit{};
        rayhit.ray.org_x = ray.origin().x;
        rayhit.ray.org_y = ray.origin().y;
        rayhit.ray.org_z = ray.origin().z;
        rayhit.ray.dir_x = ray.direction().x;
        rayhit.ray.dir_y = ray.direction().y;
        rayhit.ray.dir_z = ray.direction().z;
        rayhit.ray.tnear = 0.f;
        rayhit.ray.tfar = std::numeric_limits<float>::infinity();
        rayhit.ray.time = time;
        rayhit.ray.mask = 0xFFFFFFFF;
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
        args.context = &context;          // Pass &context directly

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
            rec.Ng = Vec3<float>{ rayhit.hit.Ng_x, rayhit.hit.Ng_y, rayhit.hit.Ng_z };
        }
        return rec;
    }

    /**
     * @brief Test if a ray is occluded by any geometry in the scene.
     * @param ray The ray to test for occlusion.
     * @param t_far The maximum distance to check for occlusion.
     * @param time The time for motion blur.
     * @return True if the ray is occluded, false otherwise.
     */
    template <IsSpectral TSpectral>
    bool SceneView<TSpectral>::occluded(const Ray<TSpectral>& ray, float t_far, float time) const
    {
        RTCRay rtc_ray{};
        rtc_ray.org_x = ray.origin().x;
        rtc_ray.org_y = ray.origin().y;
        rtc_ray.org_z = ray.origin().z;
        rtc_ray.dir_x = ray.direction().x;
        rtc_ray.dir_y = ray.direction().y;
        rtc_ray.dir_z = ray.direction().z;
        rtc_ray.tnear = 0.f;
        rtc_ray.tfar = t_far;
        rtc_ray.time = time;
        rtc_ray.mask = MASK_GEOMETRY_;
        rtc_ray.flags = 0;

        // Setup custom context
        RayContext<TSpectral> context;
        rtcInitRayQueryContext(&context); // Pass &context directly
        context.scene_view = this;

        // Wrap in Embree 4 arguments
        RTCOccludedArguments args;
        rtcInitOccludedArguments(&args);
        args.context = &context;          // Pass &context directly

        // Pass the args into the trace call
        rtcOccluded1(tlas_, &rtc_ray, &args);

        // Embree sets tfar to -inf on occlusion:
        return rtc_ray.tfar < 0.0f;
    }


    /**
     * @brief Resolve a hit record into a full interaction, including position, normals, UVs, etc.
     * @param ray The ray that caused the hit.
     * @param hit The hit record to resolve.
     * @return The resolved interaction.
     */
    template <IsSpectral TSpectral>
    Interaction<TSpectral> SceneView<TSpectral>::resolve_hit(
        const Ray<TSpectral>& ray,
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

        isect.position = xf.apply_to_point(isect.position);
        isect.normal_g = glm::normalize(xf.apply_to_direction(hit.Ng));
        isect.normal_g = glm::dot(ray.direction(), isect.normal_g) < 0.0f ? isect.normal_g : -isect.normal_g;
        isect.normal_s = glm::normalize(xf.apply_to_direction(isect.normal_s)); // TODO Do we need to invert this as well?
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
     * @brief Intersect a batch of rays with the scene and return their hit records.
     * @param rays The rays to intersect.
     * @param time The time for motion blur.
     * @return A vector of hit records corresponding to each ray.
     */
    template <IsSpectral TSpectral>
    std::vector<HitRecord> SceneView<TSpectral>::intersect(const std::vector<Ray<TSpectral>>& rays, float time) const
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
     * @brief Test if a batch of rays are occluded by any geometry in the scene.
     * @param rays The rays to test for occlusion.
     * @param t_far The maximum distance to check for occlusion.
     * @param time The time for motion blur.
     * @return A vector of booleans indicating occlusion for each ray.
     */
    template <IsSpectral TSpectral>
    std::vector<bool> SceneView<TSpectral>::occluded(const std::vector<Ray<TSpectral>>& rays, float t_far, float time) const
    {
        std::vector<bool> occlusion_results(rays.size());
        tbb::parallel_for(tbb::blocked_range<size_t>(0, rays.size()),
            [&](const tbb::blocked_range<size_t>& range) {
                for (size_t i = range.begin(); i < range.end(); ++i) {
                    occlusion_results[i] = occluded(rays[i], t_far, time);
                }
            });
        return occlusion_results;
    }

    /**
     * @brief Resolve a batch of hit records into full interactions.
     * @param rays The rays that caused the hits.
     * @param hits The hit records to resolve.
     * @return A vector of resolved interactions.
     */
    template <IsSpectral TSpectral>
    std::vector<Interaction<TSpectral>> SceneView<TSpectral>::resolve_hits(
        const std::vector<Ray<TSpectral>>& rays,
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
    void SceneView<TSpectral>::traverse_and_collect_(const std::shared_ptr<Node<TSpectral>>& node,
        const std::vector<Transform<double>>& observer_transforms,
        const std::vector<Transform<double>>& observer_inverses,
        ObservationMode obs_mode)
    {
        if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
            std::vector<Transform<float>> render_transforms(temporal_samples_.size());
            for (std::size_t i = 0; i < temporal_samples_.size(); ++i) {
                const Transform<double>& obs_ssb = observer_transforms[i];
                const Transform<double>& obs_inv = observer_inverses[i];

                Transform<double> instance_ssb = node->get_apparent_transform(obs_mode, temporal_samples_[0], temporal_samples_[i], obs_ssb);

                Transform<double> local_apparent = obs_inv * instance_ssb;

                // Down-cast to single precision once in local space:
                render_transforms[i] = static_cast<Transform<float>>(local_apparent);
            }

            const auto& asset_var = instance->asset();
            std::visit([&](auto* raw_ptr) noexcept {
                handle_asset_ptr_(raw_ptr, render_transforms);
                }, asset_var);
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
    void SceneView<TSpectral>::handle_asset_ptr_(Primitive<TSpectral>* primitive, const std::vector<Transform<float>>& instance_apparent_transforms)
    {
        add_primitive_instance_(primitive->shared_from_this(), instance_apparent_transforms);
    }


    /**
     * @brief Handle light asset pointer and add to lights vector.
     * @param light Light pointer
     * @param xf Render transform
     */
    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::handle_asset_ptr_(Light<TSpectral>* light, const std::vector<Transform<float>>& instance_apparent_transforms)
    {
        add_light_instance_(light->shared_from_this(), instance_apparent_transforms);
    }


    /**
     * @brief Handle camera model asset pointer (no-op).
     * @param camera CameraModel pointer
     * @param xf Render transform
     */
    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::handle_asset_ptr_(CameraModel<TSpectral>* camera, const std::vector<Transform<float>>& instance_apparent_transforms)
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
    void SceneView<TSpectral>::handle_asset_ptr_(UnresolvedObject<TSpectral>* light, const std::vector<Transform<float>>& instance_apparent_transforms)
    {
        add_unresolved_instance_(light->shared_from_this(), instance_apparent_transforms);
    }


    /**
     * @brief Handle model asset pointer and traverse its scene graph.
     * @param model Model pointer
     * @param instance_apparent_transforms Render transform
     */
    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::handle_asset_ptr_(Model<TSpectral>* model, const std::vector<Transform<float>>& instance_apparent_transforms)
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
    void SceneView<TSpectral>::add_primitive_instance_(std::shared_ptr<Primitive<TSpectral>> primitive, const std::vector<Transform<float>>& instance_apparent_transforms)
    {
        auto* key = primitive.get();
        auto it = batch_lookup_.find(key);

        if (it != batch_lookup_.end()) {
            size_t index = it->second;
            primitives_[index].instances.push_back(instance_apparent_transforms);
        }
        else {
            PrimitiveBatch<TSpectral> batch;
            batch.primitive = primitive;
            batch.instances.push_back(instance_apparent_transforms);

            primitives_.push_back(std::move(batch));
            batch_lookup_[key] = primitives_.size() - 1;
        }
    }


    /**
     * @brief Add a light instance to the lights vector.
     * @param light Light pointer
     * @param render_transform Render transform
     */
    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::add_light_instance_(std::shared_ptr<Light<TSpectral>> light, const std::vector<Transform<float>>& instance_apparent_transforms)
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
    void SceneView<TSpectral>::add_unresolved_instance_(std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object, const std::vector<Transform<float>>& instance_apparent_transforms)
    {
        UnresolvedInstance<TSpectral> instance;
        instance.unresolved_object = unresolved_object;
        instance.transforms = instance_apparent_transforms;

        unresolved_objects_.push_back(std::move(instance));
    }


    /**
     * @brief Traverse a model's scene graph and collect instances.
     * @param node Node to traverse
     * @param parent_tf Parent transform
     */
    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::traverse_model_graph_(const std::shared_ptr<Node<TSpectral>> node, const std::vector<Transform<float>>& parent_transforms)
    {
        std::vector<Transform<float>> current_transforms(parent_transforms.size());
        for (std::size_t i = 0; i < parent_transforms.size(); ++i) {
            current_transforms[i] = parent_transforms[i] * static_cast<Transform<float>>(node->local_transform_);
        }

        if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
            const auto& asset_var = instance->asset();
            std::visit([&](auto* raw_ptr) noexcept {
                handle_asset_ptr_(raw_ptr, current_transforms);
                }, asset_var);
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
        if (motion_blur) {
            rtcSetSceneFlags(tlas_, RTC_SCENE_FLAG_DYNAMIC);
        }

        // Add Primitives
        for (std::size_t batch_idx = 0; batch_idx < primitives_.size(); ++batch_idx) {
            const auto& batch = primitives_[batch_idx];
            RTCScene blas = batch.primitive->geometry->blas();

            const auto* material = batch.primitive->material.get();
            bool needs_alpha = material->has_alpha();

            // Register the alpha filter on the BLAS geometry (once per primitive).
            if (needs_alpha) {
                RTCGeometry geom = rtcGetGeometry(blas, 0);
                if (geom) {
                    rtcSetGeometryOccludedFilterFunction(geom, alpha_occlusion_filter_);
                    rtcSetGeometryIntersectFilterFunction(geom, alpha_intersection_filter_);

                    RTCError err = rtcGetDeviceError(device_->get());
                    if (err != RTC_ERROR_NONE) {
                        HUIRA_THROW_ERROR("Embree failed to set filter function. Error code: " + std::to_string(err));
                    }

                    rtcCommitGeometry(geom);
                    rtcCommitScene(blas);
                }
            }

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
                    rtcSetGeometryTransformQuaternion(inst_geom,
                        static_cast<unsigned int>(t_idx),
                        &decomp);
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
                HUIRA_THROW_ERROR("Embree failed to create SPHERE_POINT. Ensure EMBREE_GEOMETRY_SPHERE is enabled in your build. Error: " +
                    std::to_string(static_cast<int>(err)));
            }

            float* vertex = static_cast<float*>(rtcSetNewGeometryBuffer(geom,
                RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT4, 4 * sizeof(float), 1));

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
                rtcSetGeometryTransformQuaternion(inst_geom,
                    static_cast<unsigned int>(t_idx),
                    &decomp);
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

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::alpha_occlusion_filter_(const RTCFilterFunctionNArguments* args) noexcept {
        if (args->N != 1) {
            return;
        }

        int* valid = args->valid;
        if (*valid == 0) {
            return;
        }

        auto* ctx = static_cast<RayContext<TSpectral>*>(args->context);
        const auto* scene_view = ctx->scene_view;

        unsigned int inst_id = RTCHitN_instID(args->hit, args->N, 0, 0);
        const auto& mapping = scene_view->instance_mappings_[inst_id];
        const auto& batch = scene_view->primitives_[mapping.batch_index];
        
        const auto* material = batch.primitive->material.get();
        const auto* geometry = batch.primitive->geometry.get();
        
        HitRecord temp_hit;
        temp_hit.prim_id = RTCHitN_primID(args->hit, args->N, 0);
        temp_hit.u = RTCHitN_u(args->hit, args->N, 0);
        temp_hit.v = RTCHitN_v(args->hit, args->N, 0);
        
        Vec2<float> uv = geometry->compute_uv(temp_hit);
        
        float alpha = material->alpha_factor();
        if (material->has_alpha_texture()) {
            alpha *= material->alpha_image_->sample_bilinear(uv.x, uv.y);
        }
        
        if (alpha < 1.0f) {
            thread_local std::mt19937 generator(std::random_device{}());
            thread_local std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

            if (distribution(generator) > alpha) {
                *valid = 0;
            }
        }
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::alpha_intersection_filter_(const RTCFilterFunctionNArguments* args) noexcept {
        alpha_occlusion_filter_(args);
    }
}
