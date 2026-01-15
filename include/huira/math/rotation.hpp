#pragma once

#include <ostream>
#include <string>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/math/types.hpp"
#include "huira/units/units.hpp"

namespace huira {
	/**
	 * @brief Represents a 3D rotation using a rotation matrix, quaternion, or axis-angle representation.
	 *
	 * This class provides various constructors to create a rotation from different representations,
	 * including a rotation matrix, quaternion, Shuster quaternion, axis-angle representation, and Euler angles.
	 * It also provides methods to convert between these representations and perform operations like multiplication
	 * and inversion.
	 *
	 * @tparam T The floating-point type (e.g., float or double).
	 */
	template <IsFloatingPoint T>
	class Rotation {
	public:
		Rotation() = default;
		Rotation(Mat3<T> matrix);
		Rotation(Quaternion<T> quaternion);
		Rotation(ShusterQuaternion<T> quaternion);
		Rotation(Vec3<T> axis, Degree angle);
		Rotation(Degree angle1, Degree angle2, Degree angle3, std::string sequence = "XYZ");

		std::string to_string() const;
		Rotation inverse() const;

		Quaternion<T> get_quaternion() const;
		ShusterQuaternion<T> get_shuster_quaternion() const;

		Mat3<T> get_matrix() const;
		Vec3<T> get_x_axis() const;
		Vec3<T> get_y_axis() const;
		Vec3<T> get_z_axis() const;

		Rotation operator* (const Rotation& b) const;
		Rotation& operator*= (const Rotation& b);

		Vec3<T> operator* (const Vec3<T>& b) const;

		static Mat3<T> rotation_x(Degree angle);
		static Mat3<T> rotation_y(Degree angle);
		static Mat3<T> rotation_z(Degree angle);

	private:
		Mat3<T> matrix_{ 1 };
		Mat3<T> transpose_{ 1 };

		void set_matrix(Mat3<T> matrix);

		friend std::ostream& operator<<(std::ostream& os, const Rotation<T>& rotation)
		{
			os << rotation.get_matrix();
			return os;
		}
	};

	// Helpful typedefs:
	typedef Rotation<float> Rotation_f;
	typedef Rotation<double> Rotation_d;
}

#include "huira_impl/math/rotation.ipp"
