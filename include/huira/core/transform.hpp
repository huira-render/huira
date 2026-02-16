
#pragma once

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/types.hpp"


namespace huira {
    /**
     * @brief Rigid body transform with position, rotation, scale, and kinematic quantities.
     *
     * Represents a 3D transformation including translation, rotation, scale, velocity, and angular velocity.
     * Provides methods for conversion to matrix, inversion, composition, and application to points, directions, and velocities.
     *
     * @tparam T Floating point type (float or double)
     */
    template <IsFloatingPoint T>
    struct Transform {
        Vec3<T> position{ 0,0,0 };
        Rotation<T> rotation{};
        Vec3<T> scale{ 1,1,1 };

        Vec3<T> velocity{ 0,0,0 };
        Vec3<T> angular_velocity{ 0,0,0 };

        /**
         * @brief Convert this Transform to another floating point type.
         * @tparam U Target floating point type
         * @return Transform<U> Converted transform
         */
        template <IsFloatingPoint U>
        explicit operator Transform<U>() const;

        /**
         * @brief Convert the transform to a 4x4 matrix (T * R * S).
         * @return Mat4<T> Transformation matrix
         */
        Mat4<T> to_matrix() const;

        /**
         * @brief Compute the inverse of the transform.
         * @return Transform<T> Inverse transform
         */
        Transform<T> inverse() const;

        /**
         * @brief Compose this transform with another.
         * @param b Other transform
         * @return Transform<T> Composed transform
         */
        Transform<T> operator* (const Transform<T>& b) const;

        /**
         * @brief Apply the transform to a point (scale, rotate, translate).
         * @param point Point to transform
         * @return Vec3<T> Transformed point
         */
        Vec3<T> apply_to_point(const Vec3<T>& point) const;

        /**
         * @brief Apply the transform to a direction (scale, rotate).
         * @param dir Direction to transform
         * @return Vec3<T> Transformed direction
         */
        Vec3<T> apply_to_direction(const Vec3<T>& dir) const;

        /**
         * @brief Apply the transform to a velocity (scale, rotate, add velocity).
         * @param vel Velocity to transform
         * @return Vec3<T> Transformed velocity
         */
        Vec3<T> apply_to_velocity(const Vec3<T>& vel) const;

        /**
         * @brief Apply the transform to an angular velocity (rotate, add angular velocity).
         * @param ang_vel Angular velocity to transform
         * @return Vec3<T> Transformed angular velocity
         */
        Vec3<T> apply_to_angular_velocity(const Vec3<T>& ang_vel) const;

        /**
         * @brief Compute the velocity of a point in this reference frame.
         * @param point Point in world coordinates
         * @return Vec3<T> Velocity of the point
         */
        Vec3<T> velocity_of_point(const Vec3<T>& point) const;

        /**
         * @brief Compute the velocity of a point expressed in local coordinates.
         * @param local_point Point in local coordinates
         * @return Vec3<T> Velocity of the point
         */
        Vec3<T> velocity_of_local_point(const Vec3<T>& local_point) const;
    };
}

#include "huira_impl/core/transform.ipp"
