#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"

#include "huira/math/types.hpp"
#include "huira/math/rotation.hpp"

namespace huira {
    template <IsFloatingPoint T>
    struct Transform {
        Vec3<T> translation{ 0,0,0 };
        Rotation<T> rotation{};
        Vec3<T> scale{ 1,1,1 };

        Mat4<T> to_matrix() const;
        Transform<T> inverse() const;
        Transform<T> operator* (const Transform<T>& b) const;
    };
}

#include "huira_impl/math/transform.ipp"
