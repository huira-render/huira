#pragma once

#include <algorithm>
#include <limits>

#include "embree4/rtcore.h"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
template <IsSpectral TSpectral>
class Ray {
  public:
    Ray() noexcept = default;

    Ray(const Vec3<float>& origin, const Vec3<float>& direction, float tnear = 0.f) noexcept
        : origin_(origin), direction_(direction),
          reciprocal_direction_{1.f / direction.x, 1.f / direction.y, 1.f / direction.z},
          tnear_(tnear)
    {
    }

    // Accessors
    [[nodiscard]] const Vec3<float>& origin() const noexcept { return origin_; }
    [[nodiscard]] const Vec3<float>& direction() const noexcept { return direction_; }
    [[nodiscard]] float tnear() const noexcept { return tnear_; }
    [[nodiscard]] const Vec3<float>& reciprocal_direction() const noexcept
    {
        return reciprocal_direction_;
    }

    // Evaluate ray at parameter t: origin + t * direction
    [[nodiscard]] Vec3<float> at(float t) const noexcept { return origin_ + t * direction_; }

  private:
    Vec3<float> origin_{0, 0, 0};
    Vec3<float> direction_{0, 0, -1};
    Vec3<float> reciprocal_direction_{0, 0, -1};
    float tnear_{0.f};
};

/**
 * @brief Returns a t value strictly beyond @p t for same-ray continuation.
 *
 * Used when a ray passes through a surface (stochastic alpha, transmission)
 * and must keep tracing *along the same parameterization*: the origin and
 * direction are kept bit-exact and only tnear advances. Because the
 * intersectors are deterministic, the surface just crossed reports the same
 * t and is rejected by tnear; everything farther along survives. This is
 * exact — no coordinate-space offset, no scene-scale epsilon.
 *
 * The bump is 4 ULPs of t (plus a denormal floor so t == 0 still advances):
 * this tolerates coplanar triangle pairs that report the same crossing at
 * marginally different t (shared-edge hits), while only ever skipping
 * geometry within 4 ULPs of t along the ray — i.e., below what float32
 * coordinates can represent at that range in the first place.
 */
[[nodiscard]] inline float advance_ray_t(float t) noexcept
{
    constexpr float rel = 4.0f * std::numeric_limits<float>::epsilon();
    return t + std::max(t * rel, std::numeric_limits<float>::denorm_min());
}

struct HitRecord {
    float t = std::numeric_limits<float>::infinity(); ///< Ray parameter at hit
    float u = 0.f;                                    ///< Barycentric u
    float v = 0.f;                                    ///< Barycentric v
    unsigned int inst_id = RTC_INVALID_GEOMETRY_ID;   ///< Instance ID in TLAS
    unsigned int geom_id = RTC_INVALID_GEOMETRY_ID;   ///< Geometry ID in BLAS
    unsigned int prim_id = 0;                         ///< Triangle index
    Vec3<float> Ng{};                                 ///< Geometric face normal (unnormalized)

    [[nodiscard]] bool hit() const noexcept { return inst_id != RTC_INVALID_GEOMETRY_ID; }
};
} // namespace huira
