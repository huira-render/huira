#include <array>
#include <limits>
#include <ostream>
#include <string>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/string_cast.hpp"

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/core/units/units.hpp"
#include "huira/util/logger.hpp"

namespace huira {
	/**
	 * @brief Construct a Rotation from a local-to-parent rotation matrix.
	 * @param matrix 3x3 rotation matrix
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_local_to_parent(Mat3<T> matrix)
	{
        Rotation<T> rotation;
		rotation.set_matrix_(matrix);
        return rotation;
	}


	/**
	 * @brief Construct a Rotation from a local-to-parent quaternion.
	 * @param quaternion Hamilton quaternion
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_local_to_parent(Quaternion<T> quaternion)
	{
        Rotation<T> rotation;
        rotation.set_matrix_(glm::mat3_cast(quaternion));
        return rotation;
	}


	/**
	 * @brief Construct a Rotation from a local-to-parent Shuster quaternion.
	 * @param shuster_quaternion Shuster quaternion
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_local_to_parent(ShusterQuaternion<T> shuster_quaternion)
	{
        Rotation<T> rotation;
        rotation.set_matrix_(glm::mat3_cast(to_hamilton(shuster_quaternion)));
        return rotation;
	}


	/**
	 * @brief Construct a Rotation from an axis and angle (Rodrigues' formula).
	 * @param axis Rotation axis (will be normalized)
	 * @param angle Rotation angle in radians
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_local_to_parent(Vec3<T> axis, units::Radian angle)
	{
		// Normalize the axis vector
		T length = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
		if (length < std::numeric_limits<T>::epsilon()) {
			Mat3<T> identity{ 1 };
            Rotation<T> rotation;
			rotation.set_matrix_(identity);
            return rotation;
		}

		Vec3<T> normalized_axis = axis / length;
		T x = normalized_axis.x;
		T y = normalized_axis.y;
		T z = normalized_axis.z;

		// Convert angle to same type as T
		T angle_t = static_cast<T>(angle.to_si());
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

            Rotation<T> rotation;
		rotation.set_matrix_(matrix);
            return rotation;
	}



	/**
	 * @brief Construct a Rotation from a parent-to-local rotation matrix.
	 * @param matrix 3x3 rotation matrix
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_parent_to_local(Mat3<T> matrix)
	{
		return from_local_to_parent(glm::transpose(matrix));
	}


	/**
	 * @brief Construct a Rotation from a parent-to-local quaternion.
	 * @param quaternion Hamilton quaternion
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_parent_to_local(Quaternion<T> quaternion)
	{
		return from_local_to_parent(glm::inverse(quaternion));
	}


	/**
	 * @brief Construct a Rotation from a parent-to-local Shuster quaternion.
	 * @param shuster_quaternion Shuster quaternion
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_parent_to_local(ShusterQuaternion<T> shuster_quaternion)
	{
		return from_parent_to_local(to_hamilton(shuster_quaternion));
	}


	/**
	 * @brief Construct a Rotation from a parent-to-local axis and angle.
	 * @param axis Rotation axis (will be normalized)
	 * @param angle Rotation angle in radians
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_parent_to_local(Vec3<T> axis, units::Radian angle)
	{
		return from_local_to_parent(axis, -angle);
	}


	/**
	 * @brief Construct a Rotation from extrinsic Euler angles.
	 * @param angle1 First rotation angle (radians)
	 * @param angle2 Second rotation angle (radians)
	 * @param angle3 Third rotation angle (radians)
	 * @param sequence Axis sequence (e.g., "XYZ")
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::extrinsic_euler_angles(units::Radian angle1, units::Radian angle2, units::Radian angle3, std::string sequence)
	{
		if (sequence.size() != 3) {
            HUIRA_THROW_ERROR("Euler angle sequence must be 3 characters long, e.g., 'XYZ'");
		}

		std::array<Mat3<T>, 3> basis;
		std::array<units::Degree, 3> angles = { angle1, angle2, angle3 };

		for (size_t i = 0; i < 3; ++i) {
			char component = static_cast<char>(std::tolower(sequence[i]));

			if (component == 'x' || component == '1') {
				basis[i] = local_to_parent_x(angles[i]);
			}
			else if (component == 'y' || component == '2') {
				basis[i] = local_to_parent_y(angles[i]);
			}
			else if (component == 'z' || component == '3') {
				basis[i] = local_to_parent_z(angles[i]);
			}
			else {
                HUIRA_THROW_ERROR("Invalid character in Euler angle sequence: " + std::string(1, component));
			}
		}


        Mat3<T> matrix = basis[2] * basis[1] * basis[0];
        Rotation<T> rotation;
		rotation.set_matrix_(matrix);
        return rotation;
	}


	/**
	 * @brief Construct a Rotation from intrinsic Euler angles.
	 * @param angle1 First rotation angle (radians)
	 * @param angle2 Second rotation angle (radians)
	 * @param angle3 Third rotation angle (radians)
	 * @param sequence Axis sequence (e.g., "XYZ")
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::intrinsic_euler_angles(units::Radian angle1, units::Radian angle2, units::Radian angle3, std::string sequence)
	{
		if (sequence.size() != 3) {
			HUIRA_THROW_ERROR("Euler angle sequence must be 3 characters long, e.g., 'XYZ'");
		}

		std::array<Mat3<T>, 3> basis;
		std::array<units::Degree, 3> angles = { angle1, angle2, angle3 };

		for (size_t i = 0; i < 3; ++i) {
			char component = static_cast<char>(std::tolower(sequence[i]));

			if (component == 'x' || component == '1') {
				basis[i] = local_to_parent_x(angles[i]);
			}
			else if (component == 'y' || component == '2') {
				basis[i] = local_to_parent_y(angles[i]);
			}
			else if (component == 'z' || component == '3') {
				basis[i] = local_to_parent_z(angles[i]);
			}
			else {
				HUIRA_THROW_ERROR("Invalid character in Euler angle sequence: " + std::string(1, component));
			}
		}


		Mat3<T> matrix = basis[0] * basis[1] * basis[2];
		Rotation<T> rotation;
		rotation.set_matrix_(matrix);
		return rotation;
	}


	/**
	 * @brief Construct a Rotation from three basis vectors.
	 * @param x_axis X axis vector
	 * @param y_axis Y axis vector
	 * @param z_axis Z axis vector
	 * @return Rotation<T> Rotation object
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_basis_vectors(Vec3<T> x_axis, Vec3<T> y_axis, Vec3<T> z_axis)
	{
		Mat3<T> matrix;
		matrix[0] = x_axis;
		matrix[1] = y_axis;
		matrix[2] = z_axis;

		Rotation<T> rotation;
		rotation.set_matrix_(matrix);
		return rotation;
	}


	/**
	 * @brief Convert this Rotation to another floating point type.
	 * @tparam U Target floating point type
	 * @return Rotation<U> Converted rotation
	 */
	template <IsFloatingPoint T>
	template <IsFloatingPoint U>
	Rotation<T>::operator Rotation<U>() const
	{
		Mat3<U> cast_matrix = this->matrix_;
		return Rotation<U>::from_local_to_parent(cast_matrix);
	}



	/**
	 * @brief Get a string representation of the rotation matrix.
	 * @return std::string String representation
	 */
	template <IsFloatingPoint T>
	std::string Rotation<T>::to_string() const
	{
		return glm::to_string(matrix_);
	}


	/**
	 * @brief Get the inverse rotation.
	 * @return Rotation<T> Inverse rotation
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::inverse() const
	{
        Rotation<T> rotation;
        rotation.set_matrix_(glm::transpose(matrix_));
        return rotation;
	}


	/**
	 * @brief Get the local-to-parent rotation as a Hamilton quaternion.
	 * @return Quaternion<T> Hamilton quaternion
	 */
	template <IsFloatingPoint T>
	Quaternion<T> Rotation<T>::local_to_parent_quaternion() const
	{
		return glm::quat_cast(matrix_);
	}


	/**
	 * @brief Get the local-to-parent rotation as a Shuster quaternion.
	 * @return ShusterQuaternion<T> Shuster quaternion
	 */
	template <IsFloatingPoint T>
	ShusterQuaternion<T> Rotation<T>::local_to_parent_shuster_quaternion() const
	{
		return to_shuster(glm::quat_cast(matrix_));
	}


	/**
	 * @brief Get the parent-to-local rotation as a Hamilton quaternion.
	 * @return Quaternion<T> Hamilton quaternion
	 */
	template <IsFloatingPoint T>
	Quaternion<T> Rotation<T>::parent_to_local_quaternion() const
	{
		return glm::inverse(glm::quat_cast(matrix_));
	}


	/**
	 * @brief Get the parent-to-local rotation as a Shuster quaternion.
	 * @return ShusterQuaternion<T> Shuster quaternion
	 */
	template <IsFloatingPoint T>
	ShusterQuaternion<T> Rotation<T>::parent_to_local_shuster_quaternion() const
	{
		Quaternion<T> hamilton_quat = glm::inverse(glm::quat_cast(matrix_));
		return to_shuster(hamilton_quat);
	}



	/**
	 * @brief Get the local-to-parent rotation matrix.
	 * @return Mat3<T> Rotation matrix
	 */
	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::local_to_parent_matrix() const
	{
		return matrix_;
	}


	/**
	 * @brief Get the parent-to-local rotation matrix.
	 * @return Mat3<T> Rotation matrix
	 */
	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::parent_to_local_matrix() const
	{
		return glm::transpose(matrix_);
	}


	/**
	 * @brief Get the X axis of the rotation (first column of the matrix).
	 * @return Vec3<T> X axis vector
	 */
	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::x_axis() const
	{
		return matrix_[0];
	}


	/**
	 * @brief Get the Y axis of the rotation (second column of the matrix).
	 * @return Vec3<T> Y axis vector
	 */
	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::y_axis() const
	{
		return matrix_[1];
	}


	/**
	 * @brief Get the Z axis of the rotation (third column of the matrix).
	 * @return Vec3<T> Z axis vector
	 */
	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::z_axis() const
	{
		return matrix_[2];
	}



	/**
	 * @brief Compose two rotations (this * b).
	 * @param b Other rotation
	 * @return Rotation<T> Composed rotation
	 */
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::operator* (const Rotation<T>& b) const
	{
		Mat3<T> result_matrix = matrix_ * b.matrix_;
		return Rotation<T>(result_matrix);
	}


	/**
	 * @brief Compose this rotation with another (in-place).
	 * @param b Other rotation
	 * @return Rotation<T>& Reference to this rotation
	 */
	template <IsFloatingPoint T>
	Rotation<T>& Rotation<T>::operator*= (const Rotation<T>& b)
	{
		this->set_matrix_(matrix_ * b.matrix_);
		return *this;
	}


	/**
	 * @brief Apply the rotation to a vector.
	 * @param b Vector to rotate
	 * @return Vec3<T> Rotated vector
	 */
	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::operator* (const Vec3<T>& b) const
	{
		return matrix_ * b;
	}



	/**
	 * @brief Get a rotation matrix for a rotation about the X axis.
	 * @param angle Rotation angle in radians
	 * @return Mat3<T> Rotation matrix
	 */
	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::local_to_parent_x(units::Radian angle)
	{
		T angle_t = static_cast<T>(angle.to_si());

		T c = std::cos(angle_t);
		T s = std::sin(angle_t);
        
		Mat3<T> result;
		result[0][0] = 1; result[1][0] = 0; result[2][0] =  0;
		result[0][1] = 0; result[1][1] = c; result[2][1] = -s;
		result[0][2] = 0; result[1][2] = s; result[2][2] =  c;

		return result;
	}


	/**
	 * @brief Get a rotation matrix for a rotation about the Y axis.
	 * @param angle Rotation angle in radians
	 * @return Mat3<T> Rotation matrix
	 */
	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::local_to_parent_y(units::Radian angle)
	{
		T angle_t = static_cast<T>(angle.to_si());

		T c = std::cos(angle_t);
		T s = std::sin(angle_t);

		Mat3<T> result;
		result[0][0] =  c; result[1][0] = 0; result[2][0] = s;
		result[0][1] =  0; result[1][1] = 1; result[2][1] = 0;
		result[0][2] = -s; result[1][2] = 0; result[2][2] = c;

		return result;
	}


	/**
	 * @brief Get a rotation matrix for a rotation about the Z axis.
	 * @param angle Rotation angle in radians
	 * @return Mat3<T> Rotation matrix
	 */
	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::local_to_parent_z(units::Radian angle)
	{
		T angle_t = static_cast<T>(angle.to_si());

		T c = std::cos(angle_t);
		T s = std::sin(angle_t);

		Mat3<T> result;
		result[0][0] = c; result[1][0] = -s; result[2][0] = 0;
		result[0][1] = s; result[1][1] =  c; result[2][1] = 0;
		result[0][2] = 0; result[1][2] =  0; result[2][2] = 1;

		return result;
	}


	/**
	 * @brief Get a parent-to-local rotation matrix about the X axis.
	 * @param angle Rotation angle in radians
	 * @return Mat3<T> Rotation matrix
	 */
	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::parent_to_local_x(units::Radian angle)
	{
		return local_to_parent_x(-angle);
	}


	/**
	 * @brief Get a parent-to-local rotation matrix about the Y axis.
	 * @param angle Rotation angle in radians
	 * @return Mat3<T> Rotation matrix
	 */
	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::parent_to_local_y(units::Radian angle)
	{
		return local_to_parent_y(-angle);
	}


	/**
	 * @brief Get a parent-to-local rotation matrix about the Z axis.
	 * @param angle Rotation angle in radians
	 * @return Mat3<T> Rotation matrix
	 */
	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::parent_to_local_z(units::Radian angle)
	{
		return local_to_parent_z(-angle);
	}



	/**
	 * @brief Set the internal rotation matrix, checking for orthonormality.
	 * @param matrix 3x3 rotation matrix
	 */
	template <IsFloatingPoint T>
	void Rotation<T>::set_matrix_(Mat3<T> matrix)
	{
        constexpr T loose_epsilon = static_cast<T>(1e-3);
        T det = glm::determinant(matrix);
        if (std::fabs(det - 1.0) > loose_epsilon) {
            HUIRA_THROW_ERROR("Rotation matrix must have a determinant close to 1. Given matrix has determinant: " + std::to_string(det));
        }
        this->matrix_ = orthonormalize_(matrix);
	}


	/**
	 * @brief Orthonormalize a 3x3 matrix using Gram-Schmidt.
	 * @param matrix Input matrix
	 * @return Mat3<T> Orthonormalized matrix
	 */
	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::orthonormalize_(const Mat3<T>& matrix) const
	{
		// Extract columns (basis vectors)
		Vec3<T> x = matrix[0];
		Vec3<T> y = matrix[1];
		Vec3<T> z = matrix[2];

		// Gram-Schmidt orthonormalization
		x = glm::normalize(x);
		y = y - glm::dot(y, x) * x;
		y = glm::normalize(y);
		z = glm::cross(x, y);

		// Construct orthonormal matrix
		Mat3<T> result;
		result[0] = x;
		result[1] = y;
		result[2] = z;

		return result;
	}
}
