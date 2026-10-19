#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
#include "huira/scene/object_ids.hpp"
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
    /// How far, by default, the path a thing is drawn along may stray from its true path during
    /// the exposure, in pixels, when the number of temporal samples is chosen automatically.
    static constexpr double DEFAULT_MOTION_TOLERANCE = 0.1;

    SceneView(const Scene<TSpectral>& scene,
              const Interval& exposure_interval,
              const InstanceHandle<TSpectral>& camera_instance,
              ObservationMode obs_mode = ObservationMode::ABERRATED_STATE,
              std::optional<std::size_t> num_temporal_samples = std::nullopt,
              double motion_tolerance = DEFAULT_MOTION_TOLERANCE);

    ~SceneView();

    // A scene view owns its Embree scene, which it releases when it is destroyed: a copy would
    // release it a second time. It is built where it is used, and neither copied nor moved.
    SceneView(const SceneView&) = delete;
    SceneView& operator=(const SceneView&) = delete;
    SceneView(SceneView&&) = delete;
    SceneView& operator=(SceneView&&) = delete;

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

    [[nodiscard]] ObjectIds object_ids(const HitRecord& hit) const;

    [[nodiscard]] TSpectral direct_lit_radiance(
        const Ray<TSpectral>& ray,
        const HitRecord& hit,
        RandomSampler<float>& sampler,
        float time = 0.5f,
        const MediumStack<TSpectral>& medium_stack = MediumStack<TSpectral>{}) const;

    /// True when the TLAS contains nothing that a ray could ever hit.
    [[nodiscard]] bool tlas_is_empty() const noexcept { return tlas_empty_; }

    /// Whether anything rays can hit (geometry and lights) moves in view during the exposure, as
    /// the view samples it: rays are then traced at times through the exposure.
    [[nodiscard]] bool has_motion_blur() const noexcept { return has_motion_blur_; }

    /// The most temporal samples anything in the view was given (1 when nothing moves).
    [[nodiscard]] std::size_t max_temporal_samples() const noexcept
    {
        return max_temporal_samples_;
    }

    /// Conservative camera-space bounding sphere of one TLAS occupant.
    struct OccupancyBound {
        Vec3<float> center; ///< Camera-frame centre.
        float radius;       ///< Radius enclosing the occupant over the time it covers.
    };

    /// Per-occupant bounds for every instance in the TLAS, over the exposure.
    [[nodiscard]] const std::vector<OccupancyBound>& occupancy_bounds() const noexcept
    {
        return occupancy_bounds_;
    }

    /// False when the bounds are not usable for culling.
    [[nodiscard]] bool occupancy_bounds_complete() const noexcept
    {
        return occupancy_bounds_complete_;
    }

    /// The scene background, sampled by rays that hit nothing.
    [[nodiscard]] const std::shared_ptr<Image<TSpectral>>& background() const
    {
        return background_;
    }

    Interval get_exposure_interval() const { return exposure_interval_; }
    units::Second duration() const { return exposure_interval_.duration(); }
    Time get_time() const { return exposure_interval_.center(); }
    Time get_start_time() const { return exposure_interval_.start; }
    Time get_end_time() const { return exposure_interval_.end; }

  private:
    Interval exposure_interval_;

    // Temporal sampling. Each thing in view (each instance, and the star field) has its own
    // samples, evenly spaced through the exposure: one, at its middle, for something that does
    // not move in view, and otherwise as many as its motion needs (see choose_fractions_()), or
    // the number asked for (one with the camera's motion blur off). Times are given as fractions
    // of the exposure, from 0 to 1.
    std::optional<std::size_t> requested_temporal_samples_;
    double motion_tolerance_ = DEFAULT_MOTION_TOLERANCE;
    ObservationMode obs_mode_ = ObservationMode::ABERRATED_STATE;
    const Node<TSpectral>* camera_node_ = nullptr;
    double pixels_per_radian_ = 1.0;
    bool has_motion_blur_ = false;
    std::size_t max_temporal_samples_ = 1;

    /// The most samples one thing is given automatically, and the star field, whose memory grows
    /// with its samples times the number of stars.
    static constexpr std::size_t MAX_AUTOMATIC_SAMPLES = RTC_MAX_TIME_STEP_COUNT; // Embree's
    static constexpr std::size_t MAX_AUTOMATIC_STAR_SAMPLES = 17;

    /// The camera's pose in the solar system barycentric frame, and its inverse, at a time.
    struct ObserverPose {
        Transform<double> ssb;
        Transform<double> inverse;
    };
    std::map<double, ObserverPose> observer_poses_;
    const ObserverPose& observer_at_(double fraction);
    [[nodiscard]] Time time_at_(double fraction) const;
    [[nodiscard]] static std::vector<double> uniform_fractions_(std::size_t count);

    /// Where the renderer draws something's probes halfway between two of its temporal samples,
    /// given the fractions of the exposure at the two and halfway between them. Empty for point
    /// sources and the stars, which are drawn along a curve fitted through their samples (see
    /// TrajectoryArc).
    using DrawnHalfway =
        std::function<std::vector<Vec3<double>>(double start, double halfway, double end)>;

    /// How closely a light's distance is followed, relative to it, when its samples are chosen:
    /// its light falls off as the square of the distance, so this keeps it within 0.1%.
    static constexpr double LIGHT_DISTANCE_TOLERANCE = 5e-4;

    std::vector<double>
    choose_fractions_(const std::function<const std::vector<Vec3<double>>&(double)>& probes_at,
                      const std::function<double()>& turn_rate,
                      const DrawnHalfway& drawn_halfway,
                      std::size_t max_samples,
                      const std::string& what,
                      double distance_tolerance = std::numeric_limits<double>::infinity()) const;

    /// Points whose motion stands for an instance's asset's (see local_probes_()).
    struct LocalProbes {
        /// The center of its bounds, in the instance's frame.
        Vec3<double> center{0.0};

        /// A part of it that the renderer moves on its own: its placement in the instance's
        /// frame, and points in its own frame.
        struct Part {
            Transform<double> placement;
            std::vector<Vec3<double>> points;
        };
        std::vector<Part> parts;
    };
    [[nodiscard]] LocalProbes local_probes_(const Instance<TSpectral>& instance) const;

    /// An instance's transforms in the camera's frame at its temporal samples.
    std::vector<Transform<float>> sample_instance_(const Instance<TSpectral>& instance);

    /// Where Embree draws something halfway between two of its poses: its position and scale
    /// halfway along straight lines, and its rotation halfway along the shorter arc (see
    /// instance_transform_at()).
    [[nodiscard]] static Transform<double> halfway_pose_(const Transform<double>& start,
                                                         const Transform<double>& end);

    /// The angle between two directions (which need not be unit vectors), precise at any size.
    [[nodiscard]] static double angle_between_(const Vec3<double>& a, const Vec3<double>& b)
    {
        return std::atan2(glm::length(glm::cross(a, b)), glm::dot(a, b));
    }

    template <typename TMaterial, typename TParams>
    TSpectral sample_light_contribution_(const LightInstance<TSpectral>& light_instance,
                                         const Interaction<TSpectral>& isect,
                                         const TMaterial* material,
                                         const TParams& params,
                                         const Interaction<TSpectral>& shading_isect,
                                         const MediumStack<TSpectral>& medium_stack,
                                         RandomSampler<float>& sampler,
                                         float time) const;

    void traverse_and_collect_(const std::shared_ptr<Node<TSpectral>>& node);

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

    void collect_hidden_(const std::shared_ptr<Node<TSpectral>>& node);

    /// Assets reachable only through a hidden node. Kept so that the unlinked-object
    /// check can tell a hidden asset apart from one that was never attached.
    std::unordered_set<const void*> hidden_assets_;

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

    /// The model, instance and part the traversal is currently inside, which every primitive
    /// instance collected is recorded with (its geometry is the primitive's own).
    ObjectIds open_ids_;

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

    /// When anything moves, things that do not are given two identical poses, so that Embree
    /// keeps every instance in one BVH (see build_tlas_()), unless they outnumber what moves by
    /// more than this.
    static constexpr std::size_t MOST_STILL_PER_MOVING_IN_ONE_BVH = 64;

    void set_instance_poses_(RTCGeometry geometry,
                             const std::vector<Transform<float>>& transforms,
                             bool still_given_two_poses) const;

    std::shared_ptr<EmbreeDevice> device_ = nullptr;
    RTCScene tlas_ = nullptr;
    bool tlas_empty_ = true;

    std::vector<OccupancyBound> occupancy_bounds_;
    bool occupancy_bounds_complete_ = true;

    void compute_occupancy_bounds_();

    /// An occupant's bound at one temporal sample, and what Embree interpolates it from.
    struct SampleBound {
        Vec3<float> center;         ///< Camera-frame center.
        float radius;               ///< Radius enclosing the occupant.
        float lever;                ///< Distance from the instance's origin to the center.
        Quaternion<float> rotation; ///< The instance's rotation.
    };
    bool push_swept_bounds_(const std::vector<SampleBound>& sample_bounds);

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
