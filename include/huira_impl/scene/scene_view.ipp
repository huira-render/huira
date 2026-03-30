#include <memory>
#include <vector>
#include <limits>

#include "huira/assets/lights/light.hpp"
#include "huira/assets/mesh.hpp"

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
        HUIRA_LOG_INFO("SceneView collected " + std::to_string(geometry_.size()) + " unique mesh batches and " +
            std::to_string(lights_.size()) + " light instances.");

        // Check for unlinked objects:
        for (auto& mesh : scene.meshes_) {
            auto* key = mesh.get();
            if (batch_lookup_.find(key) == batch_lookup_.end()) {
                HUIRA_LOG_WARNING("Mesh[" + std::to_string(mesh->id()) + "] '" + mesh->name() +
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

        HUIRA_LOG_INFO("SceneView::SceneView - Created over interval [" + std::to_string(exposure_interval.start.et()) +
            ",  " + std::to_string(exposure_interval.end.et()) + "] " + 
            "for CameraModel[" + std::to_string(camera_model_->id()) + "] '" + camera_model_->name() + "'." +
            " MOTION BLUR ON");
    }

    template <IsSpectral TSpectral>
    SceneView<TSpectral>::~SceneView()
    {
        if (tlas_) {
            rtcReleaseScene(tlas_);
        }

        HUIRA_LOG_INFO("SceneView::~SceneView - destroyed, released TLAS and cleared geometry and lights.");
    }


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

        rtcIntersect1(tlas_, &rayhit);

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
        rtc_ray.mask = 0xFFFFFFFF;
        rtc_ray.flags = 0;

        rtcOccluded1(tlas_, &rtc_ray);

        // Embree sets tfar to -inf on occlusion:
        return rtc_ray.tfar < 0.0f;
    }


    template <IsSpectral TSpectral>
    Interaction<TSpectral> SceneView<TSpectral>::resolve_hit(
        const Ray<TSpectral>& ray,
        const HitRecord& hit) const
    {
        Interaction<TSpectral> isect{};

        // Look up which mesh and instance this hit corresponds to:
        const auto& mapping = instance_mappings_[hit.inst_id];
        const auto& batch = geometry_[mapping.batch_index];
        const auto& mesh = batch.mesh;
        const auto& indices = mesh->index_buffer();
        const auto& vertices = mesh->vertex_buffer();

        // Triangle vertex indices:
        std::uint32_t idx0 = indices[hit.prim_id * 3 + 0];
        std::uint32_t idx1 = indices[hit.prim_id * 3 + 1];
        std::uint32_t idx2 = indices[hit.prim_id * 3 + 2];

        // Embree barycentric convention: P = (1-u-v)*V0 + u*V1 + v*V2
        float w = 1.0f - hit.u - hit.v;

        const auto& instance_transforms = batch.instances[mapping.instance_index];
        const Transform<float>& xf = instance_transforms[0];

        // Hit position from the ray (more numerically stable than interpolating):
        isect.position = w * vertices[idx0].position
            + hit.u * vertices[idx1].position
            + hit.v * vertices[idx2].position;
        isect.wo = -ray.direction();

        isect.position = xf.apply_to_point(isect.position);
        isect.normal_g = glm::normalize(xf.apply_to_direction(hit.Ng));


        // Interpolate shading normal:
        Vec3<float> n0 = vertices[idx0].normal;
        Vec3<float> n1 = vertices[idx1].normal;
        Vec3<float> n2 = vertices[idx2].normal;
        Vec3<float> ns_object = w * n0 + hit.u * n1 + hit.v * n2;
        isect.normal_s = glm::normalize(xf.apply_to_direction(ns_object));

        isect.normal_g = glm::dot(ray.direction(), isect.normal_g) < 0.0f ? isect.normal_g : -isect.normal_g;
        if (glm::dot(isect.normal_s, isect.normal_g) < 0.0f) {
            isect.normal_s = -isect.normal_s;
        }

        // Interpolate UVs (transform-independent):
        isect.uv = w * vertices[idx0].uv
            + hit.u * vertices[idx1].uv
            + hit.v * vertices[idx2].uv;

        // Interpolate vertex albedo:
        isect.vertex_albedo = w * vertices[idx0].albedo
            + hit.u * vertices[idx1].albedo
            + hit.v * vertices[idx2].albedo;

        // Interpolate tangent frame:
        isect.tangent = Vec3<float>{ 0.0f };
        isect.bitangent = Vec3<float>{ 0.0f };
        if (mesh->has_tangents()) {
            const auto& tangents = mesh->tangent_buffer();
            Vec3<float> t_object = w * tangents[idx0].tangent
                + hit.u * tangents[idx1].tangent
                + hit.v * tangents[idx2].tangent;
            Vec3<float> bt_object = w * tangents[idx0].bitangent
                + hit.u * tangents[idx1].bitangent
                + hit.v * tangents[idx2].bitangent;

            isect.tangent = glm::normalize(xf.apply_to_direction(t_object));
            isect.bitangent = glm::normalize(xf.apply_to_direction(bt_object));

            if (glm::dot(glm::cross(isect.tangent, isect.bitangent), isect.normal_s) < 0.0f) {
                isect.bitangent = -isect.bitangent;
            }
        }

        return isect;
    }

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
     * @brief Handle mesh asset pointer and add to geometry batch.
     * @param mesh Mesh pointer
     * @param xf Render transform
     */
    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::handle_asset_ptr_(Mesh<TSpectral>* mesh, const std::vector<Transform<float>>& instance_apparent_transforms)
    {
        add_mesh_instance_(mesh->shared_from_this(), instance_apparent_transforms);
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
     * @brief Add a mesh instance to the geometry batch.
     * @param mesh Mesh pointer
     * @param render_transform Render transform
     */
    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::add_mesh_instance_(std::shared_ptr<Mesh<TSpectral>> mesh, const std::vector<Transform<float>>& instance_apparent_transforms)
    {
        auto* key = mesh.get();
        auto it = batch_lookup_.find(key);

        if (it != batch_lookup_.end()) {
            size_t index = it->second;
            geometry_[index].instances.push_back(instance_apparent_transforms);
        }
        else {
            MeshBatch<TSpectral> batch;
            batch.mesh = mesh;
            batch.instances.push_back(instance_apparent_transforms);

            geometry_.push_back(std::move(batch));

            batch_lookup_[key] = geometry_.size() - 1;
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

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::build_tlas_()
    {
        tlas_ = rtcNewScene(device_);
        bool motion_blur = (temporal_samples_.size() != 1);
        if (motion_blur) {
            rtcSetSceneFlags(tlas_, RTC_SCENE_FLAG_DYNAMIC);
        }

        // Add Meshes
        for (std::size_t batch_idx = 0; batch_idx < geometry_.size(); ++batch_idx) {
            const auto& batch = geometry_[batch_idx];
            RTCScene mesh_blas = batch.mesh->blas();

            for (std::size_t inst_idx = 0; inst_idx < batch.instances.size(); ++inst_idx) {

                std::size_t N = batch.instances[inst_idx].size();

                RTCGeometry inst_geom = rtcNewGeometry(device_, RTC_GEOMETRY_TYPE_INSTANCE);
                rtcSetGeometryInstancedScene(inst_geom, mesh_blas);
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

                rtcCommitGeometry(inst_geom);

                unsigned int geom_id = rtcAttachGeometry(tlas_, inst_geom);
                rtcReleaseGeometry(inst_geom);

                if (geom_id >= instance_mappings_.size()) {
                    instance_mappings_.resize(geom_id + 1);
                }

                // Explicitly label this as a Mesh hit
                InstanceMapping mapping;
                mapping.type = GeometryType::Mesh;
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
            RTCScene sphere_blas = rtcNewScene(device_);
            RTCGeometry geom = rtcNewGeometry(device_, RTC_GEOMETRY_TYPE_SPHERE_POINT);

            if (!geom) {
                RTCError err = rtcGetDeviceError(device_);
                HUIRA_THROW_ERROR("Embree failed to create SPHERE_POINT. Ensure EMBREE_GEOMETRY_SPHERE is enabled in your build. Error: " + std::to_string(err));
            }

            float* vertex = static_cast<float*>(rtcSetNewGeometryBuffer(geom,
                RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT4, 4 * sizeof(float), 1));

            if (!vertex) {
                RTCError err = rtcGetDeviceError(device_);
                HUIRA_THROW_ERROR("Embree failed to allocate sphere buffer. Error: " + std::to_string(err));
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
            RTCGeometry inst_geom = rtcNewGeometry(device_, RTC_GEOMETRY_TYPE_INSTANCE);
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
}
