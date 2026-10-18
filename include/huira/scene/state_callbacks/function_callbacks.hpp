#pragma once

#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/core/types.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"
#include "huira/units/units.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/validate.hpp"

namespace huira {

/**
 * @brief What a position function gives at a time: a position, and a velocity if it has one.
 */
struct PositionSample {
    Vec3<double> position{};
    std::optional<Vec3<double>> velocity{};
};

/**
 * @brief What a rotation function gives at a time: a rotation, and an angular velocity (about
 * the parent's axes) if it has one.
 */
struct RotationSample {
    Rotation<double> rotation{};
    std::optional<Vec3<double>> angular_velocity{};
};

/**
 * @brief How far apart, in seconds, the two times a rate is estimated from are: the rate is
 * the change between them, centered on the time it is wanted at.
 *
 * The estimate is the change divided by the time between the two times, as Time holds them, so it
 * errs only by how the motion curves between them: by (RATE_ESTIMATE_SPAN / 2)^2 / 6 times its
 * third derivative, about 2e-7 of the rate for something turning at 1 rad/s.
 */
inline constexpr double RATE_ESTIMATE_SPAN = 2e-3;

/**
 * @brief A node's position from a function of the pose context.
 *
 * The function gives the position in the parent's frame, and the velocity if it has one. A
 * velocity it does not give is estimated from the positions RATE_ESTIMATE_SPAN apart, centered
 * on the time.
 */
class PositionFunctionCallback : public PositionCallback {
  public:
    using Function = std::function<PositionSample(const PoseContext&)>;

    /// @param what What the position is of, for errors (the node's name).
    PositionFunctionCallback(Function function, std::string what)
        : function_(std::move(function)), what_(std::move(what))
    {
        if (!function_) {
            HUIRA_THROW_ERROR(what_ + " - The position function is empty");
        }
    }

    [[nodiscard]] PositionState evaluate(const PoseContext& context,
                                         const RotationState& /*rotation*/) const override
    {
        PositionState state;
        const PositionSample sample = position_at_(context);
        state.position = sample.position;
        if (sample.velocity) {
            state.velocity = *sample.velocity;
        } else {
            const Time before = context.time + units::Second(-0.5 * RATE_ESTIMATE_SPAN);
            const Time after = context.time + units::Second(0.5 * RATE_ESTIMATE_SPAN);
            const double span = (after - before).value();
            state.velocity = (position_at_(context.at(after)).position -
                              position_at_(context.at(before)).position) /
                             span;
        }
        return state;
    }

  private:
    Function function_;
    std::string what_;

    PositionSample position_at_(const PoseContext& context) const
    {
        PositionSample sample = function_(context);
        validate_real(sample.position, what_ + " position from its function");
        if (sample.velocity) {
            validate_real(*sample.velocity, what_ + " velocity from its function");
        }
        return sample;
    }
};

/**
 * @brief A node's rotation from a function of the pose context.
 *
 * The function gives the rotation relative to the parent, and the angular velocity, about the
 * parent's axes, if it has one. An angular velocity it does not give is estimated from the
 * rotations RATE_ESTIMATE_SPAN apart, centered on the time.
 */
class RotationFunctionCallback : public RotationCallback {
  public:
    using Function = std::function<RotationSample(const PoseContext&)>;

    /// @param what What the rotation is of, for errors (the node's name).
    RotationFunctionCallback(Function function, std::string what)
        : function_(std::move(function)), what_(std::move(what))
    {
        if (!function_) {
            HUIRA_THROW_ERROR(what_ + " - The rotation function is empty");
        }
    }

    [[nodiscard]] RotationState evaluate(const PoseContext& context,
                                         const PositionState& /*position*/) const override
    {
        RotationState state;
        const RotationSample sample = rotation_at_(context);
        state.rotation = sample.rotation;
        if (sample.angular_velocity) {
            state.angular_velocity = *sample.angular_velocity;
        } else {
            const Time before = context.time + units::Second(-0.5 * RATE_ESTIMATE_SPAN);
            const Time after = context.time + units::Second(0.5 * RATE_ESTIMATE_SPAN);
            const double span = (after - before).value();
            const Rotation<double> change = rotation_at_(context.at(after)).rotation *
                                            rotation_at_(context.at(before)).rotation.inverse();
            state.angular_velocity = rotation_vector(change) / span;
        }
        return state;
    }

  private:
    Function function_;
    std::string what_;

    RotationSample rotation_at_(const PoseContext& context) const
    {
        RotationSample sample = function_(context);
        if (sample.angular_velocity) {
            validate_real(*sample.angular_velocity, what_ + " angular velocity from its function");
        }
        return sample;
    }
};

namespace detail {

/// A position function's result as a sample: a position alone, a position with its velocity, or
/// a sample.
template <typename TResult>
PositionSample to_position_sample(TResult&& result)
{
    using R = std::remove_cvref_t<TResult>;
    if constexpr (std::is_same_v<R, PositionSample>) {
        return std::forward<TResult>(result);
    } else if constexpr (std::is_same_v<R, PositionState>) {
        return {result.position, result.velocity};
    } else {
        static_assert(std::is_convertible_v<R, Vec3<double>>,
                      "A position function returns a Vec3<double> (the position, whose velocity "
                      "is then estimated), a PositionState, or a PositionSample");
        return {Vec3<double>(result), std::nullopt};
    }
}

/// A rotation function's result as a sample: a rotation alone, a rotation with its angular
/// velocity, or a sample.
template <typename TResult>
RotationSample to_rotation_sample(TResult&& result)
{
    using R = std::remove_cvref_t<TResult>;
    if constexpr (std::is_same_v<R, RotationSample>) {
        return std::forward<TResult>(result);
    } else if constexpr (std::is_same_v<R, RotationState>) {
        return {result.rotation, result.angular_velocity};
    } else {
        static_assert(std::is_same_v<R, Rotation<double>>,
                      "A rotation function returns a Rotation<double> (whose angular velocity is "
                      "then estimated), a RotationState, or a RotationSample");
        return {result, std::nullopt};
    }
}

} // namespace detail
} // namespace huira
