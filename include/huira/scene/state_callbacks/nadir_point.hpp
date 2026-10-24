#pragma once

#include "glm/glm.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/core/types.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"

namespace huira {

namespace detail {

/// The angular velocity of a frame built from a node's position and velocity, with one axis along
/// the position and another the velocity's part at right angles to it: the turning rate of the
/// position's direction, (r x v) / |r|^2, about the parent's axes. It is exact while the node's
/// acceleration is along its position (as on a Keplerian orbit) or zero, as look_at() assumes too.
[[nodiscard]] inline Vec3<double> radial_frame_angular_velocity(const PositionState& state)
{
    return glm::cross(state.position, state.velocity) / glm::dot(state.position, state.position);
}

} // namespace detail

/**
 * @brief Orients a node with its z axis away from its parent's origin (zenith) and its y axis as
 * close to its velocity as is at right angles to that.
 */
struct ZUpYForwardCallback : public RotationCallback {
    [[nodiscard]] RotationState evaluate(const PoseContext& /*context*/,
                                         const PositionState& state) const override
    {
        Vec3<double> z_axis = glm::normalize(state.position);
        Vec3<double> y_axis = glm::normalize(state.velocity);
        Vec3<double> x_axis = glm::normalize(glm::cross(y_axis, z_axis));
        y_axis = glm::normalize(glm::cross(z_axis, x_axis));

        return {Rotation<double>::from_basis_vectors(x_axis, y_axis, z_axis),
                detail::radial_frame_angular_velocity(state)};
    }
};

/**
 * @brief Orients a node with its z axis toward its parent's origin (nadir) and its y axis as close
 * to its velocity as is at right angles to that.
 */
struct ZDownYForwardCallback : public RotationCallback {
    [[nodiscard]] RotationState evaluate(const PoseContext& /*context*/,
                                         const PositionState& state) const override
    {
        Vec3<double> z_axis = -glm::normalize(state.position);
        Vec3<double> y_axis = glm::normalize(state.velocity);
        Vec3<double> x_axis = glm::normalize(glm::cross(y_axis, z_axis));
        y_axis = glm::normalize(glm::cross(z_axis, x_axis));

        return {Rotation<double>::from_basis_vectors(x_axis, y_axis, z_axis),
                detail::radial_frame_angular_velocity(state)};
    }
};
} // namespace huira
