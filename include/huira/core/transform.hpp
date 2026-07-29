#pragma once

#include <array>
#include <string>

#include "embree4/rtcore.h"
#include "huira/concepts/numeric_concepts.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/types.hpp"

namespace huira {
/**
 * @brief Rigid body transform with position, rotation, scale, and kinematic quantities.
 *
 * Represents a 3D transformation including translation, rotation, scale, velocity, and angular
 * velocity. Provides methods for conversion to matrix, inversion, composition, and application to
 * points, directions, and velocities.
 *
 * @tparam T Floating point type (float or double)
 */
template <IsFloatingPoint T>
struct Transform {
    Vec3<T> position{0, 0, 0};
    Rotation<T> rotation{};
    Vec3<T> scale{1, 1, 1};

    /// Linear velocity of this frame's origin, expressed in the PARENT frame's
    /// axes. operator* composes it accordingly (rotating a child's velocity only
    /// by ancestor rotations, never by the child's own rotation).
    Vec3<T> velocity{0, 0, 0};

    /// Angular velocity of this frame, expressed in the PARENT frame's axes.
    /// This convention is relied upon by operator*, inverse(),
    /// apply_to_angular_velocity(), and velocity_of_point(). Body-frame rates
    /// must be re-expressed in parent axes (omega_parent = rotation * omega_body)
    /// before being stored here.
    Vec3<T> angular_velocity{0, 0, 0};

    template <IsFloatingPoint U>
    explicit operator Transform<U>() const;

    Mat4<T> to_matrix() const;

    RTCQuaternionDecomposition to_embree() const;

    std::string to_string() const;

    Transform<T> inverse() const;

    Transform<T> operator*(const Transform<T>& b) const;

    Vec3<T> apply_to_point(const Vec3<T>& point) const;

    Vec3<T> apply_to_direction(const Vec3<T>& dir) const;

    Vec3<T> apply_to_velocity(const Vec3<T>& vel) const;

    Vec3<T> apply_to_angular_velocity(const Vec3<T>& ang_vel) const;

    Vec3<T> velocity_of_point(const Vec3<T>& point) const;

    Vec3<T> velocity_of_local_point(const Vec3<T>& local_point) const;
};
} // namespace huira

#include "huira_impl/core/transform.ipp"
