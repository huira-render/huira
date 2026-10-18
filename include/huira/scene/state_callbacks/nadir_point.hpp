#pragma once

#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"

namespace huira {
struct ZUpYForwardCallback : public RotationCallback {
    [[nodiscard]] RotationState evaluate(const PoseContext& /*context*/,
                                         const PositionState& state) const override
    {
        Vec3<float> z_axis = glm::normalize(state.position);
        Vec3<float> y_axis = glm::normalize(state.velocity);
        Vec3<float> x_axis = glm::normalize(glm::cross(y_axis, z_axis));
        y_axis = glm::normalize(glm::cross(z_axis, x_axis));

        return {Rotation<double>::from_basis_vectors(x_axis, y_axis, z_axis),
                Vec3<double>{0.0, 0.0, 0.0}};
    }
};

struct ZDownYForwardCallback : public RotationCallback {
    [[nodiscard]] RotationState evaluate(const PoseContext& /*context*/,
                                         const PositionState& state) const override
    {
        Vec3<float> z_axis = -glm::normalize(state.position);
        Vec3<float> y_axis = glm::normalize(state.velocity);
        Vec3<float> x_axis = glm::normalize(glm::cross(y_axis, z_axis));
        y_axis = glm::normalize(glm::cross(z_axis, x_axis));

        return {Rotation<double>::from_basis_vectors(x_axis, y_axis, z_axis),
                Vec3<double>{0.0, 0.0, 0.0}};
    }
};
} // namespace huira
