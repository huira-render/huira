#pragma once

#include "embree4/rtcore.h"

#include "huira/core/types.hpp"
#include "huira/units/units.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/geometry/geometry.hpp"

namespace huira {

    template <IsSpectral TSpectral>
    class Ellipsoid : public Geometry<TSpectral> {
    public:
        Ellipsoid(const units::Meter& x, const units::Meter& y, const units::Meter& z) :
            radii_{ x.to_si_f(), y.to_si_f(), z.to_si_f() }
        {}
        ~Ellipsoid() override = default;

        Ellipsoid(const Ellipsoid&) = delete;
        Ellipsoid& operator=(const Ellipsoid&) = delete;
        
        Ellipsoid(Ellipsoid&&) noexcept = default;
        Ellipsoid& operator=(Ellipsoid&&) noexcept = default;

        // Geometry overrides
        void compute_surface_interaction(const HitRecord& hit, Interaction<TSpectral>& isect) const override;
        Vec2<float> compute_uv(const HitRecord& hit) const override;

        std::string type() const override { return "Ellipsoid"; }

        [[nodiscard]] Vec3<float> radii() const { return radii_; }

    private:
        Vec3<float> radii_;

        void build_blas_() const override;
        
        static void bounds_callback(const RTCBoundsFunctionArguments* args) noexcept;
        static void intersect_callback(const RTCIntersectFunctionNArguments* args) noexcept;
    };

}

#include "huira_impl/geometry/ellipsoid.ipp"
