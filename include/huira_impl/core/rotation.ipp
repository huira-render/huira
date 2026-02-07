#include <array>
#include <ostream>
#include <string>
#include <limits>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/string_cast.hpp"

#include "huira/core/types.hpp"
#include "huira/core/units/units.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/util/logger.hpp"

namespace huira {
	// ==================== //
	// === Constructors === //
	// ==================== //
	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_local_to_parent(Mat3<T> matrix)
	{
        Rotation<T> rotation;
		rotation.set_matrix_(matrix);
        return rotation;
	}

	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_local_to_parent(Quaternion<T> quaternion)
	{
        Rotation<T> rotation;
        rotation.set_matrix_(glm::mat3_cast(quaternion));
        return rotation;
	}

	template <IsFloatingPoint T>
	Rotation<T> Rotation<T>::from_local_to_parent(ShusterQuaternion<T> shuster_quaternion)
	{
        Rotation<T> rotation;
        rotation.set_matrix_(glm::mat3_cast(to_hamilton(shuster_quaternion)));
        return rotation;
	}

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
		T angle_t = static_cast<T>(angle.get_si_value());
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


    template <IsFloatingPoint T>
    Rotation<T> Rotation<T>::from_parent_to_local(Mat3<T> matrix)
    {
        return from_local_to_parent(glm::transpose(matrix));
    }

    template <IsFloatingPoint T>
    Rotation<T> Rotation<T>::from_parent_to_local(Quaternion<T> quaternion)
    {
        return from_local_to_parent(glm::inverse(quaternion));
    }

    template <IsFloatingPoint T>
    Rotation<T> Rotation<T>::from_parent_to_local(ShusterQuaternion<T> shuster_quaternion)
    {
        return from_parent_to_local(to_hamilton(shuster_quaternion));
    }

    template <IsFloatingPoint T>
    Rotation<T> Rotation<T>::from_parent_to_local(Vec3<T> axis, units::Radian angle)
    {
        return from_local_to_parent(axis, -angle);
    }

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

    template <IsFloatingPoint T>
    template <IsFloatingPoint U>
    Rotation<T>::operator Rotation<U>() const
    {
        Mat3<U> cast_matrix = this->matrix_;
        return Rotation<U>::from_local_to_parent(cast_matrix);
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
        Rotation<T> rotation;
        rotation.set_matrix_(glm::transpose(matrix_));
        return rotation;
	}


	// =============== //
	// === Getters === //
	// =============== //
	template <IsFloatingPoint T>
	Quaternion<T> Rotation<T>::local_to_parent_quaternion() const
	{
		return glm::quat_cast(matrix_);
	}

	template <IsFloatingPoint T>
	ShusterQuaternion<T> Rotation<T>::local_to_parent_shuster_quaternion() const
	{
		return to_shuster(glm::quat_cast(matrix_));
	}

    template <IsFloatingPoint T>
    Quaternion<T> Rotation<T>::parent_to_local_quaternion() const
    {
        return glm::inverse(glm::quat_cast(matrix_));
    }

    template <IsFloatingPoint T>
    ShusterQuaternion<T> Rotation<T>::parent_to_local_shuster_quaternion() const
    {
        Quaternion<T> hamilton_quat = glm::inverse(glm::quat_cast(matrix_));
        return to_shuster(hamilton_quat);
    }


	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::local_to_parent_matrix() const
	{
		return matrix_;
	}

    template <IsFloatingPoint T>
    Mat3<T> Rotation<T>::parent_to_local_matrix() const
    {
        return glm::transpose(matrix_);
    }

	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::x_axis() const
	{
		return matrix_[0];
	}

	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::y_axis() const
	{
		return matrix_[1];
	}

	template <IsFloatingPoint T>
	Vec3<T> Rotation<T>::z_axis() const
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
		this->set_matrix_(matrix_ * b.matrix_);
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
	Mat3<T> Rotation<T>::local_to_parent_x(units::Radian angle)
	{
		T angle_t = static_cast<T>(angle.get_si_value());

		T c = std::cos(angle_t);
		T s = std::sin(angle_t);
		
		Mat3<T> result;
		result[0][0] = 1; result[1][0] = 0; result[2][0] =  0;
		result[0][1] = 0; result[1][1] = c; result[2][1] = -s;
		result[0][2] = 0; result[1][2] = s; result[2][2] =  c;

		return result;
	}

	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::local_to_parent_y(units::Radian angle)
	{
		T angle_t = static_cast<T>(angle.get_si_value());

		T c = std::cos(angle_t);
		T s = std::sin(angle_t);

		Mat3<T> result;
		result[0][0] =  c; result[1][0] = 0; result[2][0] = s;
		result[0][1] =  0; result[1][1] = 1; result[2][1] = 0;
		result[0][2] = -s; result[1][2] = 0; result[2][2] = c;

		return result;
	}

	template <IsFloatingPoint T>
	Mat3<T> Rotation<T>::local_to_parent_z(units::Radian angle)
	{
		T angle_t = static_cast<T>(angle.get_si_value());

		T c = std::cos(angle_t);
		T s = std::sin(angle_t);

		Mat3<T> result;
		result[0][0] = c; result[1][0] = -s; result[2][0] = 0;
		result[0][1] = s; result[1][1] =  c; result[2][1] = 0;
		result[0][2] = 0; result[1][2] =  0; result[2][2] = 1;

		return result;
	}

    template <IsFloatingPoint T>
    Mat3<T> Rotation<T>::parent_to_local_x(units::Radian angle)
    {
        return local_to_parent_x(-angle);
    }

    template <IsFloatingPoint T>
    Mat3<T> Rotation<T>::parent_to_local_y(units::Radian angle)
    {
        return local_to_parent_y(-angle);
    }

    template <IsFloatingPoint T>
    Mat3<T> Rotation<T>::parent_to_local_z(units::Radian angle)
    {
        return local_to_parent_z(-angle);
    }


	// ======================== //
	// === Private Memebers === //
	// ======================== //
	template <IsFloatingPoint T>
	void Rotation<T>::set_matrix_(Mat3<T> matrix)
	{
		constexpr T epsilon = static_cast<T>(1e-9);
		if (std::fabs(glm::determinant(matrix) - 1.0) > epsilon) {
            HUIRA_THROW_ERROR("Rotation matrix must have a determinant of 1. Given matrix has determinant: " + std::to_string(glm::determinant(matrix)));
		}
		this->matrix_ = matrix;
	}
}
