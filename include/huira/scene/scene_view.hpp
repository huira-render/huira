#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

#include "embree4/rtcore.h"

#include "huira/assets/lights/light.hpp"
#include "huira/assets/mesh.hpp"
#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/core/units/units.hpp"
#include "huira/core/interval.hpp"
#include "huira/core/transform.hpp"
#include "huira/handles/camera_handle.hpp"
#include "huira/scene/scene.hpp"
#include "huira/scene/scene_view_types.hpp"


namespace huira {
    template <IsSpectral TSpectral>
    class Renderer;

    /**
     * @brief View of a scene at a specific time and camera instance.
     *
     * SceneView collects geometry, lights, unresolved objects, and stars for rendering.
     *
     * @tparam TSpectral Spectral type (e.g., RGB, Spectral)
     */
    template <IsSpectral TSpectral>
    class SceneView {
    public:
        SceneView(const Scene<TSpectral>& scene,
            const Interval& exposure_interval,
            const InstanceHandle<TSpectral>& camera_instance,
            ObservationMode obs_mode,
            bool motion_blur = false,
            std::size_t num_temporal_Samples = 3);

        ~SceneView();

        Interval get_exposure_interval() const { return exposure_interval_; }
        units::Second duration() const { return exposure_interval_.duration(); }
        Time get_time() const { return exposure_interval_.center(); }
        Time get_start_time() const { return exposure_interval_.start; }
        Time get_end_time() const { return exposure_interval_.end; }

    private:
        Interval exposure_interval_;
        std::vector<Time> temporal_samples_;

        void traverse_and_collect_(const std::shared_ptr<Node<TSpectral>>& node,
            const std::vector<Transform<double>>& observer_transforms, ObservationMode obs_mode);

        void handle_asset_ptr_(Mesh<TSpectral>* mesh, const std::vector<Transform<float>>& instance_apparent_transforms);
        void handle_asset_ptr_(Light<TSpectral>* light, const std::vector<Transform<float>>& instance_apparent_transforms);
        void handle_asset_ptr_(CameraModel<TSpectral>* camera, const std::vector<Transform<float>>& instance_apparent_transforms);
        void handle_asset_ptr_(UnresolvedObject<TSpectral>* light, const std::vector<Transform<float>>& instance_apparent_transforms);
        void handle_asset_ptr_(Model<TSpectral>* model, const std::vector<Transform<float>>& instance_apparent_transforms);

        void add_mesh_instance_(std::shared_ptr<Mesh<TSpectral>> mesh, const std::vector<Transform<float>>& instance_apparent_transforms);
        void add_light_instance_(std::shared_ptr<Light<TSpectral>> light, const std::vector<Transform<float>>& instance_apparent_transforms);
        void add_unresolved_instance_(std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object, const std::vector<Transform<float>>& instance_apparent_transforms);

        void traverse_model_graph_(const std::shared_ptr<Node<TSpectral>> node, const std::vector<Transform<float>>& parent_transform);

        std::shared_ptr<CameraModel<TSpectral>> camera_model_;

        std::vector<MeshBatch<TSpectral>> geometry_;
        std::unordered_map<const Mesh<TSpectral>*, std::size_t> batch_lookup_;

        std::vector<LightInstance<TSpectral>> lights_;

        std::vector<UnresolvedInstance<TSpectral>> unresolved_objects_;

        std::vector<std::vector<Star<TSpectral>>> stars_;

        void build_tlas_();

        RTCDevice device_ = nullptr;
        RTCScene tlas_ = nullptr;
        struct InstanceMapping {
            std::size_t batch_index;      // Index into geometry_
            std::size_t instance_index;   // Index into geometry_[batch_index].instances
        };
        std::vector<InstanceMapping> instance_mappings_;

        friend class Renderer<TSpectral>;
    };
}

#include "huira_impl/scene/scene_view.ipp"
