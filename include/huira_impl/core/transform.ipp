#include "huira/detail/concepts/numeric_concepts.hpp"

namespace huira {
    template <IsFloatingPoint T>
    Mat4<T> Transform<T>::to_matrix() const
    {
        // Create transformation matrix: T * R * S
        Mat4<T> result{};

        // Apply scale and rotation
        Mat4<T> rot = rotation.get_matrix();
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                result[i][j] = rot[i][j] * scale[j];
            }
        }

        // Apply position
        result[0][3] = position.x;
        result[1][3] = position.y;
        result[2][3] = position.z;

        // Set homogeneous coordinate
        result[3][3] = T(1);

        return result;
    }

    template <IsFloatingPoint T>
    Transform<T> Transform<T>::inverse() const
    {
        Transform<T> result;

        // Inverse scale: 1 / scale for each component
        result.scale = Vec3<T>(T(1) / scale.x, T(1) / scale.y, T(1) / scale.z);

        // Inverse rotation
        result.rotation = rotation.inverse();

        // Inverse position: rotate and scale the negated position
        Vec3<T> scaled_position{
            position.x / scale.x,
            position.y / scale.y,
            position.z / scale.z
        };
        result.position = result.rotation * -scaled_position;

        // Inverse velocity: transform back and account for rotating frame
        // v_inv = R^-1 * (v_frame - w x p)
        Vec3<T> vel_contribution = cross(angular_velocity, position);
        result.velocity = result.rotation * (velocity - vel_contribution);

        // Inverse angular velocity: just rotate back
        result.angular_velocity = result.rotation * -angular_velocity;

        return result;
    }

    template <IsFloatingPoint T>
    Transform<T> Transform<T>::operator* (const Transform<T>& b) const
    {
        Transform<T> result;

        // Combine scales (component-wise multiplication)
        result.scale = Vec3<T>(scale.x * b.scale.x, scale.y * b.scale.y, scale.z * b.scale.z);

        // Combine rotations (quaternion multiplication)
        result.rotation = b.rotation * rotation;

        // Combine positions
        Vec3<T> scaled_position{
            position.x * b.scale.x,
            position.y * b.scale.y,
            position.z * b.scale.z
        };
        result.position = (b.rotation * scaled_position) + b.position;

        // Combine velocities: v_result = v_b + w_b x p_transformed + R_b * v_a
        Vec3<T> transformed_pos = b.rotation * scaled_position;
        Vec3<T> vel_from_rotation = cross(b.angular_velocity, transformed_pos);
        result.velocity = b.velocity + vel_from_rotation + (b.rotation * velocity);

        // Combine angular velocities: w_result = w_b + R_b * w_a
        result.angular_velocity = b.angular_velocity + (b.rotation * angular_velocity);

        return result;
    }

    template <IsFloatingPoint T>
    Vec3<T> Transform<T>::apply_to_point(const Vec3<T>& point) const
    {
        // Static transformation: scale, rotate, translate
        Vec3<T> scaled_point{
            point.x * scale.x,
            point.y * scale.y,
            point.z * scale.z
        };

        Vec3<T> rotated_point = rotation * scaled_point;
        Vec3<T> transformed_point = rotated_point + position;
        return transformed_point;
    }

    template <IsFloatingPoint T>
    Vec3<T> Transform<T>::apply_to_direction(const Vec3<T>& dir) const
    {
        // Directions don't get translated, only scaled and rotated
        Vec3<T> scaled_dir{
            dir.x * scale.x,
            dir.y * scale.y,
            dir.z * scale.z
        };

        Vec3<T> rotated_dir = rotation * scaled_dir;
        return rotated_dir;
    }

    template <IsFloatingPoint T>
    Vec3<T> Transform<T>::apply_to_velocity(const Vec3<T>& vel) const
    {
        // Velocities rotate and add together
        Vec3<T> scaled_vel{
            vel.x * scale.x,
            vel.y * scale.y,
            vel.z * scale.z
        };

        Vec3<T> rotated_vel = rotation * scaled_vel;
        Vec3<T> transformed_vel = rotated_vel + velocity;
        return transformed_vel;
    }

    template <IsFloatingPoint T>
    Vec3<T> Transform<T>::apply_to_angular_velocity(const Vec3<T>& ang_vel) const
    {
        Vec3<T> rotated_ang_vel = rotation * ang_vel;
        Vec3<T> transformed_ang_vel = rotated_ang_vel + angular_velocity;
        return transformed_ang_vel;
    }


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

    template <IsFloatingPoint T>
    Vec3<T> Transform<T>::velocity_of_local_point(const Vec3<T>& local_point) const
    {
        // Compute velocity of a point specified in local coordinates
        // First transform the point to global space, then compute its velocity
        Vec3<T> global_point = apply_to_point(local_point);
        return velocity_of_point(global_point);
    }
}
