#pragma once

#include "huira/core/types.hpp"
#include "huira/core/rotation.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"

namespace huira {
    template <IsFloatingPoint T>
    struct Transform {
        Vec3<T> position{ 0,0,0 };
        Rotation<T> rotation{};
        Vec3<T> scale{ 1,1,1 };

        Vec3<T> velocity{ 0,0,0 };
        Vec3<T> angular_velocity{ 0,0,0 };

        template <IsFloatingPoint U>
        explicit operator Transform<U>() const;

        Mat4<T> to_matrix() const;
        Transform<T> inverse() const;
        Transform<T> operator* (const Transform<T>& b) const;

        // Apply transformations to geometric entities
        Vec3<T> apply_to_point(const Vec3<T>& point) const;
        Vec3<T> apply_to_direction(const Vec3<T>& dir) const;

        // Apply transformations to kinematic quantities
        Vec3<T> apply_to_velocity(const Vec3<T>& vel) const;
        Vec3<T> apply_to_angular_velocity(const Vec3<T>& ang_vel) const;

        // Compute velocity of a point in this reference frame
        Vec3<T> velocity_of_point(const Vec3<T>& point) const;

        // Compute velocity of a point expressed in local coordinates
        Vec3<T> velocity_of_local_point(const Vec3<T>& local_point) const;
    };
}

#include "huira_impl/core/transform.ipp"
