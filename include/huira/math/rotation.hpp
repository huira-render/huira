#pragma once

#include <string>
#include <ostream>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/math/types.hpp"
#include "huira/units/units.hpp"

#include "huira/huira_export.hpp"

namespace huira {
	template <IsFloatingPoint T>
	class HUIRA_EXPORT Rotation {
	public:
		Rotation() = default;
		Rotation(Mat3<T> matrix);
		Rotation(Quaternion<T> quaternion);
		Rotation(ShusterQuaternion<T> quaternion);
		Rotation(Vec3<T> axis, Degree angle);
		Rotation(Degree angle1, Degree angle2, Degree angle3, std::string sequence = "XYZ");

		std::string toString() const;
		Rotation inverse() const;

		Quaternion<T> getQuaternion() const;
		ShusterQuaternion<T> getShusterQuaternion() const;

		Mat3<T> getMatrix() const;
		Vec3<T> getXAxis() const;
		Vec3<T> getYAxis() const;
		Vec3<T> getZAxis() const;

		Rotation operator* (const Rotation& b) const;
		Rotation& operator*= (const Rotation& b);

		Vec3<T> operator* (const Vec3<T>& b) const;
		Mat3<T> operator* (const Mat3<T>& b) const;

		static Mat3<T> rotationX(Degree angle);
		static Mat3<T> rotationY(Degree angle);
		static Mat3<T> rotationZ(Degree angle);

	private:
		Mat3<T> matrix_{ 1 };

		void setMatrix(Mat3<T> matrix);

		friend std::ostream& operator<<(std::ostream& os, const Rotation<T>& rotation)
		{
			os << rotation.getMatrix();
			return os;
		}
	};

	// Helpful typedefs:
	typedef Rotation<float> Rotation_f;
	typedef Rotation<double> Rotation_d;

	// Declare Explicit Instantiations:
	extern template class Rotation<float>;
	extern template class Rotation<double>;
}