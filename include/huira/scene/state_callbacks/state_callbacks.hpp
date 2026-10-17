#pragma once

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
    ObservationMode observation_mode = ObservationMode::GEOMETRIC_STATE;

    /// The same context at another time.
    [[nodiscard]] PoseContext at(const Time& t) const { return {t, reference, observation_mode}; }
};

struct PositionCallback {
    virtual ~PositionCallback() = default;
    virtual void evaluate(const PoseContext& context,
                          const Rotation<double>& rotation,
                          const Vec3<double>& angular_rate) = 0;

    Vec3<double> position{};
    Vec3<double> velocity{};
};

template <typename T>
concept IsPositionCallback = std::derived_from<T, PositionCallback>;

struct RotationCallback {
    virtual ~RotationCallback() = default;
    virtual void evaluate(const PoseContext& context,
                          const Vec3<double>& position,
                          const Vec3<double>& velocity) = 0;

    Rotation<double> rotation{};
    Vec3<double> angular_velocity{};
};

template <typename T>
concept IsRotationCallback = std::derived_from<T, RotationCallback>;

struct StateCallback {
    virtual ~StateCallback() = default;
    virtual void evaluate(const PoseContext& context) = 0;

    Transform<double> state{};
};

template <typename T>
concept IsTransformCallback = std::derived_from<T, StateCallback>;
} // namespace huira
