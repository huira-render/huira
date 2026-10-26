#pragma once

#include <array>
#include <string>

#include "embree4/rtcore.h"
#include "huira/concepts/numeric_concepts.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/types.hpp"

namespace huira {
/**
 * @brief Transform with position, rotation, scale and shear, and kinematic quantities.
 *
 * Represents a 3D transformation including translation, rotation, scale, shear, velocity, and
 * angular velocity. Provides methods for conversion to matrix, inversion, composition, and
 * application to points, directions, normals, and velocities.
 *
 * A point is placed at position + rotation * S * point, where S is the upper triangular
 * scale-and-shear matrix
 *
 *     | scale.x  shear.x  shear.y |
 *     | 0        scale.y  shear.z |
 *     | 0        0        scale.z |
 *
 * the form Embree's quaternion decomposition takes. A node is given no shear, but combining
 * frames can need one: a child turned within a parent scaled unevenly is slanted by it, which no
 * rotation and scale can describe. Every combination is exactly a rotation times such an S, so
 * composing and inverting keep it.
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

    /// The scale-and-shear matrix's entries above its diagonal (see Transform): x is its xy
    /// entry (how far a point moves along x with its y), y its xz entry, and z its yz entry.
    /// Zero, but where combining frames slants something.
    Vec3<T> shear{0, 0, 0};

    template <IsFloatingPoint U>
    explicit operator Transform<U>() const;

    /// Whether the transform has any shear.
    [[nodiscard]] bool has_shear() const { return shear != Vec3<T>{0, 0, 0}; }

    /// The upper triangular scale-and-shear matrix S (see Transform).
    Mat3<T> scale_shear_matrix() const;

    Mat4<T> to_matrix() const;

    RTCQuaternionDecomposition to_embree() const;

    /// The transform as Embree's 3x4 column-major matrix (RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR).
    std::array<float, 12> to_embree_matrix() const;

    std::string to_string() const;

    Transform<T> inverse() const;

    Transform<T> operator*(const Transform<T>& b) const;

    Vec3<T> apply_to_point(const Vec3<T>& point) const;

    Vec3<T> apply_to_direction(const Vec3<T>& dir) const;

    Vec3<T> apply_to_normal(const Vec3<T>& normal) const;

    Vec3<T> apply_to_velocity(const Vec3<T>& vel) const;

    Vec3<T> apply_to_angular_velocity(const Vec3<T>& ang_vel) const;

    Vec3<T> velocity_of_point(const Vec3<T>& point) const;

    Vec3<T> velocity_of_local_point(const Vec3<T>& local_point) const;
};
} // namespace huira

#include "huira_impl/core/transform.ipp"
