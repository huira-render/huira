#pragma once

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    struct Interaction {
        Vec3<float> position; // Intersection point

        Vec3<float> normal_g; // Geometric Normal
        Vec3<float> normal_s; // Shading Normal

        Vec2<float> uv;       // Barycentric Coordinates (u,v)

        Vec3<float> wo;       // Vector towards camera ("out")
    };

    template <IsFloatingPoint T>
    inline Vec3<T> offset_intersection_(Vec3<T> intersection, const Vec3<T>& N);
}

#include "huira_impl/core/interaction.ipp"
