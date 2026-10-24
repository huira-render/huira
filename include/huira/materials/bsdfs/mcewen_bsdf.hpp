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
 * The lunar-Lambert function is a fit of I/F, not a physical model, and does not conserve
 * energy: of the light from a direction at cosine mu to the normal, it reflects albedo * rho(mu),
 * where rho rises from 1.09 overhead to 1.84 at the horizon. So that the BSDF never reflects
 * more light than it receives, the albedo is capped, for each pair of directions, at
 * 1 / max(rho(cos_i), rho(cos_o)). At an albedo up to 1 / 1.84 = 0.54 the cap is never reached,
 * and the BSDF is the lunar-Lambert function exactly; above it, the cap applies only where one
 * of the directions is low enough. The BSDF stays reciprocal and continuous. (At an albedo of 1,
 * it reflects 0.89 of the light from overhead, where the function reflects 1.09, and all of the
 * light from near the horizon, where the function reflects up to 1.84.)
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class McEwenBSDF final : public BSDF<TSpectral> {
  public:
    McEwenBSDF() noexcept;

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
