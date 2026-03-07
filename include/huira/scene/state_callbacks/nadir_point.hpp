#pragma once

#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"

namespace huira {
    struct ZUpYForwardCallback : public RotationCallback {
        void evaluate(const Time& epoch, const Vec3<double>& position, const Vec3<double>& velocity) override
        {
            (void)epoch;

            Vec3<float> z_axis = glm::normalize(position);
            Vec3<float> y_axis = glm::normalize(velocity);
            Vec3<float> x_axis = glm::normalize(glm::cross(y_axis, z_axis));
            y_axis = glm::normalize(glm::cross(z_axis, x_axis));

            this->rotation = Rotation<double>::from_basis_vectors(x_axis, y_axis, z_axis);
            this->angular_velocity = Vec3<double>{ 0.0, 0.0, 0.0 };
        }
    };

    struct ZDownYForwardCallback : public RotationCallback {
        void evaluate(const Time& epoch, const Vec3<double>& position, const Vec3<double>& velocity) override
        {
            (void)epoch;

            Vec3<float> z_axis = -glm::normalize(position);
            Vec3<float> y_axis = glm::normalize(velocity);
            Vec3<float> x_axis = glm::normalize(glm::cross(y_axis, z_axis));
            y_axis = glm::normalize(glm::cross(z_axis, x_axis));

            this->rotation = Rotation<double>::from_basis_vectors(x_axis, y_axis, z_axis);
            this->angular_velocity = Vec3<double>{ 0.0, 0.0, 0.0 };
        }
    };
}
