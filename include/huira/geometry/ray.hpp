#pragma once

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

/* Compute a conservative tnear for a ray spawned from a hit point. The hit
 * point was computed as p = origin + hit_t * direction, so its float roundoff
 * error is bounded from the magnitudes of those operands. Project that bound
 * onto the spawned ray direction so we reject self-hits caused by numerical
 * error without moving the origin in coordinate space. */

template <IsSpectral TSpectral>
inline float spawn_ray_tnear(const Ray<TSpectral>& ray, float hit_t, const Vec3<float>& direction) noexcept
{
    return 1e-6f * glm::dot(glm::abs(direction),
                             glm::abs(ray.origin()) + hit_t * glm::abs(ray.direction()));
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
