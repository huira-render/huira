#pragma once

#include <cmath>
#include <string>

#include "embree4/rtcore.h"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/geometry/geometry.hpp"
#include "huira/units/units.hpp"
#include "huira/util/logger.hpp"

namespace huira {

template <IsSpectral TSpectral>
class Ellipsoid : public Geometry<TSpectral> {
  public:
    /// @throws std::runtime_error unless each radius is positive and finite (in single
    /// precision, as it is kept).
    Ellipsoid(const units::Meter& x, const units::Meter& y, const units::Meter& z)
        : radii_{x.to_si_f(), y.to_si_f(), z.to_si_f()}
    {
        for (int i = 0; i < 3; ++i) {
            if (!(radii_[i] > 0.f) || !std::isfinite(radii_[i])) {
                HUIRA_THROW_ERROR("Ellipsoid - Each radius must be positive and finite: " +
                                  std::to_string(x.to_si()) + ", " + std::to_string(y.to_si()) +
                                  ", " + std::to_string(z.to_si()) + " m");
            }
        }
    }
    ~Ellipsoid() override = default;

    Ellipsoid(const Ellipsoid&) = delete;
    Ellipsoid& operator=(const Ellipsoid&) = delete;

    Ellipsoid(Ellipsoid&&) noexcept = default;
    Ellipsoid& operator=(Ellipsoid&&) noexcept = default;

    // Geometry overrides
    void compute_surface_interaction(const HitRecord& hit,
                                     Interaction<TSpectral>& isect) const override;
    Vec2<float> compute_uv(const HitRecord& hit) const override;

    std::string type() const override { return "Ellipsoid"; }

    [[nodiscard]] Vec3<float> radii() const { return radii_; }

  private:
    Vec3<float> radii_;

    void build_blas_() const override;

    static void bounds_callback(const RTCBoundsFunctionArguments* args) noexcept;
    static void intersect_callback(const RTCIntersectFunctionNArguments* args) noexcept;
};

} // namespace huira

#include "huira_impl/geometry/ellipsoid.ipp"
