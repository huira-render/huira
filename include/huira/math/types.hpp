#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"

#include "huira/concepts/numeric_concepts.hpp"

namespace huira {
    template <int Rows, int Cols, IsFloatingPoint T>
    using Mat = glm::mat<Cols, Rows, T, glm::highp>;

    template <int N, IsFloatingPoint T>
    using Vec = glm::vec<N, T, glm::highp>;

    template <IsFloatingPoint T>
    using Mat2 = Mat<2, 2, T>;

    template <IsFloatingPoint T>
    using Mat3 = Mat<3, 3, T>;

    template <IsFloatingPoint T>
    using Mat4 = Mat<4, 4, T>;

    template <IsFloatingPoint T>
    using Vec2 = Vec<2, T>;

    template <IsFloatingPoint T>
    using Vec3 = Vec<3, T>;

    template <IsFloatingPoint T>
    using Vec4 = Vec<4, T>;



    // Quaternion aliases
    template<IsFloatingPoint T>
    using Quaternion = glm::qua<T, glm::highp>;  // GLM/Hamilton: (w, x, y, z)

    template <IsFloatingPoint T>
    using ShusterQuaternion = Vec4<T>;  // Shuster: (x, y, z, w) - common in aerospace

    template <IsFloatingPoint T>
    ShusterQuaternion<T> toShuster(const Quaternion<T>& q) {
        return ShusterQuaternion<T>(q.x, q.y, q.z, q.w);
    }

    template <IsFloatingPoint T>
    Quaternion<T> toHamilton(const ShusterQuaternion<T>& q) {
        return Quaternion<T>(q.w, q.x, q.y, q.z);
    }
}