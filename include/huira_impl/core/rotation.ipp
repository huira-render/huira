#include <array>
#include <ostream>
#include <string>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/string_cast.hpp"

#include "huira/core/types.hpp"
#include "huira/core/units.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"

namespace huira {
	// ==================== //
	// === Constructors === //
	// ==================== //
	template <IsFloatingPoint T>
	Rotation<T>::Rotation(Mat3<T> matrix)
	{
		this->set_matrix(matrix);
	}

	template <IsFloatingPoint T>
	Rotation<T>::Rotation(Quaternion<T> quaternion)
	{
		this->set_matrix(glm::mat3_cast(quaternion));
	}

	template <IsFloatingPoint T>
	Rotation<T>::Rotation(ShusterQuaternion<T> shuster_quaternion)
	{
		this->set_matrix(glm::mat3_cast(to_hamilton(shuster_quaternion)));
	}

	template <IsFloatingPoint T>
	Rotation<T>::Rotation(Vec3<T> axis, units::Degree angle)
	{
		// Normalize the axis vector
		T length = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
		if (length < std::numeric_limits<T>::epsilon()) {
			Mat3<T> identity{ 1 };
			this->set_matrix(identity);
			return;
		}

		Vec3<T> normalized_axis = axis / length;
		T x = normalized_axis.x;
		T y = normalized_axis.y;
		T z = normalized_axis.z;

		// Convert angle to same type as T
		T angle_t = static_cast<T>(angle.getSIValue());
		T c = std::cos(angle_t);
		T s = std::sin(angle_t);


		// Create rotation matrix using Rodrigues' formula
		Mat3<T> matrix;
		matrix[0][0] = c + x * x * (1 - c);
		matrix[0][1] = y * x * (1 - c) + z * s;
		matrix[0][2] = z * x * (1 - c) - y * s;

		matrix[1][0] = x * y * (1 - c) - z * s;
		matrix[1][1] = c + y * y * (1 - c);
		matrix[1][2] = z * y * (1 - c) + x * s;

		matrix[2][0] = x * z * (1 - c) + y * s;
		matrix[2][1] = y * z * (1 - c) - x * s;
		matrix[2][2] = c + z * z * (1 - c);

		this->set_matrix(matrix);
	}

	template <IsFloatingPoint T>
	Rotation<T>::Rotation(units::Degree angle1, units::Degree angle2, units::Degree angle3, std::string sequence)
	{
		if (sequence.size() != 3) {
			// TODO throw error
		}

		std::array<Mat3<T>, 3> basis;
		std::array<units::Degree, 3> angles = { angle1, angle2, angle3 };

		for (size_t i = 0; i < 3; ++i) {
			char component = static_cast<char>(std::tolower(sequence[i]));

			if (component == 'x' || component == '1') {
				basis[i] = rotation_x(angles[i]);
			}
			else if (component == 'y' || component == '2') {
				basis[i] = rotation_y(angles[i]);
			}
			else if (component == 'z' || component == '3') {
				basis[i] = rotation_z(angles[i]);
			}
			else {
				// TODO throw error
			}
		}


		Mat3<T> matrix = basis[0] * basis[1] * basis[2];
		this->set_matrix(matrix);
	}


	// ========================= //
	// === Memeber Functions === //
	// ========================= //
	template <IsFloatingPoint T>
	std::string Rotation<T>::to_string() const
	{
		return glm::to_string(matrix_);
	}

	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::inverse() const
	{
		return Rotation<T>(transpose_);
	}


	// =============== //
	// === Getters === //
	// =============== //
	template <IsFloatingPoint T>
	Quaternion<T> Rotation<T>::get_quaternion() const
	{
		return glm::quat_cast(matrix_);
	}

	template <IsFloatingPoint T>
	ShusterQuaternion<T> Rotation<T>::get_shuster_quaternion() const
	{
		return to_shuster(glm::quat_cast(matrix_));
	}

	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::get_matrix() const
	{
		return matrix_;
	}

	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::get_x_axis() const
	{
		return matrix_[0];
	}

	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::get_y_axis() const
	{
		return matrix_[1];
	}

	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::get_z_axis() const
	{
		return matrix_[2];
	}

	// ========================== //
	// === Operator Overloads === //
	// ========================== //
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::operator* (const Rotation<T>& b) const
	{
		Mat3<T> result_matrix = matrix_ * b.matrix_;
		return Rotation<T>(result_matrix);
	}

	template <IsFloatingPoint T>
	Rotation<T>& Rotation<T>::operator*= (const Rotation<T>& b)
	{
		this->set_matrix(matrix_ * b.matrix_);
		return *this;
	}

	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::operator* (const Vec3<T>& b) const
	{
		return matrix_ * b;
	}


	// ====================== //
	// === Static Members === //
	// ====================== //
	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::rotation_x(units::Degree angle)
	{
		T angle_t = static_cast<T>(angle.getSIValue());

		T c = std::cos(angle_t);
		T s = std::sin(angle_t);
		
		Mat3<T> result;
		result[0][0] = 1; result[1][0] = 0; result[2][0] =  0;
		result[0][1] = 0; result[1][1] = c; result[2][1] = -s;
		result[0][2] = 0; result[1][2] = s; result[2][2] =  c;

		return result;
	}

	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::rotation_y(units::Degree angle)
	{
		T angle_t = static_cast<T>(angle.getSIValue());

		T c = std::cos(angle_t);
		T s = std::sin(angle_t);

		Mat3<T> result;
		result[0][0] =  c; result[1][0] = 0; result[2][0] = s;
		result[0][1] =  0; result[1][1] = 1; result[2][1] = 0;
		result[0][2] = -s; result[1][2] = 0; result[2][2] = c;

		return result;
	}

	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::rotation_z(units::Degree angle)
	{
		T angle_t = static_cast<T>(angle.getSIValue());

		T c = std::cos(angle_t);
		T s = std::sin(angle_t);

		Mat3<T> result;
		result[0][0] = c; result[1][0] = -s; result[2][0] = 0;
		result[0][1] = s; result[1][1] =  c; result[2][1] = 0;
		result[0][2] = 0; result[1][2] =  0; result[2][2] = 1;

		return result;
	}


	// ======================== //
	// === Private Memebers === //
	// ======================== //
	template <IsFloatingPoint T>
	void Rotation<T>::set_matrix(Mat3<T> matrix)
	{
		constexpr T epsilon = static_cast<T>(1e-9);
		if (std::fabs(glm::determinant(matrix) - 1.0) > epsilon) {
			// TODO Throw error
		}
		this->matrix_ = matrix;
		this->transpose_ = glm::transpose(matrix);
	}
}
