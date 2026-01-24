#pragma once

#include "huira/core/types.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

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
}
