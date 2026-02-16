#pragma once

#include "huira/core/types.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    /**
     * @brief Surface interaction information for rendering.
     *
     * Stores geometric and shading information at a surface intersection point,
     * including position, normals, barycentric coordinates, and outgoing direction.
     * Used in rendering calculations for lighting, shading, and visibility.
     *
     * @tparam TSpectral Spectral type for the rendering pipeline
     */
    template <IsSpectral TSpectral>
    struct Interaction {
        Vec3<float> position;   ///< Intersection point in world space
        Vec3<float> normal_g;   ///< Geometric normal at the intersection
        Vec3<float> normal_s;   ///< Shading normal at the intersection
        Vec3<float> uvw;        ///< Barycentric coordinates (u, v, w)
        Vec3<float> wo;         ///< Outgoing direction (towards camera)
    };

    /**
     * @brief Offsets an intersection point along a normal to prevent self-intersection artifacts.
     *
     * Uses bit-level manipulation to offset the intersection point in floating-point or integer space,
     * depending on the magnitude, to avoid shadow acne and other precision issues in ray tracing.
     *
     * @tparam T Floating-point type
     * @param intersection The intersection point
     * @param N The geometric normal at the intersection
     * @return Vec3<T> Offset intersection point
     */
    template <IsFloatingPoint T>
    inline Vec3<T> offset_intersection_(Vec3<T> intersection, const Vec3<T>& N);
}

#include "huira_impl/render/interaction.ipp"
