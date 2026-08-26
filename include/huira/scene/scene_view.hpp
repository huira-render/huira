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
 * @brief How partial surface opacity is resolved along a transmittance query.
 */
enum class AlphaMode {
    /// Russian-roulette on opacity: unbiased over many samples, one RNG draw per
    /// partially opaque surface crossed. Used by shadow rays, where the estimate is
    /// already averaged over the pixel's samples.
    Stochastic,
    /// Analytic expected value of the stochastic estimator, drawing no random
    /// numbers. Used for queries that are evaluated exactly once and therefore have
    /// no samples to average over - notably occlusion of unresolved point sources,
    /// where a coin flip would make a star flicker on and off behind thin geometry.
    Expected
};

/**
 * @brief View of a scene at a specific time and camera instance.
 *
 * SceneView collects geometry, lights, unresolved objects, and stars for rendering.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
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

    [[nodiscard]] TSpectral
    evaluate_transmittance(const Ray<TSpectral>& shadow_ray,
                           float t_far,
                           const MediumStack<TSpectral>& initial_stack,
                           RandomSampler<float>& sampler,
                           float time = 0.5f,
                           AlphaMode alpha_mode = AlphaMode::Stochastic) const;

    [[nodiscard]] Interaction<TSpectral> resolve_hit(const Ray<TSpectral>& ray,
                                                     const HitRecord& hit) const;

    [[nodiscard]] std::vector<HitRecord> intersect(const std::vector<Ray<TSpectral>>& rays,
                                                   float time = 0.5f) const;

    [[nodiscard]] std::vector<Interaction<TSpectral>>
    resolve_hits(const std::vector<Ray<TSpectral>>& rays, const std::vector<HitRecord>& hits) const;

    /// The light instances collected for this view.
    [[nodiscard]] const std::vector<LightInstance<TSpectral>>& lights() const { return lights_; }

    /// Sentinel returned by indirect_source_index() for hits that are not on an indirect source.
    static constexpr std::size_t NO_INDIRECT_SOURCE = std::numeric_limits<std::size_t>::max();

    /// The designated indirect illumination sources collected for this view.
    [[nodiscard]] const std::vector<IndirectSourceInstance<TSpectral>>& indirect_sources() const
    {
        return indirect_sources_;
    }

    [[nodiscard]] std::size_t indirect_source_index(const HitRecord& hit) const;

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

    void begin_indirect_source_(const std::vector<Transform<float>>& instance_apparent_transforms);

    void end_indirect_source_(const Instance<TSpectral>& instance);

    /// Populates each indirect source's world-space bounding spheres from its
    /// members' BLAS bounds and per-temporal-sample transforms.
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

    /// Index into indirect_sources_ of the source currently being collected by the
    /// traversal, or NO_INDIRECT_SOURCE outside a designated instance's sub-graph.
    std::size_t open_indirect_index_ = NO_INDIRECT_SOURCE;

    /**
     * @brief Apparent star directions over the exposure, in camera coordinates.
     *
     * Stored as a flat struct-of-arrays rather than a vector-of-vectors: a catalogue
     * of a few million stars is rebuilt on every view, and one heap allocation per
     * star costs far more than the arithmetic that fills it.
     *
     * @c directions holds @c sample_count entries per star, star-major, so star @c i
     * at temporal sample @c j is at index <tt>i * sample_count + j</tt>.
     *
     * @c irradiances holds one entry per star. A catalogue star's irradiance is
     * constant over the exposure - only its apparent direction moves - so there is
     * nothing to store per sample.
     */
    struct StarField {
        std::vector<Vec3<float>> directions;
        std::vector<TSpectral> irradiances;
        std::size_t sample_count = 0;

        [[nodiscard]] std::size_t size() const noexcept { return irradiances.size(); }
        [[nodiscard]] bool empty() const noexcept { return irradiances.empty(); }

        /// Directions for star @p i, contiguous, @c sample_count long.
        [[nodiscard]] const Vec3<float>* directions_for(std::size_t i) const noexcept
        {
            return directions.data() + i * sample_count;
        }
    };

    StarField stars_;

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
