#pragma once

#include <ostream>
#include <string>

#include "huira/core/types.hpp"
#include "huira/core/units/units.hpp"

#include "huira/core/concepts/numeric_concepts.hpp"

namespace huira {
    /**
     * @brief A strict, explicit representation of a 3D rotation.
     *
     * ## Internal Storage Contract
     * This class **ALWAYS** stores the "Local-to-Parent" (Child-to-Parent) rotation matrix internally.
     * - **Direction:** Applying this rotation to a vector transforms it from the **Local** frame to the **Parent** frame ($v_{parent} = R \cdot v_{local}$).
     * - **Basis Vectors:** The columns of the internal matrix correspond to the Local frame's Basis Vectors ($X, Y, Z$) expressed in the Parent's coordinate system.
     *
     * ## coordinate System
     * - **Handedness:** Right-Handed (Standard Physics/GLM).
     * - **Column-Major:** The internal memory layout is column-major (compatible with GLM/OpenGL).
     *
     * ## Usage
     * To prevent ambiguity, this class disables implicit construction from raw matrices or quaternions.
     * You must use the static named constructors to explicitly state the direction of your source data:
     * - `Rotation::from_local_to_parent(...)` (Standard Model Matrices: Alibi Transformations)
     * - `Rotation::from_parent_to_local(...)` (SPICE/PDS: Coordinate Transformations)
     *
     * @tparam T The floating-point type (e.g., float or double).
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
