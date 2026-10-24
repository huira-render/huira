#pragma once

// The parts of CookTorranceBSDF's specular lobe that its albedo table is computed from: the
// masking-shadowing term, the Fresnel term, the dielectric's f0 and the least roughness. The BSDF
// evaluates them in float, and tools/build_cook_torrance_albedo, which builds the table in
// cook_torrance_albedo.ipp, in double, from this one header.
//
// Change anything here, and the table must be rebuilt: test_cook_torrance fails until it is (the
// table records a fingerprint of this file; see that tool). The lobe's distribution is GGX
// (sampling::ggx_distribution()), which the tool's quadrature assumes.
//
// This header uses the standard library only, so that the tool builds without huira's
// dependencies.

#include <algorithm>
#include <cmath>
#include <concepts>
#include <limits>

namespace huira::detail::cook_torrance {

/// The least roughness, which keeps a perfectly smooth surface's lobe from being singular.
inline constexpr float MIN_ROUGHNESS = 0.01f;

/// A dielectric's reflectance at normal incidence (an index of refraction of 1.5).
inline constexpr float DIELECTRIC_F0 = 0.04f;

/**
 * @brief Smith's Lambda for GGX: the fraction of the microsurface seen from direction v that
 * other microfacets hide, relative to the part that is seen, (sqrt(1 + alpha^2 tan^2) - 1) / 2.
 *
 * @param n_dot_v The cosine of v's angle from the normal.
 * @param alpha2 alpha^2, where alpha is the roughness squared.
 */
template <std::floating_point T>
[[nodiscard]] T smith_lambda(T n_dot_v, T alpha2) noexcept
{
    // cos^2 is kept from 0, not from 1e-8, which would compute every direction within 1e-4 of
    // the horizon as if it were at 1e-4, and mask far too little there:
    const T cos2 = n_dot_v * n_dot_v;
    const T tan2 = (T{1} - cos2) / std::max(cos2, std::numeric_limits<T>::min());
    return T{0.5} * (std::sqrt(T{1} + alpha2 * tan2) - T{1});
}

/// Smith's masking: the fraction of the microfacets facing v that v sees, 1 / (1 + Lambda(v)).
template <std::floating_point T>
[[nodiscard]] T smith_G1(T n_dot_v, T alpha2) noexcept
{
    return T{1} / (T{1} + smith_lambda(n_dot_v, alpha2));
}

/**
 * @brief Smith's height-correlated masking-shadowing, 1 / (1 + Lambda(wo) + Lambda(wi)) (Heitz
 * 2014; glTF 2.0, Appendix B). A microfacet high on the surface is likelier to be both seen and
 * lit, so masking and shadowing are not independent: their product, G1(wo) G1(wi), shadows too
 * much, most of all for rough surfaces at grazing angles.
 */
template <std::floating_point T>
[[nodiscard]] T smith_G2(T n_dot_wo, T n_dot_wi, T alpha2) noexcept
{
    return T{1} / (T{1} + smith_lambda(n_dot_wo, alpha2) + smith_lambda(n_dot_wi, alpha2));
}

/**
 * @brief The weight of Schlick's Fresnel approximation, (1 - cos)^5: the reflectance is
 * f0 + (1 - f0) * weight, for the cosine of the angle between the direction and the microfacet
 * normal.
 */
template <std::floating_point T>
[[nodiscard]] T schlick_weight(T cos_theta) noexcept
{
    // cos_theta, from the dot product of a direction and a normalized half vector, can round to
    // just over 1, which would make t^5 negative and the reflectance below f0 (below 0 for f0 = 0).
    const T t = T{1} - std::clamp(cos_theta, T{0}, T{1});
    const T t2 = t * t;
    return t2 * t2 * t;
}

} // namespace huira::detail::cook_torrance
