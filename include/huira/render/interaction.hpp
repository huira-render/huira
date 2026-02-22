#pragma once

#include "huira/core/types.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    /**
     * @brief Surface interaction information for rendering.
     *
     * Stores geometric and shading information at a surface intersection point,
     * including position, normals, tangent frame, texture coordinates, interpolated
     * vertex albedo, and outgoing direction. Used in BSDF evaluation, texture
     * lookup, and light transport calculations.
     *
     * @tparam TSpectral Spectral type for the rendering pipeline
     */
    template <IsSpectral TSpectral>
    struct Interaction {
        Vec3<float> position;       ///< Intersection point in world space
        Vec3<float> normal_g;       ///< Geometric normal at the intersection
        Vec3<float> normal_s;       ///< Shading normal at the intersection

        Vec3<float> tangent;        ///< Tangent vector (for normal mapping / anisotropic BSDFs)
        Vec3<float> bitangent;      ///< Bitangent vector (cross(normal_s, tangent))

        Vec3<float> uvw;            ///< Barycentric coordinates (u, v, w) for interpolation
        Vec2<float> uv;             ///< Texture coordinates for material/texture lookup

        Vec3<float> wo;             ///< Outgoing direction (towards camera), world space

        TSpectral vertex_albedo{ 1 }; ///< Interpolated vertex color (default: white / unity)
    };

    /**
     * @brief Constructs a local shading frame (tangent, bitangent, normal) from
     *        a shading normal, falling back to an arbitrary basis when no UV-derived
     *        tangent is available.
     *
     * Uses the Duff et al. (2017) method for robust orthonormal basis construction.
     *
     * @param normal_s The shading normal (must be normalized)
     * @param tangent  [out] Computed tangent vector
     * @param bitangent [out] Computed bitangent vector
     */
    inline void build_default_tangent_frame(
        const Vec3<float>& normal_s,
        Vec3<float>& tangent,
        Vec3<float>& bitangent) noexcept
    {
        const float sign = std::copysign(1.0f, normal_s.z);
        const float a = -1.0f / (sign + normal_s.z);
        const float b = normal_s.x * normal_s.y * a;
        tangent = Vec3<float>{ 1.0f + sign * normal_s.x * normal_s.x * a, sign * b, -sign * normal_s.x };
        bitangent = Vec3<float>{ b, sign + normal_s.y * normal_s.y * a, -normal_s.y };
    }

    /**
     * @brief Offsets an intersection point along a normal to prevent self-intersection artifacts.
     */
    template <IsFloatingPoint T>
    inline Vec3<T> offset_intersection_(Vec3<T> intersection, const Vec3<T>& N);
}

#include "huira_impl/render/interaction.ipp"
