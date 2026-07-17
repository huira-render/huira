#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "embree4/rtcore.h"
#include "huira/assets/lights/light.hpp"
#include "huira/assets/primitive.hpp"
#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/core/interval.hpp"
#include "huira/core/transform.hpp"
#include "huira/geometry/ray.hpp"
#include "huira/handles/camera_handle.hpp"
#include "huira/render/interaction.hpp"
#include "huira/scene/indirect_source_instance.hpp"
#include "huira/scene/scene.hpp"
#include "huira/scene/scene_view_types.hpp"
#include "huira/units/units.hpp"
#include "huira/volumes/medium_stack.hpp"

namespace huira {
template <IsSpectral TSpectral>
class Renderer;

enum class GeometryType { Primitive, Light };

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

    [[nodiscard]] HitRecord
    intersect(const Ray<TSpectral>& ray, float time = 0.5f, unsigned int mask = 0xFFFFFFFF) const;

    [[nodiscard]] TSpectral evaluate_transmittance(const Ray<TSpectral>& shadow_ray,
                                                   float t_far,
                                                   const MediumStack<TSpectral>& initial_stack,
                                                   RandomSampler<float>& sampler,
                                                   float time = 0.5f) const;

    [[nodiscard]] Interaction<TSpectral> resolve_hit(const Ray<TSpectral>& ray,
                                                     const HitRecord& hit) const;

    [[nodiscard]] std::vector<HitRecord> intersect(const std::vector<Ray<TSpectral>>& rays,
                                                   float time = 0.5f) const;

    [[nodiscard]] std::vector<Interaction<TSpectral>>
    resolve_hits(const std::vector<Ray<TSpectral>>& rays, const std::vector<HitRecord>& hits) const;

    /// The light instances collected for this view.
    [[nodiscard]] const std::vector<LightInstance<TSpectral>>& lights() const { return lights_; }

    /// Sentinel returned by indirect_source_index() for hits that are not on a
    /// designated indirect source.
    static constexpr std::size_t NO_INDIRECT_SOURCE = std::numeric_limits<std::size_t>::max();

    /// The designated indirect illumination sources collected for this view.
    [[nodiscard]] const std::vector<IndirectSourceInstance<TSpectral>>& indirect_sources() const
    {
        return indirect_sources_;
    }

    /**
     * @brief Returns the indirect-source index of the instance a hit landed on.
     * @param hit The hit record to classify.
     * @return Index into indirect_sources(), or NO_INDIRECT_SOURCE if the hit is
     *         invalid or not on a designated indirect source.
     */
    [[nodiscard]] std::size_t indirect_source_index(const HitRecord& hit) const
    {
        if (!hit.hit()) {
            return NO_INDIRECT_SOURCE;
        }
        return instance_mappings_[hit.inst_id].indirect_index;
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
    [[nodiscard]] TSpectral direct_lit_radiance(
        const Ray<TSpectral>& ray,
        const HitRecord& hit,
        RandomSampler<float>& sampler,
        float time = 0.5f,
        const MediumStack<TSpectral>& medium_stack = MediumStack<TSpectral>{}) const;

    Interval get_exposure_interval() const { return exposure_interval_; }
    units::Second duration() const { return exposure_interval_.duration(); }
    Time get_time() const { return exposure_interval_.center(); }
    Time get_start_time() const { return exposure_interval_.start; }
    Time get_end_time() const { return exposure_interval_.end; }

  private:
    Interval exposure_interval_;
    std::vector<Time> temporal_samples_;

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
    template <typename TMaterial, typename TParams>
    TSpectral sample_light_contribution_(const LightInstance<TSpectral>& light_instance,
                                         const Interaction<TSpectral>& isect,
                                         const TMaterial* material,
                                         const TParams& params,
                                         const Interaction<TSpectral>& shading_isect,
                                         const MediumStack<TSpectral>& medium_stack,
                                         RandomSampler<float>& sampler,
                                         float time) const;

    void traverse_and_collect_(const std::shared_ptr<Node<TSpectral>>& node,
                               const std::vector<Transform<double>>& observer_transforms,
                               const std::vector<Transform<double>>& observer_inverses,
                               ObservationMode obs_mode);

    void handle_asset_ptr_(Primitive<TSpectral>* primitive,
                           const std::vector<Transform<float>>& instance_apparent_transforms);
    void handle_asset_ptr_(Light<TSpectral>* light,
                           const std::vector<Transform<float>>& instance_apparent_transforms);
    void handle_asset_ptr_(CameraModel<TSpectral>* camera,
                           const std::vector<Transform<float>>& instance_apparent_transforms);
    void handle_asset_ptr_(UnresolvedObject<TSpectral>* light,
                           const std::vector<Transform<float>>& instance_apparent_transforms);
    void handle_asset_ptr_(Model<TSpectral>* model,
                           const std::vector<Transform<float>>& instance_apparent_transforms);

    void add_primitive_instance_(std::shared_ptr<Primitive<TSpectral>> primitive,
                                 const std::vector<Transform<float>>& instance_apparent_transforms);
    void add_light_instance_(std::shared_ptr<Light<TSpectral>> light,
                             const std::vector<Transform<float>>& instance_apparent_transforms);
    void
    add_unresolved_instance_(std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object,
                             const std::vector<Transform<float>>& instance_apparent_transforms);
    void add_indirect_source_instance_(
        std::shared_ptr<Primitive<TSpectral>> primitive,
        const std::vector<Transform<float>>& instance_apparent_transforms);

    /// Populates each indirect source's world-space bounding spheres from its
    /// primitive's BLAS bounds and per-temporal-sample transforms.
    void compute_indirect_source_bounds_();

    void traverse_model_graph_(const std::shared_ptr<Node<TSpectral>> node,
                               const std::vector<Transform<float>>& parent_transform);

    std::shared_ptr<CameraModel<TSpectral>> camera_model_;
    std::vector<Transform<float>> camera_to_world_;

    std::vector<PrimitiveBatch<TSpectral>> primitives_;
    std::unordered_map<const Primitive<TSpectral>*, std::size_t> batch_lookup_;

    std::vector<LightInstance<TSpectral>> lights_;

    std::vector<UnresolvedInstance<TSpectral>> unresolved_objects_;

    std::vector<IndirectSourceInstance<TSpectral>> indirect_sources_;

    std::vector<std::vector<Star<TSpectral>>> stars_;

    std::shared_ptr<Image<TSpectral>> background_;

    void build_tlas_();

    std::shared_ptr<EmbreeDevice> device_ = nullptr;
    RTCScene tlas_ = nullptr;

    uint32_t MASK_GEOMETRY_ = 0x01;
    uint32_t MASK_LIGHT_ = 0x02;

    struct InstanceMapping {
        GeometryType type;
        std::size_t batch_index; // Index into geometry_ if type == Primitive
        std::size_t
            instance_index; // Index into geometry_[batch_index].instances if type == Primitive

        std::size_t light_index; // Index into lights_ if type == Light

        /// Index into indirect_sources_ if this instance is a designated
        /// indirect source; NO_INDIRECT_SOURCE otherwise.
        std::size_t indirect_index = NO_INDIRECT_SOURCE;
    };
    std::vector<InstanceMapping> instance_mappings_;

    friend class Renderer<TSpectral>;
};
} // namespace huira

#include "huira_impl/scene/scene_view.ipp"
