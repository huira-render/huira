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

        // Apply translation
        result[0][3] = translation.x;
        result[1][3] = translation.y;
        result[2][3] = translation.z;

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

        // Inverse translation: rotate and scale the negated translation
        Vec3<T> scaled_translation{
            translation.x / scale.x,
            translation.y / scale.y,
            translation.z / scale.z
        };
        result.translation = result.rotation * -scaled_translation;

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

        // Combine translations
        Vec3<T> scaled_translation{
            translation.x * b.scale.x,
            translation.y * b.scale.y,
            translation.z * b.scale.z
        };
        result.translation = (b.rotation * scaled_translation) + b.translation;

        return result;
    }
}
