#pragma once

#include <ostream>
#include <string>

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/core/units/units.hpp"

namespace huira {
    /**
     * @brief Represents a 3D rotation using a 3x3 matrix, with various construction and conversion utilities.
     *
     * Provides static methods for constructing rotations from matrices, quaternions, axis-angle, Euler angles, and basis vectors.
     * Supports conversion to/from quaternions, matrix, and axis representations, as well as composition and application to vectors.
     *
     * @tparam T Floating point type (float or double)
     */
    template <IsFloatingPoint T>
    class Rotation {
    public:
        Rotation() = default;

        static Rotation from_local_to_parent(Mat3<T> matrix);
        static Rotation from_local_to_parent(Quaternion<T> quaternion);
        static Rotation from_local_to_parent(ShusterQuaternion<T> shuster_quaternion);
        static Rotation from_local_to_parent(Vec3<T> axis, units::Radian angle);

        static Rotation from_parent_to_local(Mat3<T> matrix);
        static Rotation from_parent_to_local(Quaternion<T> quaternion);
        static Rotation from_parent_to_local(ShusterQuaternion<T> shuster_quaternion);
        static Rotation from_parent_to_local(Vec3<T> axis, units::Radian angle);

        static Rotation extrinsic_euler_angles(units::Radian angle1, units::Radian angle2, units::Radian angle3, std::string sequence = "XYZ");
        static Rotation intrinsic_euler_angles(units::Radian angle1, units::Radian angle2, units::Radian angle3, std::string sequence = "XYZ");

        static Rotation from_basis_vectors(Vec3<T> x_axis, Vec3<T> y_axis, Vec3<T> z_axis);

        template <IsFloatingPoint U>
        explicit operator Rotation<U>() const;

        std::string to_string() const;
        Rotation inverse() const;

        Quaternion<T> local_to_parent_quaternion() const;
        ShusterQuaternion<T> local_to_parent_shuster_quaternion() const;
        Quaternion<T> parent_to_local_quaternion() const;
        ShusterQuaternion<T> parent_to_local_shuster_quaternion() const;

        Mat3<T> local_to_parent_matrix() const;
        Mat3<T> parent_to_local_matrix() const;

        Vec3<T> x_axis() const;
        Vec3<T> y_axis() const;
        Vec3<T> z_axis() const;

        Rotation operator* (const Rotation& b) const;
        Rotation& operator*= (const Rotation& b);

        Vec3<T> operator* (const Vec3<T>& b) const;

        static Mat3<T> local_to_parent_x(units::Radian angle);
        static Mat3<T> local_to_parent_y(units::Radian angle);
        static Mat3<T> local_to_parent_z(units::Radian angle);

        static Mat3<T> parent_to_local_x(units::Radian angle);
        static Mat3<T> parent_to_local_y(units::Radian angle);
        static Mat3<T> parent_to_local_z(units::Radian angle);

    private:
        Mat3<T> matrix_{ 1 };

        void set_matrix_(Mat3<T> matrix);

        Rotation(Mat3<T> matrix) { set_matrix_(matrix); }

        Mat3<T> orthonormalize_(const Mat3<T>& matrix) const;
    };

    // Helpful typedefs:
    typedef Rotation<float> Rotation_f;
    typedef Rotation<double> Rotation_d;
}

#include "huira_impl/core/rotation.ipp"
