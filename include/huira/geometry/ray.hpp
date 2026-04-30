#pragma once

#include <limits>

#include "embree4/rtcore.h"

#include "huira/core/types.hpp"
#include "huira/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Ray {
    public:
        Ray() noexcept = default;

        Ray(const Vec3<float>& origin, const Vec3<float>& direction) noexcept
            : origin_(origin)
            , direction_(direction)
            , reciprocal_direction_{ 1.f / direction.x,
                                     1.f / direction.y,
                                     1.f / direction.z }
        {

        }

        // Accessors
        [[nodiscard]] const Vec3<float>& origin() const noexcept { return origin_; }
        [[nodiscard]] const Vec3<float>& direction() const noexcept { return direction_; }
        [[nodiscard]] const Vec3<float>& reciprocal_direction() const noexcept { return reciprocal_direction_; }

        // Evaluate ray at parameter t: origin + t * direction
        [[nodiscard]] Vec3<float> at(float t) const noexcept { return origin_ + t * direction_; }

    private:
        Vec3<float> origin_{ 0,0,0 };
        Vec3<float> direction_{ 0,0,-1 };
        Vec3<float> reciprocal_direction_{ 0,0,-1 };
    };

    struct HitRecord {
        float t = std::numeric_limits<float>::infinity();  ///< Ray parameter at hit
        float u = 0.f;                ///< Barycentric u
        float v = 0.f;                ///< Barycentric v
        unsigned int inst_id = RTC_INVALID_GEOMETRY_ID;  ///< Instance ID in TLAS
        unsigned int geom_id = RTC_INVALID_GEOMETRY_ID;  ///< Geometry ID in BLAS
        unsigned int prim_id = 0;     ///< Triangle index
        Vec3<float> Ng{};             ///< Geometric face normal (unnormalized)

        [[nodiscard]] bool hit() const noexcept {
            return inst_id != RTC_INVALID_GEOMETRY_ID;
        }
    };
}
