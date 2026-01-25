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
    SceneView<TSpectral>::SceneView(const Scene<TSpectral>& scene, const Time& t_obs, const CameraHandle<TSpectral>& camera, ObservationMode obs_mode)
    {
        auto camera_node = camera.get();
        Transform<double> obs_ssb = camera_node->get_ssb_transform_(t_obs);
        
        traverse_and_collect_(scene.root_node_, t_obs, obs_ssb, obs_mode);
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
            std::visit([&](auto* raw_ptr) {
                using T = std::remove_pointer_t<decltype(raw_ptr)>;

                if constexpr (std::is_same_v<T, Mesh<TSpectral>>) {
                    add_mesh_instance_(raw_ptr->shared_from_this(), render_transform);
                }
                else if constexpr (std::is_same_v<T, Light<TSpectral>>) {
                    add_light_instance_(raw_ptr->shared_from_this(), render_transform);
                }
                }, asset_var);
        }

        for (const auto& child : node->get_children()) {
            traverse_and_collect_(child, t_obs, obs_ssb, obs_mode);
        }
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

}
