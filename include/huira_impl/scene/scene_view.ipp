#include <memory>
#include <vector>

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
     * @param t_obs Observation time
     * @param camera_instance Camera instance handle
     * @param obs_mode Observation mode
     */
    template <IsSpectral TSpectral>
    SceneView<TSpectral>::SceneView(const Scene<TSpectral>& scene,
        const Interval& exposure_interval,
        const InstanceHandle<TSpectral>& camera_instance,
        ObservationMode obs_mode,
        bool motion_blur, std::size_t num_temporal_samples)
        : exposure_interval_{ exposure_interval },
        device_{ scene.device_ }
    {
        // Create the temporal samples:
        if (motion_blur) {
            if (num_temporal_samples < 2) {
                HUIRA_THROW_ERROR("SceneView::SceneView - Motion blur requires 2 or more temporal samples");
            }
            temporal_samples_ = exposure_interval_.samples(num_temporal_samples);
        }
        else {
            temporal_samples_ = { exposure_interval_.center() };
        }

        // Get the camera model:
        auto camera_node = camera_instance.get();
        const auto& asset_var = camera_node->asset();
        if (!std::holds_alternative<CameraModel<TSpectral>*>(asset_var)) {
            HUIRA_THROW_ERROR("SceneView received an Instance for the observer that does not contain a CameraModel!");
        }
        this->camera_model_ = std::get<CameraModel<TSpectral>*>(asset_var)->shared_from_this();

        // Extract camera pose:
        std::vector<Transform<double>> observer_transforms(temporal_samples_.size());
        for (std::size_t i = 0; i < temporal_samples_.size(); ++i) {
            Transform<double> obs_ssb = camera_node->get_ssb_transform_(temporal_samples_[0], temporal_samples_[i]);
            Rotation<double> sensor_rotation = camera_model_->sensor_rotation();
            obs_ssb.rotation = obs_ssb.rotation * sensor_rotation;

            observer_transforms[i] = obs_ssb;
        }

        // Collect geometry and lights by traversing the scene graph:
        traverse_and_collect_(scene.root_node_, observer_transforms, obs_mode);
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


    /**
     * @brief Traverse the scene graph and collect renderable objects.
     *
     * Recursively visits nodes and collects mesh, light, unresolved, and model instances.
     *
     * @param node Node to traverse
     * @param t_obs Observation time
     * @param obs_ssb Observer SSB transform
     * @param obs_mode Observation mode
     */
    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::traverse_and_collect_(const std::shared_ptr<Node<TSpectral>>& node,
        const std::vector<Transform<double>>& observer_transforms, ObservationMode obs_mode)
    {
        if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
            std::vector<Transform<float>> render_transforms(temporal_samples_.size());
            for (std::size_t i = 0; i < temporal_samples_.size(); ++i) {
                const Transform<double>& obs_ssb = observer_transforms[i];

                Transform<double> instance_ssb = node->get_apparent_transform(obs_mode, temporal_samples_[0], temporal_samples_[i], obs_ssb);

                Transform<double> local_apparent = obs_ssb.inverse() * instance_ssb;

                // Down-cast to single precision once in local space:
                render_transforms[i] = static_cast<Transform<float>>(local_apparent);
            }

            const auto& asset_var = instance->asset();
            std::visit([&](auto* raw_ptr) noexcept {
                handle_asset_ptr_(raw_ptr, render_transforms);
                }, asset_var);
        }

        for (const auto& child : node->get_children()) {
            traverse_and_collect_(child, observer_transforms, obs_mode);
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
        // TODO Is it valid to store just one light temporal sample?

        LightInstance<TSpectral> instance;
        instance.light = light;
        instance.transform = instance_apparent_transforms[0];

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

        // For each mesh batch (unique mesh + list of instance transforms):
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

                // Attach to TLAS — the returned ID is the geomID we'll see in ray hits:
                unsigned int geom_id = rtcAttachGeometry(tlas_, inst_geom);
                rtcReleaseGeometry(inst_geom);

                // Record the mapping so we can resolve hits:
                if (geom_id >= instance_mappings_.size()) {
                    instance_mappings_.resize(geom_id + 1);
                }
                instance_mappings_[geom_id] = { batch_idx, inst_idx };
            }
        }

        rtcCommitScene(tlas_);
    }
}
