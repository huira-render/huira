#include <memory>
#include <string>

#include "glm/glm.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/physics.hpp"
#include "huira/core/spice.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
#include "huira/scene/state_callbacks/keplerian_orbit.hpp"
#include "huira/scene/state_callbacks/nadir_point.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/validate.hpp"

namespace huira {

/**
 * @brief Construct a Node and assign a unique ID.
 * @param scene Pointer to the owning Scene
 */
template <IsSpectral TSpectral>
Node<TSpectral>::Node(Scene<TSpectral>* scene) : scene_(scene)
{
}

/**
 * @brief Set the node's position manually.
 * @param position Position vector in meters
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_position(const Vec3<double>& position)
{
    if (this->position_must_be_spice_()) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - cannot manually set position when child has a spice_origin");
    }
    validate_real(position, this->get_info() + " position");

    this->local_transform_.position = position;
    this->position_mode_ = TransformMode::MANUAL_TRANSFORM;
    this->spice_origin_ = "";
}

/**
 * @brief Set the node's position manually using unit types.
 * @param x X position (meters)
 * @param y Y position (meters)
 * @param z Z position (meters)
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_position(units::Meter x, units::Meter y, units::Meter z)
{
    if (this->position_must_be_spice_()) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - cannot manually set position when child has a spice_origin");
    }

    HUIRA_LOG_INFO(this->get_info() + " - set_position(" + std::to_string(x.to_si()) + ", " +
                   std::to_string(y.to_si()) + ", " + std::to_string(z.to_si()) + ")");

    const Vec3<double> position{x.to_si(), y.to_si(), z.to_si()};
    validate_real(position, this->get_info() + " position");
    this->local_transform_.position = position;
    this->position_mode_ = TransformMode::MANUAL_TRANSFORM;
    this->spice_origin_ = "";
}

/**
 * @brief Set the node's rotation manually.
 * @param rotation Rotation object
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_rotation(const Rotation<double>& rotation)
{
    if (this->rotation_must_be_spice_()) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - cannot manually set rotation when child has a spice_frame");
    }

    HUIRA_LOG_INFO(this->get_info() + " - set_rotation(" + rotation.to_string());

    this->local_transform_.rotation = rotation;
    this->rotation_mode_ = TransformMode::MANUAL_TRANSFORM;
    this->spice_frame_ = "";
}

/**
 * @brief Set the node's scale.
 * @param scale Scale vector: each component finite and not 0 (a negative one mirrors).
 * @throws std::runtime_error if a component is 0, too small to invert, or not finite.
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_scale(const Vec3<double>& scale)
{
    HUIRA_LOG_INFO(this->get_info() + " - set_scale(" + std::to_string(scale[0]) + ", " +
                   std::to_string(scale[1]) + ", " + std::to_string(scale[2]) + ")");

    check_scale_(scale);
    this->local_transform_.scale = scale;
}

/// Throws unless each component of a scale is finite, and so is its inverse, which the inverse
/// transform takes: 0 would flatten the node.
template <IsSpectral TSpectral>
void Node<TSpectral>::check_scale_(const Vec3<double>& scale) const
{
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(scale[i]) || !std::isfinite(1.0 / scale[i])) {
            HUIRA_THROW_ERROR(
                this->get_info() +
                " - The scale must be finite and not 0 in each component: " + vec_to_string(scale));
        }
    }
}

/**
 * @brief Set the node's scale using individual values.
 * @param sx X scale
 * @param sy Y scale
 * @param sz Z scale
 * @throws std::runtime_error if one is 0, too small to invert, or not finite.
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_scale(double sx, double sy, double sz)
{
    HUIRA_LOG_INFO(this->get_info() + " - set_scale(" + std::to_string(sx) + ", " +
                   std::to_string(sy) + ", " + std::to_string(sz) + ")");

    const Vec3<double> scale{sx, sy, sz};
    check_scale_(scale);
    this->local_transform_.scale = scale;
}

/**
 * @brief Set the node's velocity manually.
 * @param velocity Velocity vector in m/s
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_velocity(const Vec3<double>& velocity)
{
    if (this->position_mode_ != TransformMode::MANUAL_TRANSFORM) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - cannot manually set velocity when node does not use manual position");
    }
    validate_real(velocity, this->get_info() + " velocity");

    this->local_transform_.velocity = velocity;
}

/**
 * @brief Set the node's velocity manually using unit types.
 * @param vx X velocity (m/s)
 * @param vy Y velocity (m/s)
 * @param vz Z velocity (m/s)
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_velocity(units::MetersPerSecond vx,
                                   units::MetersPerSecond vy,
                                   units::MetersPerSecond vz)
{
    if (this->position_mode_ != TransformMode::MANUAL_TRANSFORM) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - cannot manually set velocity when node does not use manual position");
    }

    HUIRA_LOG_INFO(this->get_info() + " - set_velocity(" + std::to_string(vx.to_si()) + ", " +
                   std::to_string(vy.to_si()) + ", " + std::to_string(vz.to_si()) + ")");

    const Vec3<double> velocity{vx.to_si(), vy.to_si(), vz.to_si()};
    validate_real(velocity, this->get_info() + " velocity");
    this->local_transform_.velocity = velocity;
}

/**
 * @brief Set the node's angular velocity manually, expressed in the PARENT frame.
 *
 * The angular velocity vector is interpreted in the axes of this node's parent
 * frame, and the node's orientation evolves as q(t) = delta(t) * q_0. This is
 * only an "inertial" rate if every ancestor of this node is itself static. To
 * command rates about the node's own axes (e.g. a roll about a camera
 * boresight), use set_body_angular_velocity() instead.
 *
 * @param angular_velocity Angular velocity vector in rad/s, parent-frame axes
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_angular_velocity(const Vec3<double>& angular_velocity)
{
    if (this->rotation_mode_ != TransformMode::MANUAL_TRANSFORM) {
        HUIRA_THROW_ERROR(
            this->get_info() +
            " - cannot manually set angular velocity when node does not use manual rotation");
    }
    validate_real(angular_velocity, this->get_info() + " angular velocity");

    this->local_transform_.angular_velocity = angular_velocity;
    this->body_frame_rates_ = false;
}

/**
 * @brief Set the node's angular velocity manually using unit types, expressed in
 * the PARENT frame.
 *
 * The (wx, wy, wz) components are interpreted in the axes of this node's parent
 * frame, and the node's orientation evolves as q(t) = delta(t) * q_0. This is
 * only an "inertial" rate if every ancestor of this node is itself static. To
 * command rates about the node's own axes (e.g. a roll about a camera
 * boresight), use set_body_angular_velocity() instead.
 *
 * @param wx X angular velocity about the parent-frame x-axis (rad/s)
 * @param wy Y angular velocity about the parent-frame y-axis (rad/s)
 * @param wz Z angular velocity about the parent-frame z-axis (rad/s)
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_angular_velocity(units::RadiansPerSecond wx,
                                           units::RadiansPerSecond wy,
                                           units::RadiansPerSecond wz)
{
    if (this->rotation_mode_ != TransformMode::MANUAL_TRANSFORM) {
        HUIRA_THROW_ERROR(
            this->get_info() +
            " - cannot manually set angular velocity when node does not use manual rotation");
    }

    HUIRA_LOG_INFO(this->get_info() + " - set_angular_velocity(" + std::to_string(wx.to_si()) +
                   ", " + std::to_string(wy.to_si()) + ", " + std::to_string(wz.to_si()) + ")");

    const Vec3<double> angular_velocity{wx.to_si(), wy.to_si(), wz.to_si()};
    validate_real(angular_velocity, this->get_info() + " angular velocity");
    this->local_transform_.angular_velocity = angular_velocity;
    this->body_frame_rates_ = false;
}

/**
 * @brief Set the node's angular velocity manually using unit types, expressed in
 * the node's own BODY frame.
 *
 * The (wx, wy, wz) components are interpreted in this node's own axes, and the
 * node's orientation evolves as q(t) = q_0 * delta(t). For a camera (OpenCV
 * convention, +z = boresight), a pure wz therefore produces a roll about the
 * boresight regardless of how the camera is oriented relative to its parent.
 * To command rates in the parent frame's axes, use set_angular_velocity()
 * instead.
 *
 * @param wx X-body angular velocity (rad/s)
 * @param wy Y-body angular velocity (rad/s)
 * @param wz Z-body angular velocity (rad/s)
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_body_angular_velocity(units::RadiansPerSecond wx,
                                                units::RadiansPerSecond wy,
                                                units::RadiansPerSecond wz)
{
    if (this->rotation_mode_ != TransformMode::MANUAL_TRANSFORM) {
        HUIRA_THROW_ERROR(
            this->get_info() +
            " - cannot manually set angular velocity when node does not use manual rotation");
    }

    HUIRA_LOG_INFO(this->get_info() + " - set_body_angular_velocity(" + std::to_string(wx.to_si()) +
                   ", " + std::to_string(wy.to_si()) + ", " + std::to_string(wz.to_si()) + ")");

    const Vec3<double> angular_velocity{wx.to_si(), wy.to_si(), wz.to_si()};
    validate_real(angular_velocity, this->get_info() + " angular velocity");
    this->local_transform_.angular_velocity = angular_velocity;
    this->body_frame_rates_ = true;
}

template <IsSpectral TSpectral>
void Node<TSpectral>::set_manual_transform(const Transform<double>& transform)
{
    if (this->rotation_must_be_spice_() || this->position_must_be_spice_()) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - cannot manually set transformation when child has a spice_frame");
    }

    HUIRA_LOG_INFO(this->get_info() + " - set_manual_transform(" + transform.to_string());

    // Its rotation was checked when it was made:
    validate_real(transform.position, this->get_info() + " position");
    validate_real(transform.velocity, this->get_info() + " velocity");
    validate_real(transform.angular_velocity, this->get_info() + " angular velocity");
    check_scale_(transform.scale);
    this->local_transform_ = transform;
    this->body_frame_rates_ = false;
}

/**
 * @brief Set the node's SPICE origin for ephemeris-based transforms.
 * @param spice_origin SPICE origin string
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_spice_origin(const std::string& spice_origin)
{
    if (!position_can_be_spice_()) {
        HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE progom: parent node (" +
                          parent_->get_info() + ") has manually set position");
    }
    HUIRA_LOG_INFO(this->get_info() + " - set_spice_origin('" + spice_origin + "')");

    this->spice_origin_ = spice_origin;
    this->position_mode_ = TransformMode::SPICE_TRANSFORM;
}

/**
 * @brief Set the node's SPICE frame for rotation-based transforms.
 * @param spice_frame SPICE frame string
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_spice_frame(const std::string& spice_frame)
{
    if (!rotation_can_be_spice_()) {
        HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE frame: parent node (" +
                          parent_->get_info() + ") has manually set rotation");
    }
    HUIRA_LOG_INFO(this->get_info() + " - set_spice_frame('" + spice_frame + "')");

    this->spice_frame_ = spice_frame;
    this->rotation_mode_ = TransformMode::SPICE_TRANSFORM;
}

/**
 * @brief Set both SPICE origin and frame for the node.
 * @param spice_origin SPICE origin string
 * @param spice_frame SPICE frame string
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_spice(const std::string& spice_origin, const std::string& spice_frame)
{
    if (!position_can_be_spice_()) {
        HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE progom: parent node (" +
                          parent_->get_info() + ") has manually set position");
    }
    if (!rotation_can_be_spice_()) {
        HUIRA_THROW_ERROR(this->get_info() + " - cannot set SPICE frame: parent node (" +
                          parent_->get_info() + ") has manually set rotation");
    }
    HUIRA_LOG_INFO(this->get_info() + " - set_spice('" + spice_origin + ", " + spice_frame + "')");

    this->spice_origin_ = spice_origin;
    this->spice_frame_ = spice_frame;
    this->position_mode_ = TransformMode::SPICE_TRANSFORM;
    this->rotation_mode_ = TransformMode::SPICE_TRANSFORM;
}

/**
 * @brief Place the node by a position callback of a class of one's own, made from the arguments
 * given.
 *
 * Its evaluate() gives the node's position and velocity in its parent's frame for a pose context
 * (see PositionCallback). It replaces the node's position source: a position set by hand, a SPICE
 * origin, or another callback.
 *
 * @tparam TCallback A class derived from PositionCallback.
 * @throws std::runtime_error if a child of the node is placed by SPICE, which places its children
 *         only under nodes placed by SPICE.
 */
template <IsSpectral TSpectral>
template <IsPositionCallback TCallback, typename... Args>
void Node<TSpectral>::set_custom_position_callback(Args&&... args)
{
    if (this->position_must_be_spice_()) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - cannot set a position source when child has a spice_origin");
    }
    position_callback_ = std::make_unique<TCallback>(std::forward<Args>(args)...);
    this->position_mode_ = TransformMode::POSITION_CALLBACK;
    this->spice_origin_ = "";
}

/**
 * @brief Place the node by a function of the pose context.
 *
 * The function is called with a PoseContext, whose time is when the position is wanted, and
 * returns the node's position in its parent's frame, in meters: a Vec3<double>, whose velocity is
 * then estimated from nearby times, or a PositionState (position and velocity, in meters per
 * second), or a PositionSample. It is called many times while a scene view is built, at times in
 * any order, and never while it renders. It must give the same answer for the same context.
 *
 * @throws std::runtime_error if a child of the node is placed by SPICE.
 */
template <IsSpectral TSpectral>
template <typename TFunction>
    requires std::invocable<const TFunction&, const PoseContext&>
void Node<TSpectral>::set_position_function(TFunction function)
{
    HUIRA_LOG_INFO(this->get_info() + " - set_position_function()");
    this->set_custom_position_callback<PositionFunctionCallback>(
        [function = std::move(function)](const PoseContext& context) {
            return detail::to_position_sample(function(context));
        },
        this->get_info());
}

/**
 * @brief Place the node by a table of times and positions, with or without velocities, in its
 * parent's frame (see PositionTableCallback).
 *
 * @param times When each entry is: at least two, each later than the one before.
 * @param positions The position at each time, in meters.
 * @param velocities The velocity at each time, in meters per second, or none (the default) to
 *        estimate them from the positions.
 * @throws std::runtime_error if the table is not valid, or a child of the node is placed by
 *         SPICE.
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_position_table(const std::vector<Time>& times,
                                         std::vector<Vec3<double>> positions,
                                         std::vector<Vec3<double>> velocities)
{
    HUIRA_LOG_INFO(this->get_info() + " - set_position_table(" + std::to_string(times.size()) +
                   " entries)");
    this->set_custom_position_callback<PositionTableCallback>(
        times, std::move(positions), std::move(velocities), this->get_info());
}

/**
 * @brief Move the node on a Keplerian orbit about its parent, by CSPICE's conics_c().
 *
 * @param semi_major_axis The semi-major axis: positive for an ellipse (eccentricity under 1),
 *        negative for a hyperbola (over 1).
 * @param eccentricity Non-negative, and not 1: a parabola has no semi-major axis.
 * @param inclination, raan, arg_periapsis, mean_anomaly Angles, finite.
 * @param epoch When the body is at mean_anomaly.
 * @param mu The central body's gravitational parameter, in km^3/s^2 as SPICE takes it; positive.
 * @throws std::runtime_error if any element is not finite, or they do not describe an orbit
 *         whose periapsis is a positive distance away, about a body of positive mass.
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_keplerian_orbit(units::Meter semi_major_axis,
                                          double eccentricity,
                                          units::Radian inclination,
                                          units::Radian raan,
                                          units::Radian arg_periapsis,
                                          units::Radian mean_anomaly,
                                          Time epoch,
                                          double mu)
{
    const double a = semi_major_axis.to_si();
    for (const double angle : {inclination.to_si(),
                               raan.to_si(),
                               arg_periapsis.to_si(),
                               mean_anomaly.to_si(),
                               epoch.et()}) {
        validate_real(angle, this->get_info() + " orbital angle or epoch");
    }
    // The periapsis, a (1 - e), is a positive distance for an ellipse with a > 0 and for a
    // hyperbola with a < 0:
    if (!(eccentricity >= 0.0) || !std::isfinite(eccentricity) || eccentricity == 1.0 ||
        !std::isfinite(a) || !(a * (1.0 - eccentricity) > 0.0)) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - The orbit needs an eccentricity that is non-negative, finite and "
                          "not 1, and a semi-major axis positive for an ellipse and negative for "
                          "a hyperbola: " +
                          std::to_string(a) + " m, eccentricity " + std::to_string(eccentricity));
    }
    validate_strictly_positive(mu, this->get_info() + " gravitational parameter");
    this->set_custom_position_callback<KeplerianOrbitCallback>(
        semi_major_axis, eccentricity, inclination, raan, arg_periapsis, mean_anomaly, epoch, mu);
}

/**
 * @brief Orient the node by a rotation callback of a class of one's own, made from the arguments
 * given.
 *
 * Its evaluate() gives the node's rotation and angular velocity relative to its parent for a
 * pose context and the node's position (see RotationCallback). It replaces the node's rotation
 * source: a rotation set by hand, a SPICE frame, or another callback, such as look_at().
 *
 * @tparam TCallback A class derived from RotationCallback.
 * @throws std::runtime_error if a child of the node is oriented by SPICE, which orients its
 *         children only under nodes oriented by SPICE.
 */
template <IsSpectral TSpectral>
template <IsRotationCallback TCallback, typename... Args>
void Node<TSpectral>::set_custom_rotation_callback(Args&&... args)
{
    if (this->rotation_must_be_spice_()) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - cannot set a rotation source when child has a spice_frame");
    }
    rotation_callback_ = std::make_unique<TCallback>(std::forward<Args>(args)...);
    this->rotation_mode_ = TransformMode::ROTATION_CALLBACK;
    this->spice_frame_ = "";
}

/**
 * @brief Orient the node by a function of the pose context.
 *
 * The function is called with a PoseContext, whose time is when the rotation is wanted, and
 * returns the node's rotation relative to its parent: a Rotation<double>, whose angular velocity
 * is then estimated from nearby times, or a RotationState (rotation and angular velocity, in
 * radians per second about the parent's axes), or a RotationSample. It is called many times while
 * a scene view is built, at times in any order, and never while it renders. It must give the same
 * answer for the same context.
 *
 * @throws std::runtime_error if a child of the node is oriented by SPICE.
 */
template <IsSpectral TSpectral>
template <typename TFunction>
    requires std::invocable<const TFunction&, const PoseContext&>
void Node<TSpectral>::set_rotation_function(TFunction function)
{
    HUIRA_LOG_INFO(this->get_info() + " - set_rotation_function()");
    this->set_custom_rotation_callback<RotationFunctionCallback>(
        [function = std::move(function)](const PoseContext& context) {
            return detail::to_rotation_sample(function(context));
        },
        this->get_info());
}

/**
 * @brief Orient the node by a table of times and rotations, with or without angular velocities,
 * relative to its parent (see RotationTableCallback).
 *
 * @param times When each entry is: at least two, each later than the one before.
 * @param rotations The rotation at each time, from the node's frame to its parent's.
 * @param angular_velocities The angular velocity at each time, in radians per second about the
 *        parent's axes, or none (the default) to take the steady rate between entries.
 * @throws std::runtime_error if the table is not valid, or a child of the node is oriented by
 *         SPICE.
 */
template <IsSpectral TSpectral>
void Node<TSpectral>::set_rotation_table(const std::vector<Time>& times,
                                         std::vector<Rotation<double>> rotations,
                                         std::vector<Vec3<double>> angular_velocities)
{
    HUIRA_LOG_INFO(this->get_info() + " - set_rotation_table(" + std::to_string(times.size()) +
                   " entries)");
    this->set_custom_rotation_callback<RotationTableCallback>(
        times, std::move(rotations), std::move(angular_velocities), this->get_info());
}

template <IsSpectral TSpectral>
void Node<TSpectral>::set_z_up_y_forward_callback()
{
    this->set_custom_rotation_callback<ZUpYForwardCallback>();
}

template <IsSpectral TSpectral>
void Node<TSpectral>::set_z_down_y_forward_callback()
{
    this->set_custom_rotation_callback<ZDownYForwardCallback>();
}

template <IsSpectral TSpectral>
template <IsTransformCallback TCallback, typename... Args>
void Node<TSpectral>::set_custom_state_callback(Args&&... args)
{
    transform_callback_ = std::make_unique<TCallback>(std::forward<Args>(args)...);
    this->position_mode_ = TransformMode::TRANSFORM_CALLBACK;
    this->rotation_mode_ = TransformMode::TRANSFORM_CALLBACK;
    this->spice_origin_ = "";
    this->spice_frame_ = "";
}

/**
 * @brief Get the apparent transform of the node for a given observation mode and time.
 * @param obs_mode Observation mode (true, geometric, aberrated)
 * @param epoch Time that the manually set transform corresponds to
 * @param t_obs Observation time
 * @param observer_ssb_state Observer's SSB transform
 * @return Transform<double> Apparent transform
 */
template <IsSpectral TSpectral>
Transform<double>
Node<TSpectral>::get_apparent_transform(ObservationMode obs_mode,
                                        const Time& epoch,
                                        const Time& t_obs,
                                        const Transform<double>& observer_ssb_state) const
{
    const PoseContext context{t_obs, epoch, obs_mode};
    auto [apparent_state, _] = get_geometric_state_(context, observer_ssb_state);

    if (obs_mode == ObservationMode::ABERRATED_STATE) {
        // Geometric Direction:
        Vec3<double> P_ssb = apparent_state.position;
        Vec3<double> P_obs = observer_ssb_state.position;
        Vec3<double> P_rel = P_ssb - P_obs;

        double dist = glm::length(P_rel);

        // Safety check for degenerate geometry:
        if (dist > 1e-8) {
            Vec3<double> u = P_rel / dist;
            Vec3<double> u_app = compute_aberrated_direction(u, observer_ssb_state.velocity);
            apparent_state.position = P_obs + (u_app * dist);
        }
    }

    return apparent_state;
}

/**
 * @brief Get the geometric state (transform and light time) of the node.
 * @param context The observation time, the time poses set by hand hold at, and the observation
 *        mode: light time is solved for unless it is TRUE_STATE
 * @param observer_ssb_state Observer's SSB transform
 * @param tol Tolerance for iteration
 * @return std::pair<Transform<double>, double> {transform, light time}
 */
template <IsSpectral TSpectral>
std::pair<Transform<double>, double> Node<TSpectral>::get_geometric_state_(
    const PoseContext& context, const Transform<double>& observer_ssb_state, double tol) const
{
    if (context.observation_mode == ObservationMode::TRUE_STATE) {
        return {this->get_ssb_transform_(context), 0.0};
    }

    Transform<double> full_ssb_transform = this->get_ssb_transform_(context);
    double dt = glm::length(observer_ssb_state.position - full_ssb_transform.position) /
                SPEED_OF_LIGHT<double>();
    for (std::size_t i = 0; i < 10; ++i) {
        full_ssb_transform = this->get_ssb_transform_(context, dt);

        const double new_dt =
            glm::length(observer_ssb_state.position - full_ssb_transform.position) /
            SPEED_OF_LIGHT<double>();

        if (std::abs(new_dt - dt) < tol) {
            dt = new_dt;
            break;
        }

        dt = new_dt;
    }

    return {full_ssb_transform, dt};
}

/**
 * @brief Get the node's transform in the Solar System Barycenter (SSB) frame.
 * @param context The observation time, the time poses set by hand hold at, and the observation
 *        mode, which callbacks are given (at the emission time)
 * @param dt Light time delay
 * @return Transform<double> SSB transform
 */
template <IsSpectral TSpectral>
Transform<double> Node<TSpectral>::get_ssb_transform_(const PoseContext& context, double dt) const
{
    const Time& epoch = context.reference;
    const Time& t_obs = context.time;
    Time t_emit = Time::from_et(t_obs.et() - dt);
    const PoseContext emit_context = context.at(t_emit);

    // Fully SPICE-defined: no parent needed, compute directly against SSB
    if (position_mode_ == TransformMode::SPICE_TRANSFORM &&
        rotation_mode_ == TransformMode::SPICE_TRANSFORM) {
        Transform<double> ssb_state{};
        auto [pos, vel, _] =
            spice::spkezr<double>(this->spice_origin_, t_emit, "J2000", "NONE", "SSB");
        ssb_state.position = pos;
        ssb_state.velocity = vel;
        auto [rotation, ang_vel] = spice::sxform<double>("J2000", this->spice_frame_, t_emit);
        ssb_state.rotation = rotation;
        ssb_state.angular_velocity = ang_vel;
        ssb_state.scale = this->get_static_scale();
        return ssb_state;
    }

    // Any other configuration require a parent
    if (!parent_) {
        HUIRA_THROW_ERROR(
            this->get_info() +
            " - cannot compute SSB transform: non-SPICE transform mode but no parent");
    }
    Transform<double> parent_ssb = parent_->get_ssb_transform_(context, dt);

    // Full transform callback: single source defines the entire local state
    if (position_mode_ == TransformMode::TRANSFORM_CALLBACK &&
        rotation_mode_ == TransformMode::TRANSFORM_CALLBACK) {
        Transform<double> local = transform_callback_->evaluate(emit_context);
        local.scale = this->get_static_scale();
        Transform<double> ssb_state = parent_ssb * local;
        return ssb_state;
    }

    // Mixed case - Step 1:
    Transform<double> local{};
    if (position_mode_ == TransformMode::SPICE_TRANSFORM) {
        // Convert SPICE SSB result to local frame
        auto [pos, vel, _] =
            spice::spkezr<double>(this->spice_origin_, t_emit, "J2000", "NONE", "SSB");
        Transform<double> parent_inv = parent_ssb.inverse();
        local.position = parent_inv.apply_to_point(pos);
        local.velocity = parent_inv.apply_to_direction(vel);
    } else if (position_mode_ == TransformMode::MANUAL_TRANSFORM) {
        auto pos = get_local_position_at_(epoch, t_obs, dt);
        local.position = pos.position;
        local.velocity = pos.velocity;
    }

    if (rotation_mode_ == TransformMode::SPICE_TRANSFORM) {
        auto [rotation, ang_vel] = spice::sxform<double>("J2000", this->spice_frame_, t_emit);
        Transform<double> parent_inv = parent_ssb.inverse();
        local.rotation = parent_inv.rotation * rotation;
        local.angular_velocity = parent_inv.apply_to_direction(ang_vel);
    } else if (rotation_mode_ == TransformMode::MANUAL_TRANSFORM) {
        auto rot = get_local_rotation_at_(epoch, t_obs, dt);
        local.rotation = rot.rotation;
        local.angular_velocity = rot.angular_velocity;
    }

    // Mixed case - Step 2:
    if (position_mode_ == TransformMode::POSITION_CALLBACK) {
        const PositionState state =
            position_callback_->evaluate(emit_context, {local.rotation, local.angular_velocity});
        local.position = state.position;
        local.velocity = state.velocity;
    }

    if (rotation_mode_ == TransformMode::ROTATION_CALLBACK) {
        const RotationState state =
            rotation_callback_->evaluate(emit_context, {local.position, local.velocity});
        local.rotation = state.rotation;
        local.angular_velocity = state.angular_velocity;
    }
    local.scale = this->get_static_scale();

    Transform<double> ssb_state = parent_ssb * local;
    return ssb_state;
}

/**
 * @brief Get the node's local position transform at a given time.
 * @param epoch Time that the manually set transform corresponds to
 * @param t_obs Observation time
 * @param dt Light time delay
 * @return Transform<double> Local position transform
 */
template <IsSpectral TSpectral>
Transform<double>
Node<TSpectral>::get_local_position_at_(const Time& epoch, const Time& t_obs, double dt) const
{
    double total_dt = (t_obs.et() - dt) - epoch.et();

    Transform<double> local_transform_at_time{};
    if (position_mode_ == TransformMode::MANUAL_TRANSFORM) {
        local_transform_at_time.position =
            local_transform_.position + total_dt * local_transform_.velocity;
        local_transform_at_time.velocity = local_transform_.velocity;
    } else {
        // This is not expected to happen since the setters should prevent it
        HUIRA_THROW_ERROR("get_local_position_at_ - Unknown position_mode_");
    }
    return local_transform_at_time;
}

/**
 * @brief Get the node's local rotation transform at a given time.
 * @param epoch Time that the manually set transform corresponds to
 * @param t_obs Observation time
 * @param dt Light time delay
 * @return Transform<double> Local rotation transform
 */
template <IsSpectral TSpectral>
Transform<double>
Node<TSpectral>::get_local_rotation_at_(const Time& epoch, const Time& t_obs, double dt) const
{
    double total_dt = (t_obs.et() - dt) - epoch.et();

    Transform<double> local_transform_at_time{};
    if (rotation_mode_ == TransformMode::MANUAL_TRANSFORM) {
        Vec3<double> omega = local_transform_.angular_velocity;
        double angle = glm::length(omega) * total_dt;

        if (std::abs(angle) < 1e-12) {
            local_transform_at_time.rotation = local_transform_.rotation;
        } else {
            Vec3<double> axis = glm::normalize(omega);
            Rotation<double> delta =
                Rotation<double>::from_local_to_parent(axis, units::Radian{angle});

            if (body_frame_rates_) {
                // Body-frame rates: the increment is applied in the body frame,
                // i.e. q(t) = q_0 * delta
                local_transform_at_time.rotation = local_transform_.rotation * delta;
            } else {
                // Parent-frame rates: the increment is applied in the parent frame,
                // i.e. q(t) = delta * q_0
                local_transform_at_time.rotation = delta * local_transform_.rotation;
            }
        }

        // Transform::angular_velocity is (by convention -- see Transform::operator*)
        // always expressed in the node's parent frame. Body-frame rates must
        // therefore be re-expressed in parent axes before being stored:
        //     omega_parent(t) = q(t) * omega_body = q_0 * delta * omega_body
        //                     = q_0 * omega_body
        // (constant in time, since delta is a rotation about omega_body itself and
        // therefore preserves it). Parent-frame rates are stored as given.
        if (body_frame_rates_) {
            local_transform_at_time.angular_velocity = local_transform_.rotation * omega;
        } else {
            local_transform_at_time.angular_velocity = omega;
        }
    } else {
        // This is not expected to happen since the setters should prevent it
        HUIRA_THROW_ERROR("get_local_rotation_at_ - Unknown rotation_mode_");
    }
    return local_transform_at_time;
}

template <IsSpectral TSpectral>
Vec3<double> Node<TSpectral>::get_static_position() const
{
    if (position_mode_ != TransformMode::MANUAL_TRANSFORM) {
        HUIRA_THROW_ERROR(
            this->get_info() +
            " - cannot get static position when position mode is not MANUAL_TRANSFORM");
    }
    return local_transform_.position;
}
template <IsSpectral TSpectral>
Rotation<double> Node<TSpectral>::get_static_rotation() const
{
    if (position_mode_ != TransformMode::MANUAL_TRANSFORM) {
        HUIRA_THROW_ERROR(
            this->get_info() +
            " - cannot get static rotation when rotation mode is not MANUAL_TRANSFORM");
    }
    return local_transform_.rotation;
}
template <IsSpectral TSpectral>
Vec3<double> Node<TSpectral>::get_static_scale() const
{
    return local_transform_.scale;
}
template <IsSpectral TSpectral>
Vec3<double> Node<TSpectral>::get_static_velocity() const
{
    if (position_mode_ != TransformMode::MANUAL_TRANSFORM) {
        HUIRA_THROW_ERROR(
            this->get_info() +
            " - cannot get static velocity when position mode is not MANUAL_TRANSFORM");
    }
    return local_transform_.velocity;
}
template <IsSpectral TSpectral>
Vec3<double> Node<TSpectral>::get_static_angular_velocity() const
{
    if (position_mode_ != TransformMode::MANUAL_TRANSFORM) {
        HUIRA_THROW_ERROR(
            this->get_info() +
            " - cannot get static angular velocity when rotation mode is not MANUAL_TRANSFORM");
    }
    return local_transform_.angular_velocity;
}

template <IsSpectral TSpectral>
std::string Node<TSpectral>::get_spice_origin() const
{
    if (position_mode_ != TransformMode::SPICE_TRANSFORM) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - cannot get spice origin when position mode is not SPICE_TRANSFORM");
    }
    return spice_origin_;
}

template <IsSpectral TSpectral>
std::string Node<TSpectral>::get_spice_frame() const
{
    if (position_mode_ != TransformMode::SPICE_TRANSFORM) {
        HUIRA_THROW_ERROR(this->get_info() +
                          " - cannot get spice frame when rotation mode is not SPICE_TRANSFORM");
    }
    return spice_frame_;
}

/**
 * @brief Find the first ancestor (including self) with a SPICE origin
 * @return Pair of (ancestor node, accumulated transform from this to ancestor)
 * @throws std::runtime_error if no SPICE origin found in ancestry
 */
template <IsSpectral TSpectral>
std::pair<const Node<TSpectral>*, Transform<double>>
Node<TSpectral>::find_spice_origin_ancestor_() const
{
    Transform<double> accumulated;
    accumulated.position = Vec3<double>{0, 0, 0};
    accumulated.velocity = Vec3<double>{0, 0, 0};
    accumulated.rotation = Rotation<double>{};
    accumulated.scale = Vec3<double>{1, 1, 1};

    const Node<TSpectral>* current = this;

    // Walk up the scene graph
    while (current != nullptr) {
        // Check if this node has a SPICE origin
        if (current->position_mode_ == TransformMode::SPICE_TRANSFORM &&
            !current->spice_origin_.empty()) {
            return {current, accumulated};
        }

        // If this is not the starting node, accumulate its transform
        if (current != this) {
            // Accumulate position in parent's frame
            accumulated.position = current->local_transform_.position +
                                   current->local_transform_.rotation * accumulated.position;

            // Accumulate velocity
            accumulated.velocity = current->local_transform_.velocity +
                                   current->local_transform_.rotation * accumulated.velocity;

            // Accumulate rotation
            accumulated.rotation = current->local_transform_.rotation * accumulated.rotation;

            // Accumulate scale (component-wise multiplication)
            accumulated.scale = current->local_transform_.scale * accumulated.scale;
        } else {
            // For the starting node, initialize with its local transform
            accumulated = this->local_transform_;
        }

        // Move to parent
        current = current->parent_;
    }

    // No SPICE origin found in the entire ancestry chain
    HUIRA_THROW_ERROR(
        this->get_info() +
        " - cannot query SPICE frame: no ancestor with SPICE origin found in scene graph");
}

/**
 * @brief Find the first ancestor (including self) with a SPICE frame
 * @return Pair of (ancestor node, accumulated rotation from this to ancestor)
 * @throws std::runtime_error if no SPICE frame found in ancestry
 */
template <IsSpectral TSpectral>
std::pair<const Node<TSpectral>*, std::pair<Rotation<double>, Vec3<double>>>
Node<TSpectral>::find_spice_frame_ancestor_() const
{
    Rotation<double> accumulated_rotation = Rotation<double>{};
    Vec3<double> accumulated_ang_vel{0, 0, 0};

    const Node<TSpectral>* current = this;

    // Walk up the scene graph
    while (current != nullptr) {
        // Check if this node has a SPICE frame
        if (current->rotation_mode_ == TransformMode::SPICE_TRANSFORM &&
            !current->spice_frame_.empty()) {
            return {current, {accumulated_rotation, accumulated_ang_vel}};
        }

        // Accumulate rotation
        if (current != this) {
            // Compose rotations going up the tree
            accumulated_rotation = current->local_transform_.rotation * accumulated_rotation;

            // Accumulate angular velocity
            // ω_total = ω_parent + R_parent * ω_child
            accumulated_ang_vel = current->local_transform_.angular_velocity +
                                  current->local_transform_.rotation * accumulated_ang_vel;
        } else {
            // For the starting node, initialize with its local transform
            accumulated_rotation = this->local_transform_.rotation;
            accumulated_ang_vel = this->local_transform_.angular_velocity;
        }

        // Move to parent
        current = current->parent_;
    }

    // No SPICE frame found in the entire ancestry chain
    HUIRA_THROW_ERROR(
        this->get_info() +
        " - cannot query SPICE frame: no ancestor with SPICE rotation frame found in scene graph");
}

// ========================= //
// === Protected Members === //
// ========================= //
template <IsSpectral TSpectral>
bool Node<TSpectral>::position_can_be_spice_() const
{
    if (parent_) {
        if (parent_->position_mode_ != TransformMode::SPICE_TRANSFORM) {
            return false;
        }
    }
    return true;
}

template <IsSpectral TSpectral>
bool Node<TSpectral>::rotation_can_be_spice_() const
{
    if (parent_) {
        if (parent_->rotation_mode_ != TransformMode::SPICE_TRANSFORM) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Gets a handle to the parent node.
 *
 * Returns a base NodeHandle to the parent. This always returns the parent as a base
 * Node type, even if the parent is actually a more specific type like FrameNode.
 *
 * @return NodeHandle<TSpectral, Node<TSpectral>> Handle to the parent node
 * @throws std::runtime_error If this node has no parent (e.g., root node)
 */
template <IsSpectral TSpectral>
NodeHandle<TSpectral, Node<TSpectral>> Node<TSpectral>::get_parent() const
{
    if (!parent_) {
        HUIRA_THROW_ERROR(this->get_info() + " - node has no parent");
    }

    // Find the shared_ptr for the parent from the scene
    // We need to search through the scene graph to find the shared_ptr
    auto parent_shared = scene_->find_node_shared_ptr_(parent_);

    if (!parent_shared) {
        HUIRA_THROW_ERROR(this->get_info() + " - failed to find parent's shared_ptr");
    }

    return NodeHandle<TSpectral, Node<TSpectral>>(parent_shared);
}

/**
 * @brief Gets a handle to the parent node with a specific type.
 *
 * Returns a handle to the parent cast to the specified node type. This performs
 * a dynamic cast to verify the parent is actually of the requested type at runtime.
 *
 * @tparam TParentNode The expected type of the parent node (e.g., FrameNode<TSpectral>)
 * @return NodeHandle<TSpectral, TParentNode> Handle to the parent with the specified type
 * @throws std::runtime_error If this node has no parent
 * @throws std::runtime_error If the parent is not of type TParentNode
 */
/**
 * @brief Walk up to the root, checking each node's visibility flag.
 * @return bool True only if this node and every ancestor are visible
 */
template <IsSpectral TSpectral>
bool Node<TSpectral>::is_effectively_visible() const
{
    for (const Node<TSpectral>* node = this; node != nullptr; node = node->parent_) {
        if (!node->visible_) {
            return false;
        }
    }
    return true;
}

template <IsSpectral TSpectral>
template <typename TParentNode>
NodeHandle<TSpectral, TParentNode> Node<TSpectral>::get_parent_as() const
{
    if (!parent_) {
        HUIRA_THROW_ERROR(this->get_info() + " - node has no parent");
    }

    // Verify that parent is of the requested type
    TParentNode* typed_parent = dynamic_cast<TParentNode*>(parent_);
    if (!typed_parent) {
        HUIRA_THROW_ERROR(this->get_info() + " - parent is not of the requested type");
    }

    // Find the shared_ptr for the parent from the scene
    auto parent_shared = scene_->find_node_shared_ptr_(parent_);

    if (!parent_shared) {
        HUIRA_THROW_ERROR(this->get_info() + " - failed to find parent's shared_ptr");
    }

    // Cast the shared_ptr to the specific type
    auto typed_parent_shared = std::dynamic_pointer_cast<TParentNode>(parent_shared);
    if (!typed_parent_shared) {
        HUIRA_THROW_ERROR(this->get_info() + " - failed to cast parent to requested type");
    }

    return NodeHandle<TSpectral, TParentNode>(typed_parent_shared);
}
} // namespace huira
