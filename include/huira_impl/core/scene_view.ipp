#include <memory>
#include <vector>

#include "huira/assets/mesh.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/core/scene.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/handles/camera_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    SceneView<TSpectral>::SceneView(const Scene<TSpectral>& scene,
        const Time& t_obs,
        const InstanceHandle<TSpectral>& camera_instance,
        ObservationMode obs_mode)
    {
        auto camera_node = camera_instance.get();

        const auto& asset_var = camera_node->asset();
        if (!std::holds_alternative<CameraModel<TSpectral>*>(asset_var)) {
            HUIRA_THROW_ERROR("SceneView received an Instance for the observer that does not contain a CameraModel!");
        }

        this->camera_model_ = std::get<CameraModel<TSpectral>*>(asset_var)->shared_from_this();

        Transform<double> obs_ssb = camera_node->get_ssb_transform_(t_obs);

        HUIRA_LOG_INFO("Generating SceneView at time ET=" + std::to_string(t_obs.et()) +
            " for CameraModel[" + std::to_string(camera_model_->id()) + "] '" + camera_model_->name() + "'.");
        
        traverse_and_collect_(scene.root_node_, t_obs, obs_ssb, obs_mode);

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
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::traverse_and_collect_(const std::shared_ptr<Node<TSpectral>>& node,
        const Time& t_obs, const Transform<double> obs_ssb, ObservationMode obs_mode)
    {
        if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
            Transform<double> instance_ssb = node->get_apparent_transform(obs_mode, t_obs, obs_ssb);

            Transform<double> local_apparent = obs_ssb.inverse() * instance_ssb;

            // Down-cast to single precision once in local space:
            Transform<float> render_transform = static_cast<Transform<float>>(local_apparent);

            const auto& asset_var = instance->asset();
            std::visit([&](auto* raw_ptr) noexcept {
                handle_asset_ptr_(raw_ptr, render_transform);
                }, asset_var);
        }

        for (const auto& child : node->get_children()) {
            traverse_and_collect_(child, t_obs, obs_ssb, obs_mode);
        }
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::handle_asset_ptr_(Mesh<TSpectral>* mesh, const Transform<float>& xf)
    {
        add_mesh_instance_(mesh->shared_from_this(), xf);
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::handle_asset_ptr_(Light<TSpectral>* light, const Transform<float>& xf)
    {
        add_light_instance_(light->shared_from_this(), xf);
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::handle_asset_ptr_(CameraModel<TSpectral>* camera, const Transform<float>& xf)
    {
        (void)camera;
        (void)xf;
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::handle_asset_ptr_(UnresolvedObject<TSpectral>* light, const Transform<float>& xf)
    {
        add_unresolved_instance_(light->shared_from_this(), xf);
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::handle_asset_ptr_(Model<TSpectral>* model, const Transform<float>& instance_apparent_xf)
    {
        if (!model) {
            return;
        }
        std::shared_ptr<FrameNode<TSpectral>> model_graph_ptr = model->root_node_;
        traverse_model_graph_(model_graph_ptr, instance_apparent_xf);
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::add_mesh_instance_(std::shared_ptr<Mesh<TSpectral>> mesh, const Transform<float>& render_transform)
    {
        auto* key = mesh.get();
        auto it = batch_lookup_.find(key);

        if (it != batch_lookup_.end()) {
            size_t index = it->second;
            geometry_[index].instances.push_back(render_transform);
        }
        else {
            MeshBatch<TSpectral> batch;
            batch.mesh = mesh;
            batch.instances.push_back(render_transform);

            geometry_.push_back(std::move(batch));

            batch_lookup_[key] = geometry_.size() - 1;
        }
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::add_light_instance_(std::shared_ptr<Light<TSpectral>> light, const Transform<float>& render_transform)
    {
        LightInstance<TSpectral> instance;
        instance.light = light;
        instance.transform = render_transform;

        lights_.push_back(std::move(instance));
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::add_unresolved_instance_(std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object, const Transform<float>& render_transform)
    {
        UnresolvedInstance<TSpectral> instance;
        instance.unresolved_object = unresolved_object;
        instance.transform = render_transform;

        unresolved_objects_.push_back(std::move(instance));
    }

    template <IsSpectral TSpectral>
    void SceneView<TSpectral>::traverse_model_graph_(const std::shared_ptr<Node<TSpectral>> node, const Transform<float>& parent_tf)
    {
        Transform<float> current_tf = parent_tf * static_cast<Transform<float>>(node->local_transform_);

        if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(node)) {
            const auto& asset_var = instance->asset();
            std::visit([&](auto* raw_ptr) noexcept {
                handle_asset_ptr_(raw_ptr, current_tf);
                }, asset_var);
        }

        // 3. Recurse Rigidly
        for (const auto& child : node->get_children()) {
            traverse_model_graph_(child, current_tf);
        }
    }
}
