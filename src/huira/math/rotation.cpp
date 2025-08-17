#include "huira/math/rotation.hpp"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/math/types.hpp"

namespace huira {
	template <IsFloatingPoint T>
	Rotation<T>::Rotation(Mat3<T> matrix)
		: matrix_{ matrix }
	{

	}

	template <IsFloatingPoint T>
	Rotation<T>::Rotation(Quaternion<T> quaternion)
	{
		matrix_ = glm::mat3_cast(quaternion);
	}

	template <IsFloatingPoint T>
	Rotation<T>::Rotation(ShusterQuaternion<T> shuster_quaternion)
	{
		matrix_ = glm::mat3_cast(toHamilton(shuster_quaternion));
	}

	// Explicit Instantiations:
	extern template class Rotation<float>;
	extern template class Rotation<double>;

	// Helpful typedefs:
	typedef Rotation<float> Rotation_f;
	typedef Rotation<double> Rotation_d;
}