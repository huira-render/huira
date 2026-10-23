#pragma once

#include "huira/materials/bsdfs/bsdf.hpp"

namespace huira {

/**
 * @brief McEwen BSDF for photometric planetary surface rendering.
 *
 * An empirically derived reflectance model by Alfred S. McEwen (1991) that blends
 * Lambertian and Lommel-Seeliger scattering based on the phase angle (the angle
 * between the incident light and the observer).
 *
 * It effectively captures the photometric properties of rough, dusty planetary
 * surfaces like the Moon. The mixing fraction beta is calculated as exp(-alpha / alpha_0),
 * where alpha is the phase angle and alpha_0 is a 60-degree falloff constant.
 *
 * f(wo, wi) = albedo / pi * ((1 - beta) + 2 beta / (cos_i + cos_o))
 *
 * which is McEwen's lunar-Lambert function, I/F = albedo (2 beta cos_i / (cos_i + cos_o) +
 * (1 - beta) cos_i). Its Lommel-Seeliger part is normalized to equal Lambert's where the light
 * and the viewer are both overhead, so that the albedo is the normal albedo whatever beta is.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class McEwenBSDF final : public BSDF<TSpectral> {
  public:
    McEwenBSDF() noexcept = default;

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

    std::string type() const override { return "McEwenBSDF"; }
};

} // namespace huira

#include "huira_impl/materials/bsdfs/mcewen_bsdf.ipp"
