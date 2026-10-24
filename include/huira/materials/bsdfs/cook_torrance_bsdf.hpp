#pragma once

#include "huira/materials/bsdfs/bsdf.hpp"
#include "huira/materials/bsdfs/cook_torrance_lobe.hpp"

namespace huira {

/**
 * @brief Cook-Torrance microfacet BSDF with metallic-roughness parameterization.
 *
 * Implements the Cook-Torrance microfacet reflectance model, combining a
 * specular microfacet lobe with a Lambertian diffuse lobe, blended by the
 * metallic parameter.
 *
 * The full model at a surface point is:
 *
 *   f(wo, wi) = (1 - metallic) * albedo / pi
 *                 * (1 - E(cos_i)) * (1 - E(cos_o)) / (1 - E_avg)
 *             + D(h) * F(wo, h) * G(wo, wi) / (4 * |cos_o| * |cos_i|)
 *
 * The individual components are:
 *
 *   - **D** (Normal Distribution Function): GGX / Trowbridge-Reitz.
 *     Models the statistical distribution of microfacet orientations on the
 *     surface. GGX produces a longer specular tail than Beckmann, giving
 *     more realistic highlights on rough surfaces.
 *
 *   - **F** (Fresnel): Schlick approximation.
 *     Models the increase in reflectance at grazing angles. For dielectrics,
 *     F0 ~= 0.04. For conductors (metals), F0 equals the base color.
 *
 *   - **G** (Geometry / Masking-Shadowing): Smith height-correlated.
 *     Models self-shadowing and masking between microfacets, matched to
 *     the GGX normal distribution for physical consistency.
 *
 *   - **E** (Directional albedo of the dielectric's specular lobe): the light
 *     the lobe, with F0 = 0.04, reflects of a unit irradiance from a direction
 *     at that cosine to the normal, and **E_avg** its cosine-weighted average.
 *     The diffuse lobe gets only the light the specular lobe does not
 *     reflect (Kelemen and Szirmay-Kalos 2001): of the light toward wo, the
 *     specular lobe reflects E(cos_o) and the diffuse lobe albedo * (1 -
 *     E(cos_o)), so a white dielectric reflects all the light it receives and
 *     never more, and the BSDF stays reciprocal. E has no closed form; it is
 *     tabulated, never below its true value, by tools/build_cook_torrance_albedo,
 *     from the parts of the lobe in cook_torrance_lobe.hpp. Change those, and
 *     test_cook_torrance fails until the table is rebuilt.
 *     (glTF's sample implementation weights the diffuse lobe by 1 - F(wo.h)
 *     instead, which at grazing angles reflects up to 1.8 times the light.)
 *
 * The metallic parameter controls the blend: at metallic = 0, the surface
 * is a dielectric with a diffuse base and a weak specular highlight. At
 * metallic = 1, the surface is a pure conductor with no diffuse component
 * and a colored specular reflection. This follows the glTF 2.0
 * metallic-roughness parameterization.
 *
 * All spatially-varying parameters (albedo, roughness, metallic) come
 * from ShadingParams, evaluated by the Material. This BSDF is stateless
 * with respect to surface point and a single instance is shared across
 * all materials using this model.
 *
 * Importance sampling uses VNDF (Visible Normal Distribution Function)
 * sampling (Heitz 2018) for the specular lobe, combined with cosine-weighted
 * hemisphere sampling for the diffuse lobe via one-sample MIS.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class CookTorranceBSDF final : public BSDF<TSpectral> {
  public:
    CookTorranceBSDF() noexcept = default;

    CookTorranceBSDF(const CookTorranceBSDF&) = delete;
    CookTorranceBSDF& operator=(const CookTorranceBSDF&) = delete;
    CookTorranceBSDF(CookTorranceBSDF&&) = delete;
    CookTorranceBSDF& operator=(CookTorranceBSDF&&) = delete;

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

    std::string type() const override { return "CookTorranceBSDF"; }

  private:
    /// Minimum roughness to prevent numerical singularity at perfect specularity.
    static constexpr float min_roughness_ = detail::cook_torrance::MIN_ROUGHNESS;

    [[nodiscard]] static float
    ggx_D(const Vec3<float>& n, const Vec3<float>& h, float alpha2) noexcept;
};

} // namespace huira

#include "huira_impl/materials/bsdfs/cook_torrance_bsdf.ipp"
