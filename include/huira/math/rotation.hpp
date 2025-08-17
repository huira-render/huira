#pragma once

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/math/types.hpp"

namespace huira {
	template <IsFloatingPoint T>
	class Rotation {
	public:
		Rotation(Mat3<T> matrix);

		Rotation(Quaternion<T> quaternion);
		Rotation(ShusterQuaternion<T> quaternion);

	private:
		Mat3<T> matrix_;
	};
};