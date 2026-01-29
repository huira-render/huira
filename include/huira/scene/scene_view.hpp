#pragma once

#include <memory>
#include <vector>
#include <cstddef>
#include <unordered_map>

#include "huira/assets/mesh.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/assets/unresolved_object.hpp"
#include "huira/scene/scene.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/handles/camera_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Renderer;

    template <IsSpectral TSpectral>
    struct MeshBatch {
        std::shared_ptr<const Mesh<TSpectral>> mesh;
        std::vector<Transform<float>> instances;
    };

    template <IsSpectral TSpectral>
    struct LightInstance {
        std::shared_ptr<const Light<TSpectral>> light;
        Transform<float> transform;
    };

    template <IsSpectral TSpectral>
    struct UnresolvedInstance {
        std::shared_ptr<const UnresolvedObject<TSpectral>> unresolved_object;
        Transform<float> transform;
    };

    template <IsSpectral TSpectral>
    class SceneView {
    public:
        SceneView(const Scene<TSpectral>& scene,
            const Time& time,
            const InstanceHandle<TSpectral>& camera_instance,
            ObservationMode obs_mode);

    private:
        void traverse_and_collect_(const std::shared_ptr<Node<TSpectral>>& node,
            const Time& t_obs, const Transform<double> obs_ssb, ObservationMode obs_mode);

        void handle_asset_ptr_(Mesh<TSpectral>* mesh, const Transform<float>& xf);
        void handle_asset_ptr_(Light<TSpectral>* light, const Transform<float>& xf);
        void handle_asset_ptr_(CameraModel<TSpectral>* camera, const Transform<float>& xf);
        void handle_asset_ptr_(UnresolvedObject<TSpectral>* light, const Transform<float>& xf);
        void handle_asset_ptr_(Model<TSpectral>* model, const Transform<float>& xf);

        void add_mesh_instance_(std::shared_ptr<Mesh<TSpectral>> mesh, const Transform<float>& render_transform);
        void add_light_instance_(std::shared_ptr<Light<TSpectral>> light, const Transform<float>& render_transform);
        void add_unresolved_instance_(std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object, const Transform<float>& render_transform);
        void traverse_model_graph_(const std::shared_ptr<Node<TSpectral>> node, const Transform<float>& parent_tf);

        std::shared_ptr<CameraModel<TSpectral>> camera_model_;

        std::vector<MeshBatch<TSpectral>> geometry_;
        std::unordered_map<const Mesh<TSpectral>*, std::size_t> batch_lookup_;

        std::vector<LightInstance<TSpectral>> lights_;

        std::vector<UnresolvedInstance<TSpectral>> unresolved_objects_;

        friend class Renderer<TSpectral>;
    };
}

#include "huira_impl/scene/scene_view.ipp"
