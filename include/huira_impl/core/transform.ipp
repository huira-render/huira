#include <array>
#include <cmath>
#include <cstddef>

#include "embree4/rtcore.h"
#include "glm/glm.hpp"
#include "huira/concepts/numeric_concepts.hpp"

namespace huira {
namespace detail {
/// S * v, for the scale-and-shear matrix S of a transform with this scale and shear: only the
/// scale's products where there is no shear.
template <IsFloatingPoint T>
Vec3<T> apply_scale_shear(const Vec3<T>& scale, const Vec3<T>& shear, const Vec3<T>& v)
{
    Vec3<T> result{v.x * scale.x, v.y * scale.y, v.z * scale.z};
    if (shear != Vec3<T>{0, 0, 0}) {
        result.x += shear.x * v.y + shear.y * v.z;
        result.y += shear.z * v.z;
    }
    return result;
}

/// Whether a scale along the axes and a rotation commute (S R = R S): exactly when each entry of
/// the rotation that carries one axis into another is zero, or joins two axes scaled alike. A
/// child turned so within a parent's scale is turned and scaled, with no shear.
template <IsFloatingPoint T>
bool scale_commutes_with(const Vec3<T>& scale, const Mat3<T>& rotation)
{
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            if (row != column && rotation[column][row] != T(0) && scale[row] != scale[column]) {
                return false;
            }
        }
    }
    return true;
}

/// +1 or -1 for each component, by its sign.
template <IsFloatingPoint T>
Vec3<T> signs_of(const Vec3<T>& v)
{
    return {std::copysign(T(1), v.x), std::copysign(T(1), v.y), std::copysign(T(1), v.z)};
}

/**
 * @brief Give a transform the rotation, scale and shear of a linear map: the map factored into a
 *        rotation times an upper triangular scale-and-shear matrix (a QR decomposition).
 *
 * The factoring is unique once the signs of the scale are chosen: they are given, so that a
 * mirror keeps its sign on the axis it had, and a map that needs no shear is factored into the
 * rotation and scale it was made of. Their product is the sign of the map's determinant.
 */
template <IsFloatingPoint T>
void factor_linear(const Mat3<T>& linear, const Vec3<T>& scale_signs, Transform<T>& transform)
{
    // Gram-Schmidt on the columns, each made orthogonal to those before it twice, which keeps
    // them at right angles however unequal the scales are:
    std::array<Vec3<T>, 3> axes;
    for (std::size_t k = 0; k < 3; ++k) {
        Vec3<T> axis = linear[static_cast<int>(k)];
        for (int pass = 0; pass < 2; ++pass) {
            for (std::size_t j = 0; j < k; ++j) {
                axis -= glm::dot(axes[j], axis) * axes[j];
            }
        }
        axes[k] = glm::normalize(axis);
    }
    transform.rotation = Rotation<T>::from_local_to_parent(
        Mat3<T>{axes[0] * scale_signs.x, axes[1] * scale_signs.y, axes[2] * scale_signs.z});
    const Mat3<T> upper = glm::transpose(transform.rotation.local_to_parent_matrix()) * linear;
    transform.scale = Vec3<T>{upper[0][0], upper[1][1], upper[2][2]};
    transform.shear = Vec3<T>{upper[1][0], upper[2][0], upper[2][1]};
}
} // namespace detail

/**
 * @brief Convert this Transform to another floating point type.
 * @tparam U Target floating point type
 * @return `Transform<U>` Converted transform
 */
template <IsFloatingPoint T>
template <IsFloatingPoint U>
Transform<T>::operator Transform<U>() const
{
    Transform<U> result;
    result.position = this->position;
    result.rotation = static_cast<Rotation<U>>(this->rotation);
    result.scale = this->scale;
    result.velocity = this->velocity;
    result.angular_velocity = this->angular_velocity;
    result.shear = this->shear;
    return result;
}

/**
 * @brief The upper triangular scale-and-shear matrix (see Transform).
 * @return Mat3<T> The matrix, column-major as glm's are
 */
template <IsFloatingPoint T>
Mat3<T> Transform<T>::scale_shear_matrix() const
{
    return Mat3<T>{
        Vec3<T>{scale.x, 0, 0}, Vec3<T>{shear.x, scale.y, 0}, Vec3<T>{shear.y, shear.z, scale.z}};
}

/**
 * @brief Convert the transform to a 4x4 matrix (T * R * S), which places points as
 * apply_to_point() does: M * (p, 1).
 *
 * Mat4 is column-major, as glm's matrices are: the first three columns are the rotation times
 * the scale-and-shear matrix, and the fourth is the translation.
 *
 * @return Mat4<T> Transformation matrix
 */
template <IsFloatingPoint T>
Mat4<T> Transform<T>::to_matrix() const
{
    const Mat3<T> linear = rotation.local_to_parent_matrix() * scale_shear_matrix();
    Mat4<T> result{T(1)};
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            result[column][row] = linear[column][row];
        }
    }
    result[3][0] = position.x;
    result[3][1] = position.y;
    result[3][2] = position.z;
    return result;
}

/**
 * @brief Convert the transform to embree pose information.
 * @return RTCQuaternionDecomposition Embree pose information
 */
template <IsFloatingPoint T>
RTCQuaternionDecomposition Transform<T>::to_embree() const
{
    RTCQuaternionDecomposition decomp;

    // Scale:
    decomp.scale_x = static_cast<float>(scale.x);
    decomp.scale_y = static_cast<float>(scale.y);
    decomp.scale_z = static_cast<float>(scale.z);

    // Shear, above the scale in Embree's upper triangular matrix as in ours:
    decomp.skew_xy = static_cast<float>(shear.x);
    decomp.skew_xz = static_cast<float>(shear.y);
    decomp.skew_yz = static_cast<float>(shear.z);

    // No shift:
    decomp.shift_x = 0.0f;
    decomp.shift_y = 0.0f;
    decomp.shift_z = 0.0f;

    // Translation:
    decomp.translation_x = static_cast<float>(position.x);
    decomp.translation_y = static_cast<float>(position.y);
    decomp.translation_z = static_cast<float>(position.z);

    // Quaternion — check your Rotation class's convention.
    // Embree uses (r, i, j, k) = (w, x, y, z):
    auto q = rotation.local_to_parent_quaternion();
    decomp.quaternion_r = static_cast<float>(q.w);
    decomp.quaternion_i = static_cast<float>(q.x);
    decomp.quaternion_j = static_cast<float>(q.y);
    decomp.quaternion_k = static_cast<float>(q.z);

    return decomp;
}

/**
 * @brief Convert the transform to Embree's 3x4 column-major matrix.
 *
 * Its first three columns are the rotation times the scale-and-shear matrix, and its last the
 * translation: the same transform as to_embree() gives, as a matrix.
 *
 * @return std::array<float, 12> The matrix, column by column
 */
template <IsFloatingPoint T>
std::array<float, 12> Transform<T>::to_embree_matrix() const
{
    const Mat3<T> linear = rotation.local_to_parent_matrix() * scale_shear_matrix();
    std::array<float, 12> matrix{};
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            matrix[static_cast<std::size_t>(3 * column + row)] =
                static_cast<float>(linear[column][row]);
        }
    }
    matrix[9] = static_cast<float>(position.x);
    matrix[10] = static_cast<float>(position.y);
    matrix[11] = static_cast<float>(position.z);
    return matrix;
}

template <IsFloatingPoint T>
std::string Transform<T>::to_string() const
{
    std::string output = "";
    output += glm::to_string(position) + " | ";
    output += rotation.to_string() + " | ";
    output += glm::to_string(scale) + " | ";
    output += glm::to_string(velocity) + " | ";
    output += glm::to_string(angular_velocity) + " | ";
    output += glm::to_string(shear);
    return output;
}

/**
 * @brief Compute the inverse of the transform.
 *
 * Its rotation is this one's inverse and its scale this one's reciprocal, when the scale is even
 * along the axes the rotation mixes. Otherwise the inverse of rotating, then scaling, is
 * unscaling, then rotating back, which no rotation and scale along the axes describe: its linear
 * part is factored into a rotation, scale and shear.
 *
 * @return Transform<T> Inverse transform
 */
template <IsFloatingPoint T>
Transform<T> Transform<T>::inverse() const
{
    Transform<T> result;

    if (has_shear() || !detail::scale_commutes_with(scale, rotation.local_to_parent_matrix())) {
        const Mat3<T> inverse_linear =
            glm::inverse(rotation.local_to_parent_matrix() * scale_shear_matrix());
        detail::factor_linear(inverse_linear, detail::signs_of(scale), result);
        result.position = -(inverse_linear * position);
        // As below, with the inverse's linear part in full:
        result.velocity = inverse_linear * (glm::cross(angular_velocity, position) - velocity);
        result.angular_velocity = rotation.inverse() * (-angular_velocity);
        return result;
    }

    // Inverse rotation
    result.rotation = rotation.inverse(); // conjugate for unit quaternion

    // Inverse scale (assuming no zero components!)
    result.scale = Vec3<T>{T(1) / scale.x, T(1) / scale.y, T(1) / scale.z};

    // Inverse position: undo the rotation and scale
    result.position = result.rotation * (-position * result.scale);

    // Inverse velocity (in the new frame): chosen such that T * T.inverse()
    // has zero velocity. Composition contributes both the rotated inverse
    // velocity and the omega-cross-r term of this frame's angular velocity
    // acting on the inverse position:
    //     v + R * v_inv + omega x (R * p_inv) = 0,  with  R * p_inv = -p
    // =>  v_inv = R^-1 * (omega x p - v)
    result.velocity =
        result.rotation * ((glm::cross(angular_velocity, position) - velocity) * result.scale);

    // Inverse angular velocity
    result.angular_velocity = result.rotation * (-angular_velocity);

    return result;
}

/**
 * @brief Compose this transform with another.
 *
 * The rotations compose, and the scales multiply, when b neither shears nor is turned between
 * axes this transform scales differently. Otherwise this transform's scale slants b, and the
 * product of the two linear parts is factored into a rotation, scale and shear.
 *
 * @param b Other transform
 * @return Transform<T> Composed transform
 */
template <IsFloatingPoint T>
Transform<T> Transform<T>::operator*(const Transform<T>& b) const
{
    Transform<T> result;

    // Position: rotate and scale b's position by this transform, then add this position
    const Vec3<T> scaled_b_position = detail::apply_scale_shear(scale, shear, b.position);
    result.position = position + rotation * scaled_b_position;

    if (!has_shear() && !b.has_shear() &&
        detail::scale_commutes_with(scale, b.rotation.local_to_parent_matrix())) {
        // Rotation: compose rotations
        result.rotation = rotation * b.rotation;

        // Scale: component-wise multiply
        result.scale = scale * b.scale;
    } else {
        const Mat3<T> linear = rotation.local_to_parent_matrix() * scale_shear_matrix() *
                               b.rotation.local_to_parent_matrix() * b.scale_shear_matrix();
        detail::factor_linear(linear, detail::signs_of(scale * b.scale), result);
    }

    // Velocity: this is trickier - b's velocity needs to be transformed
    // and we need to account for angular velocity contribution
    result.velocity = velocity + rotation * detail::apply_scale_shear(scale, shear, b.velocity) +
                      glm::cross(angular_velocity, rotation * scaled_b_position);

    // Angular velocity: rotate b's angular velocity and add
    result.angular_velocity = angular_velocity + rotation * (b.angular_velocity);

    return result;
}

/**
 * @brief Apply the transform to a point (scale and shear, rotate, translate).
 * @param point Point to transform
 * @return Vec3<T> Transformed point
 */
template <IsFloatingPoint T>
Vec3<T> Transform<T>::apply_to_point(const Vec3<T>& point) const
{
    // Static transformation: scale, rotate, translate
    Vec3<T> scaled_point = detail::apply_scale_shear(scale, shear, point);

    Vec3<T> rotated_point = rotation * scaled_point;
    Vec3<T> transformed_point = rotated_point + position;
    return transformed_point;
}

/**
 * @brief Apply the transform to a direction (scale and shear, rotate).
 * @param dir Direction to transform
 * @return Vec3<T> Transformed direction
 */
template <IsFloatingPoint T>
Vec3<T> Transform<T>::apply_to_direction(const Vec3<T>& dir) const
{
    // Directions don't get translated, only scaled and rotated
    Vec3<T> scaled_dir = detail::apply_scale_shear(scale, shear, dir);

    Vec3<T> rotated_dir = rotation * scaled_dir;
    return rotated_dir;
}

/**
 * @brief Apply the transform to a surface's normal: by the inverse transpose of what turns its
 *        tangents (apply_to_direction()), which keeps it at right angles to them when the scale
 *        is uneven or shears. The same direction as apply_to_direction() gives, for an even
 *        scale. Not normalized.
 * @param normal Normal to transform
 * @return Vec3<T> Transformed normal
 */
template <IsFloatingPoint T>
Vec3<T> Transform<T>::apply_to_normal(const Vec3<T>& normal) const
{
    // The scale-and-shear matrix's inverse transpose, by forward substitution (its transpose is
    // lower triangular). With no shear, this divides by the scale:
    Vec3<T> unscaled;
    unscaled.x = normal.x / scale.x;
    unscaled.y = (normal.y - shear.x * unscaled.x) / scale.y;
    unscaled.z = (normal.z - shear.y * unscaled.x - shear.z * unscaled.y) / scale.z;
    return rotation * unscaled;
}

/**
 * @brief Apply the transform to a velocity (scale, rotate, add velocity).
 * @param vel Velocity to transform
 * @return Vec3<T> Transformed velocity
 */
template <IsFloatingPoint T>
Vec3<T> Transform<T>::apply_to_velocity(const Vec3<T>& vel) const
{
    // Velocities rotate and add together
    Vec3<T> scaled_vel = detail::apply_scale_shear(scale, shear, vel);

    Vec3<T> rotated_vel = rotation * scaled_vel;
    Vec3<T> transformed_vel = rotated_vel + velocity;
    return transformed_vel;
}

/**
 * @brief Apply the transform to an angular velocity (rotate, add angular velocity).
 * @param ang_vel Angular velocity to transform
 * @return Vec3<T> Transformed angular velocity
 */
template <IsFloatingPoint T>
Vec3<T> Transform<T>::apply_to_angular_velocity(const Vec3<T>& ang_vel) const
{
    Vec3<T> rotated_ang_vel = rotation * ang_vel;
    Vec3<T> transformed_ang_vel = rotated_ang_vel + angular_velocity;
    return transformed_ang_vel;
}

/**
 * @brief Compute the velocity of a point rigidly attached to this frame.
 * @param point Point in world coordinates
 * @return Vec3<T> Velocity of the point
 */
template <IsFloatingPoint T>
Vec3<T> Transform<T>::velocity_of_point(const Vec3<T>& point) const
{
    // Compute the velocity of a point rigidly attached to this frame
    // v_point = v_frame + w x r
    // where r is the vector from the frame origin to the point
    Vec3<T> r = point - position;
    Vec3<T> v_from_rotation = cross(angular_velocity, r);
    return velocity + v_from_rotation;
}

/**
 * @brief Compute the velocity of a point specified in local coordinates.
 * @param local_point Point in local coordinates
 * @return Vec3<T> Velocity of the point
 */
template <IsFloatingPoint T>
Vec3<T> Transform<T>::velocity_of_local_point(const Vec3<T>& local_point) const
{
    // Compute velocity of a point specified in local coordinates
    // First transform the point to global space, then compute its velocity
    Vec3<T> global_point = apply_to_point(local_point);
    return velocity_of_point(global_point);
}
} // namespace huira
