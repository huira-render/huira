#pragma once

#include <cmath>
#include <limits>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

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
    Vec3<float> position; ///< Intersection point in world space
    Vec3<float> normal_g; ///< Geometric normal at the intersection
    Vec3<float> normal_s; ///< Shading normal at the intersection

    Vec3<float> tangent;   ///< Tangent vector (for normal mapping / anisotropic BSDFs)
    Vec3<float> bitangent; ///< Bitangent vector (cross(normal_s, tangent))

    Vec2<float> uv; ///< Texture coordinates (u, v)

    Vec3<float> wo; ///< Outgoing direction (towards camera), world space

    TSpectral vertex_albedo{1}; ///< Interpolated vertex color (default: white / unity)

    /// Conservative bound on the numerical error of `position` (meters).
    /// Set by SceneView::resolve_hit from the local- and world-frame
    /// coordinate magnitudes; consumed by offset_spawn_point().
    float p_err{0.0f};
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
inline void build_default_tangent_frame(const Vec3<float>& normal_s,
                                        Vec3<float>& tangent,
                                        Vec3<float>& bitangent) noexcept
{
    const float sign = std::copysign(1.0f, normal_s.z);
    const float a = -1.0f / (sign + normal_s.z);
    const float b = normal_s.x * normal_s.y * a;
    tangent = Vec3<float>{1.0f + sign * normal_s.x * normal_s.x * a, sign * b, -sign * normal_s.x};
    bitangent = Vec3<float>{b, sign + normal_s.y * normal_s.y * a, -normal_s.y};
}

/**
 * @brief Relative scale used to bound the numerical error of a resolved hit position.
 *
 * The world-space interaction position is produced by (a) reconstructing /
 * interpolating the point in the primitive's local frame and (b) applying the
 * instance transform. Both steps accumulate a small number of ULPs of the
 * *larger* of the local- and world-frame coordinate magnitudes (rotation and
 * interpolation error scale with the local magnitude; the final translation
 * rounds at the world magnitude). Measured error is 1-3 ULPs; 16 ULPs
 * (16 * FLT_EPSILON as a relative factor) gives >5x headroom on both sides.
 *
 * Consequences at scale: for a hit at camera-relative range R, the spawn
 * offset is <= ~1.9e-6 * max(R, |p_local|). At lunar range (3.8e8 m) this is
 * ~725 m — well under e.g. a 6 km cloud/surface gap — and the angular
 * displacement seen from the camera is <2 microradians, below typical
 * optical-navigation pixel IFOVs. (Contrast: the previous 256-ULP
 * Waechter-Binder offset was ~8.2 km at that range.)
 */
inline constexpr float SPAWN_POINT_ERROR_SCALE = 16.0f * std::numeric_limits<float>::epsilon();

/**
 * @brief Offsets a spawn point along the (already side-flipped) geometric normal
 *        by the propagated position-error bound.
 *
 * Used only for rays whose direction differs from the incident ray (BSDF
 * bounces, shadow rays); a displacement along the geometric normal guarantees
 * clearance from the surface's tangent plane for *every* spawn direction,
 * including grazing ones — a property a tnear interval cannot provide, since
 * the parametric distance to a false re-hit grows as 1/sin(theta) at grazing
 * incidence. Rays that keep their direction (alpha pass-through,
 * transmittance marching) must NOT use this: they continue the same ray via
 * Ray::tnear() and advance_ray_t(), which is exact.
 *
 * @param p       Resolved intersection position (world space)
 * @param n       Geometric normal, pre-flipped toward the spawn hemisphere
 * @param p_err   Position error bound from Interaction::p_err
 */
inline Vec3<float> offset_spawn_point(const Vec3<float>& p,
                                      const Vec3<float>& n,
                                      float p_err) noexcept
{
    return p + n * p_err;
}

/**
 * @brief Offsets an intersection point along a normal to prevent self-intersection artifacts.
 * @deprecated Superseded by same-ray continuation (Ray::tnear + advance_ray_t) for
 *             direction-preserving rays and offset_spawn_point() for respawned rays.
 *             Its fixed 256-ULP displacement exceeds scene feature sizes at planetary
 *             camera-relative ranges (~8.2 km at 3.8e8 m). Retained temporarily for
 *             any out-of-tree callers; remove once none remain.
 */
template <IsFloatingPoint T>
inline Vec3<T> offset_intersection_(Vec3<T> intersection, const Vec3<T>& N);
} // namespace huira

#include "huira_impl/render/interaction.ipp"
