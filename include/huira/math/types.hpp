#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtx/io.hpp"

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


    // Helpful typedefs:
    typedef Mat2<float> Mat2_f;
    typedef Mat2<double> Mat2_d;

    typedef Mat3<float> Mat3_f;
    typedef Mat3<double> Mat3_d;

    typedef Mat4<float> Mat4_f;
    typedef Mat4<double> Mat4_d;

    typedef Vec2<float> Vec2_f;
    typedef Vec2<double> Vec2_d;

    typedef Vec3<float> Vec3_f;
    typedef Vec3<double> Vec3_d;

    typedef Vec4<float> Vec4_f;
    typedef Vec4<double> Vec4_d;

    typedef Quaternion<float> Quaternion_f;
    typedef Quaternion<double> Quaternion_d;

    typedef ShusterQuaternion<float> ShusterQuaternion_f;
    typedef ShusterQuaternion<double> ShusterQuaternion_d;
}