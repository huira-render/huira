#pragma once

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Ray {
    public:
        Ray() noexcept = default;

        Ray(const Vec3<TFloat>& origin, const Vec3<TFloat>& direction) noexcept
            : origin_(origin)
            , direction_(direction)
            , reciprocal_direction_{ TFloat{1} / direction.x,
                                     TFloat{1} / direction.y,
                                     TFloat{1} / direction.z }
        {

        }

        // Accessors
        [[nodiscard]] const Vec3<TFloat>& origin() const noexcept { return origin_; }
        [[nodiscard]] const Vec3<TFloat>& direction() const noexcept { return direction_; }
        [[nodiscard]] const Vec3<TFloat>& reciprocal_direction() const noexcept { return reciprocal_direction_; }

        // Evaluate ray at parameter t: origin + t * direction
        [[nodiscard]] Vec3<TFloat> at(TFloat t) const noexcept { return origin_ + t * direction_; }

    private:
        Vec3<TFloat> origin_{ 0,0,0 };
        Vec3<TFloat> direction_{ 0,0,-1 };
        Vec3<TFloat> reciprocal_direction_{ 0,0,-1 };
    };
}
