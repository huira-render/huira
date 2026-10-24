#pragma once

#include <cmath>
#include <string>

#include "huira/materials/bsdfs/bsdf.hpp"
#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Hapke BSDF (5-Parameter Model) for planetary regolith.
 *
 * An implementation of the classic 5-parameter Hapke photometric model,
 * widely used for rendering asteroids, moons, and dusty planetary bodies.
 * * It simulates complex particulate scattering including:
 * - Shadow-Hiding Opposition Effect (Surge in brightness at 0 phase angle)
 * - Double Henyey-Greenstein phase function (Forward/Backward scattering)
 * - Isotropic multiple scattering approximation via Chandrasekhar H-functions
 *
 * Parameters:
 * - omega: Single scattering albedo (Provided spatially via params.albedo)
 * - h:     Opposition effect width (porosity/compaction)
 * - B0:    Opposition effect amplitude
 * - b:     DHG phase function asymmetry
 * - c:     DHG phase function forward/backward fraction
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class HapkeBSDF final : public BSDF<TSpectral> {
  public:
    HapkeBSDF(const HapkeBSDF&) = delete;
    HapkeBSDF& operator=(const HapkeBSDF&) = delete;
    HapkeBSDF(HapkeBSDF&&) = delete;
    HapkeBSDF& operator=(HapkeBSDF&&) = delete;

    /// @throws std::runtime_error unless h and B0 are finite and non-negative (h = 0 has no
    /// opposition effect), b is within (-1, 1), where the Henyey-Greenstein lobes are finite, and
    /// c is from 0 to 1.
    HapkeBSDF(float h, float B0, float b, float c) : h_(h), B0_(B0), b_(b), c_(c)
    {
        if (!(h >= 0.f) || !std::isfinite(h) || !(B0 >= 0.f) || !std::isfinite(B0) ||
            !(std::abs(b) < 1.f) || !(c >= 0.f && c <= 1.f)) {
            HUIRA_THROW_ERROR("HapkeBSDF - h and B0 must be finite and non-negative, b within "
                              "(-1, 1), and c from 0 to 1: h " +
                              std::to_string(h) + ", B0 " + std::to_string(B0) + ", b " +
                              std::to_string(b) + ", c " + std::to_string(c));
        }
    }

    [[nodiscard]] BSDFRequirements requirements() const override;

    [[nodiscard]] TSpectral eval(const Vec3<float>& wo,
                                 const Vec3<float>& wi,
                                 const Interaction<TSpectral>& isect,
                                 const ShadingParams<TSpectral>& params) const override;

    [[nodiscard]] BSDFSample<TSpectral> sample(const Vec3<float>& wo,
                                               const Interaction<TSpectral>& isect,
                                               const ShadingParams<TSpectral>& params,
                                               float u1,
                                               float u2) const override;

    [[nodiscard]] float pdf(const Vec3<float>& wo,
                            const Vec3<float>& wi,
                            const Interaction<TSpectral>& isect,
                            const ShadingParams<TSpectral>& params) const override;

    std::string type() const override { return "HapkeBSDF"; }

  private:
    float h_;
    float B0_;
    float b_;
    float c_;

    [[nodiscard]] float phase_function_dhg(float cos_alpha) const noexcept;
    [[nodiscard]] float opposition_effect(float tan_half_alpha) const noexcept;
    [[nodiscard]] TSpectral h_function(float mu, const TSpectral& omega) const noexcept;
};

} // namespace huira

#include "huira_impl/materials/bsdfs/hapke_bsdf.ipp"
