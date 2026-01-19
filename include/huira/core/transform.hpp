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

        Mat4<T> to_matrix() const;
        Transform<T> inverse() const;
        Transform<T> operator* (const Transform<T>& b) const;

        Vec3<T> apply_to_point(const Vec3<T>& point) const;
        Vec3<T> apply_to_direction(const Vec3<T>& dir) const;
        Vec3<T> apply_to_velocity(const Vec3<T>& vel) const;
        Vec3<T> apply_to_angular_velocity(const Vec3<T>& ang_vel) const;
    };
}

#include "huira_impl/core/transform.ipp"
