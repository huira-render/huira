#pragma once

#include <limits>
#include <array>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    struct RayHit {
        float t = std::numeric_limits<float>::infinity();

        std::array<Vertex<TSpectral>, 3> vert;
        Vec3<float> face_normal;
        std::array<float, 3> w;
    };

    template <IsFloatingPoint T>
    inline Vec3<T> offset_intersection_(Vec3<T> intersection, const Vec3<T>& N);
}

#include "huira_impl/core/ray_hit.ipp"
