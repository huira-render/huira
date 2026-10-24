#pragma once

#include <cmath>
#include <concepts>

#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"

namespace huira {

/**
 * @brief Where a scene view places each object.
 *
 * - TRUE_STATE: where it is at the time of observation.
 * - GEOMETRIC_STATE: where it was when the light now reaching the observer left it (light time).
 * - ABERRATED_STATE: that, as seen by a moving observer (light time and stellar aberration).
 */
enum class ObservationMode { TRUE_STATE, GEOMETRIC_STATE, ABERRATED_STATE };

/**
 * @brief The frame a fixed point given to look_at() is in.
 *
 * - PARENT: the looking node's parent's frame, as a position set by set_position() is. The point
 *   moves with the parent.
 * - ROOT: the scene's root frame (the solar system barycenter, with the J2000 axes).
 */
enum class TargetFrame { PARENT, ROOT };

/**
 * @brief What a pose is evaluated for, given to every pose callback.
 */
struct PoseContext {
    /// The time the pose is wanted at: the time of observation, less any light time.
    Time time;

    /// The time that poses set by hand hold at: the reference time of the scene view's interval.
    /// A callback that evaluates other nodes passes it on, so that they are placed as the scene
    /// view places them.
    Time reference;

    /// How the scene view observes the scene.
    ObservationMode observation_mode = ObservationMode::ABERRATED_STATE;

    /// The same context at another time.
    [[nodiscard]] PoseContext at(const Time& t) const { return {t, reference, observation_mode}; }
};

/**
 * @brief A node's position and velocity, in its parent's frame.
 */
struct PositionState {
    Vec3<double> position{}; ///< In meters.
    Vec3<double> velocity{}; ///< In meters per second.
};

/**
 * @brief A node's orientation and angular velocity, relative to its parent.
 */
struct RotationState {
    Rotation<double> rotation{};     ///< From the node's frame to its parent's.
    Vec3<double> angular_velocity{}; ///< In radians per second, about the parent's axes.
};

/**
 * @brief Where a node is, as a function of time: the base of the position sources other than
 * SPICE and poses set by hand.
 *
 * A callback is evaluated many times while a scene view is built, at times in any order
 * (temporal samples, light-time iterations, and estimates of rates), and never while it renders.
 * It must give the same answer for the same context.
 */
struct PositionCallback {
    virtual ~PositionCallback() = default;

    /**
     * @brief The node's position and velocity in its parent's frame.
     * @param context The time it is wanted at, and how the scene is observed.
     * @param rotation The node's rotation at that time, from its own source: identity if that is
     *        a rotation callback, which is evaluated after this one, and given this position.
     */
    [[nodiscard]] virtual PositionState evaluate(const PoseContext& context,
                                                 const RotationState& rotation) const = 0;
};

template <typename T>
concept IsPositionCallback = std::derived_from<T, PositionCallback>;

/**
 * @brief How a node is oriented, as a function of time: the base of the rotation sources other
 * than SPICE and rotations set by hand. As for PositionCallback, it must give the same answer for
 * the same context.
 */
struct RotationCallback {
    virtual ~RotationCallback() = default;

    /**
     * @brief The node's rotation and angular velocity relative to its parent.
     * @param context The time it is wanted at, and how the scene is observed.
     * @param position The node's position and velocity in its parent's frame at that time.
     */
    [[nodiscard]] virtual RotationState evaluate(const PoseContext& context,
                                                 const PositionState& position) const = 0;
};

template <typename T>
concept IsRotationCallback = std::derived_from<T, RotationCallback>;

/**
 * @brief A node's whole pose (position, velocity, rotation and angular velocity) relative to its
 * parent, as a function of time. Its scale is the node's own.
 */
struct StateCallback {
    virtual ~StateCallback() = default;

    [[nodiscard]] virtual Transform<double> evaluate(const PoseContext& context) const = 0;
};

template <typename T>
concept IsTransformCallback = std::derived_from<T, StateCallback>;

/**
 * @brief A rotation as its rotation vector: its axis times its angle, the shorter way round, in
 * the frame it rotates (for a change of a node's rotation, its parent's).
 *
 * Found from its quaternion, so it is precise at any angle, however small.
 */
[[nodiscard]] inline Vec3<double> rotation_vector(const Rotation<double>& rotation)
{
    Quaternion<double> q = rotation.local_to_parent_quaternion();
    if (q.w < 0.0) {
        q = -q;
    }
    const Vec3<double> v{q.x, q.y, q.z};
    const double sin_half = glm::length(v);
    if (sin_half == 0.0) {
        return Vec3<double>{0.0};
    }
    return v * (2.0 * std::atan2(sin_half, q.w) / sin_half);
}

/**
 * @brief The rotation by a rotation vector: about its direction, by its length in radians.
 */
[[nodiscard]] inline Rotation<double> rotation_from_vector(const Vec3<double>& vector)
{
    const double angle = glm::length(vector);
    if (angle == 0.0) {
        return Rotation<double>{};
    }
    const Vec3<double> v = vector * (std::sin(0.5 * angle) / angle);
    return Rotation<double>::from_local_to_parent(
        Quaternion<double>(std::cos(0.5 * angle), v.x, v.y, v.z));
}

} // namespace huira
