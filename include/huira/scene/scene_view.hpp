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
#include "huira/render/interaction.hpp"
#include "huira/render/ray.hpp"
#include "huira/scene/scene.hpp"
#include "huira/scene/scene_view_types.hpp"


namespace huira {
    template <IsSpectral TSpectral>
    class Renderer;

    enum class GeometryType {
        Atmosphere,
        Mesh,
        Light
    };

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
            std::size_t num_temporal_samples = 1);

        ~SceneView();

        [[nodiscard]] HitRecord intersect(const Ray<TSpectral>& ray, float time = 0.5f) const;

        [[nodiscard]] bool occluded(const Ray<TSpectral>& ray, float t_far, float time = 0.5f) const;

        [[nodiscard]] Interaction<TSpectral> resolve_hit(
            const Ray<TSpectral>& ray,
            const HitRecord& hit) const;

        [[nodiscard]] std::vector<HitRecord> intersect(const std::vector<Ray<TSpectral>>& rays, float time = 0.5f) const;

        [[nodiscard]] std::vector<bool> occluded(const std::vector<Ray<TSpectral>>& rays, float t_far, float time = 0.5f) const;

        [[nodiscard]] std::vector<Interaction<TSpectral>> resolve_hits(
            const std::vector<Ray<TSpectral>>& rays,
            const std::vector<HitRecord>& hits) const;

        Interval get_exposure_interval() const { return exposure_interval_; }
        units::Second duration() const { return exposure_interval_.duration(); }
        Time get_time() const { return exposure_interval_.center(); }
        Time get_start_time() const { return exposure_interval_.start; }
        Time get_end_time() const { return exposure_interval_.end; }

    private:
        Interval exposure_interval_;
        std::vector<Time> temporal_samples_;

        void traverse_and_collect_(const std::shared_ptr<Node<TSpectral>>& node,
            const std::vector<Transform<double>>& observer_transforms,
            const std::vector<Transform<double>>& observer_inverses, ObservationMode obs_mode);

        void handle_asset_ptr_(Atmosphere<TSpectral>* atmosphere, const std::vector<Transform<float>>& instance_apparent_transforms);
        void handle_asset_ptr_(Mesh<TSpectral>* mesh, const std::vector<Transform<float>>& instance_apparent_transforms);
        void handle_asset_ptr_(Light<TSpectral>* light, const std::vector<Transform<float>>& instance_apparent_transforms);
        void handle_asset_ptr_(CameraModel<TSpectral>* camera, const std::vector<Transform<float>>& instance_apparent_transforms);
        void handle_asset_ptr_(UnresolvedObject<TSpectral>* light, const std::vector<Transform<float>>& instance_apparent_transforms);
        void handle_asset_ptr_(Model<TSpectral>* model, const std::vector<Transform<float>>& instance_apparent_transforms);

        void add_atmosphere_instance_(std::shared_ptr<Atmosphere<TSpectral>> atmosphere, const std::vector<Transform<float>>& instance_apparent_transforms);
        void add_mesh_instance_(std::shared_ptr<Mesh<TSpectral>> mesh, const std::vector<Transform<float>>& instance_apparent_transforms);
        void add_light_instance_(std::shared_ptr<Light<TSpectral>> light, const std::vector<Transform<float>>& instance_apparent_transforms);
        void add_unresolved_instance_(std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object, const std::vector<Transform<float>>& instance_apparent_transforms);

        void traverse_model_graph_(const std::shared_ptr<Node<TSpectral>> node, const std::vector<Transform<float>>& parent_transform);

        std::shared_ptr<CameraModel<TSpectral>> camera_model_;
        std::vector<Transform<float>> camera_to_world_;

        std::vector<AtmosphereInstance<TSpectral>> atmospheres_;

        std::vector<MeshBatch<TSpectral>> geometry_;
        std::unordered_map<const Mesh<TSpectral>*, std::size_t> batch_lookup_;

        std::vector<LightInstance<TSpectral>> lights_;

        std::vector<UnresolvedInstance<TSpectral>> unresolved_objects_;

        std::vector<std::vector<Star<TSpectral>>> stars_;

        std::shared_ptr<Image<TSpectral>> background_;

        void build_tlas_();

        std::shared_ptr<EmbreeDevice> device_ = nullptr;
        RTCScene tlas_ = nullptr;

        uint32_t MASK_GEOMETRY_ = 0x01;
        uint32_t MASK_LIGHT_ = 0x02;

        struct InstanceMapping {
            GeometryType type;
            std::size_t batch_index;      // Index into geometry_ if type == Mesh
            std::size_t instance_index;   // Index into geometry_[batch_index].instances if type == Mesh

            std::size_t light_index;      // Index into lights_ if type == Light

            std::size_t atmosphere_index; // Index into atmospheres_ if type == Atmosphere
        };
        std::vector<InstanceMapping> instance_mappings_;


        struct AlphaFilterContext {
            const Material<TSpectral>* material;
            const Mesh<TSpectral>* mesh;
        };
        std::vector<std::unique_ptr<AlphaFilterContext>> filter_contexts_;
        static void alpha_occlusion_filter_(const RTCFilterFunctionNArguments* args);
        static void alpha_intersection_filter_(const RTCFilterFunctionNArguments* args);

        friend class Renderer<TSpectral>;
    };
}

#include "huira_impl/scene/scene_view.ipp"
