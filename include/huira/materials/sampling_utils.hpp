#pragma once

#include <cmath>
#include <numbers>

#include "huira/core/types.hpp"

namespace huira::sampling {

/// Result of a hemisphere sampling operation.
struct HemisphereSample {
    Vec3<float> direction; ///< Sampled direction in local space (+Z = normal)
    float pdf;             ///< Probability density of the sample
};

// =========================================================================
//  Basic hemisphere sampling strategies
// =========================================================================

/**
 * @brief Uniform hemisphere sampling.
 * PDF = 1 / (2 * pi)
 */
[[nodiscard]] inline HemisphereSample uniform_hemisphere(float u1, float u2) noexcept
{
    const float z = u1;
    const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const float phi = 2.0f * std::numbers::pi_v<float> * u2;
    return {.direction = {r * std::cos(phi), r * std::sin(phi), z},
            .pdf = 1.0f / (2.0f * std::numbers::pi_v<float>)};
}

/**
 * @brief Cosine-weighted hemisphere sampling (Malley's method).
 * PDF = cos(theta) / pi
 */
[[nodiscard]] inline HemisphereSample cosine_hemisphere(float u1, float u2) noexcept
{
    const float r = std::sqrt(u1);
    const float phi = 2.0f * std::numbers::pi_v<float> * u2;
    const float x = r * std::cos(phi);
    const float y = r * std::sin(phi);
    const float z = std::sqrt(std::max(0.0f, 1.0f - u1));
    return {.direction = {x, y, z}, .pdf = z * std::numbers::inv_pi_v<float>};
}

/**
 * @brief Power-cosine hemisphere sampling: cos^n(theta).
 * PDF = (n + 1) / (2 * pi) * cos^n(theta)
 */
[[nodiscard]] inline HemisphereSample power_cosine_hemisphere(float u1, float u2, float n) noexcept
{
    const float z = std::pow(u1, 1.0f / (n + 1.0f));
    const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    const float phi = 2.0f * std::numbers::pi_v<float> * u2;
    return {.direction = {r * std::cos(phi), r * std::sin(phi), z},
            .pdf = (n + 1.0f) / (2.0f * std::numbers::pi_v<float>)*std::pow(z, n)};
}

/**
 * @brief Sine-weighted hemisphere sampling.
 * Biases samples toward the horizon. Useful for crater interior rendering
 * where zenith contributions are occluded and the primary contributing
 * directions are near-horizontal.
 */
[[nodiscard]] inline HemisphereSample sine_hemisphere(float u1, float u2) noexcept
{
    const float cos_theta = std::cbrt(1.0f - u1);
    const float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));
    const float phi = 2.0f * std::numbers::pi_v<float> * u2;

    const float pdf = 2.0f * sin_theta / (std::numbers::pi_v<float> * std::numbers::pi_v<float>);

    return {.direction = {sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta},
            .pdf = std::max(pdf, 1e-8f)};
}

// =========================================================================
//  GGX / Microfacet sampling
// =========================================================================

/// Result of a microfacet normal sampling operation.
struct MicrofacetSample {
    Vec3<float> half_vector; ///< Sampled microfacet normal in local space
    float pdf;               ///< PDF with respect to the half-vector solid angle
};

/**
 * @brief The GGX (Trowbridge-Reitz) normal distribution D(h), from the squared cosine and sine
 * of the angle between the microfacet normal h and the surface normal.
 *
 * D = alpha^2 / (pi (cos^2 (alpha^2 - 1) + 1)^2), computed here as alpha^2 / (pi (sin^2 +
 * cos^2 alpha^2)^2). The two are equal, but the first, in float, loses alpha^2 on smooth
 * surfaces: near h = n it subtracts numbers close to 1, whose rounding error (6e-8) is as large
 * as alpha^2 at roughness 0.016, and below roughness 0.013, alpha^2 - 1 rounds to -1, leaving a
 * denominator of 0 and an infinite D. Give sin^2 as computed directly (from a cross product, or
 * from local coordinates), not as 1 - cos^2: cos rounds to 1 within 2.4e-4 rad of the normal,
 * which would flatten the lobe of a surface smoother than that, and make it reflect several
 * times the light it should (18 times at roughness 0.01).
 *
 * @param cos2 Squared cosine of the angle between h and the normal (h above the surface)
 * @param sin2 Squared sine of that angle
 * @param alpha2 Roughness to the fourth power (alpha = roughness^2)
 */
[[nodiscard]] inline float ggx_distribution(float cos2, float sin2, float alpha2) noexcept
{
    const float denom = sin2 + cos2 * alpha2;
    return alpha2 / (std::numbers::pi_v<float> * denom * denom);
}

/**
 * @brief Samples a microfacet normal from the GGX (Trowbridge-Reitz) distribution.
 * PDF (with respect to half-vector) = D(h) * cos(theta_h)
 */
[[nodiscard]] inline MicrofacetSample
ggx_sample_half_vector(float u1, float u2, float roughness) noexcept
{
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;

    // tan^2 = alpha^2 u1 / (1 - u1): the usual cos^2 = (1 - u1) / (1 + (alpha^2 - 1) u1) is 1
    // for every u1 on smooth surfaces (see ggx_distribution()).
    const float tan_theta2 = alpha2 * u1 / std::max(1.0f - u1, 1e-7f);
    const float cos_theta2 = 1.0f / (1.0f + tan_theta2);
    const float sin_theta2 = tan_theta2 * cos_theta2;
    const float cos_theta = std::sqrt(cos_theta2);
    const float sin_theta = std::sqrt(sin_theta2);
    const float phi = 2.0f * std::numbers::pi_v<float> * u2;

    const Vec3<float> h = {sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta};

    const float D = ggx_distribution(cos_theta2, sin_theta2, alpha2);
    const float pdf = D * cos_theta;

    return {.half_vector = h, .pdf = std::max(pdf, 1e-8f)};
}

/**
 * @brief GGX Visible Normal Distribution Function sampling (Heitz 2018).
 *
 * Samples the VNDF: D(h) * G1(wo, h) * max(0, dot(wo, h)) / wo.z.
 * Produces significantly lower variance than naive D(h) sampling,
 * especially at grazing angles.
 *
 * @param wo Outgoing direction in local space (z-up, must be in upper hemisphere)
 * @param roughness Perceptual roughness (will be squared to get alpha)
 * @param u1, u2 Uniform random numbers in [0, 1)
 */
[[nodiscard]] inline MicrofacetSample
ggx_vndf_sample(const Vec3<float>& wo, float roughness, float u1, float u2) noexcept
{
    const float alpha = roughness * roughness;

    const Vec3<float> Vh = glm::normalize(Vec3<float>{alpha * wo.x, alpha * wo.y, wo.z});

    const float len2 = Vh.x * Vh.x + Vh.y * Vh.y;
    const Vec3<float> T1 = len2 > 1e-7f ? Vec3<float>{-Vh.y, Vh.x, 0.0f} / std::sqrt(len2)
                                        : Vec3<float>{1.0f, 0.0f, 0.0f};
    const Vec3<float> T2 = glm::cross(Vh, T1);

    const float r = std::sqrt(u1);
    const float phi = 2.0f * std::numbers::pi_v<float> * u2;
    const float t1 = r * std::cos(phi);
    float t2 = r * std::sin(phi);
    const float s = 0.5f * (1.0f + Vh.z);
    t2 = (1.0f - s) * std::sqrt(std::max(0.0f, 1.0f - t1 * t1)) + s * t2;

    const Vec3<float> Nh =
        t1 * T1 + t2 * T2 + std::sqrt(std::max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;

    const Vec3<float> h =
        glm::normalize(Vec3<float>{alpha * Nh.x, alpha * Nh.y, std::max(0.0f, Nh.z)});

    const float cos_theta_h = h.z;
    const float alpha2 = alpha * alpha;
    const float D = ggx_distribution(cos_theta_h * cos_theta_h, h.x * h.x + h.y * h.y, alpha2);

    const float cos_theta_o = wo.z;
    const float cos_theta_o2 = cos_theta_o * cos_theta_o;
    const float tan_theta_o2 = (1.0f - cos_theta_o2) / std::max(cos_theta_o2, 1e-8f);
    const float lambda = (-1.0f + std::sqrt(1.0f + alpha2 * tan_theta_o2)) * 0.5f;
    const float G1 = 1.0f / (1.0f + lambda);

    const float wo_dot_h = std::max(glm::dot(wo, h), 0.0f);
    const float pdf = D * G1 * wo_dot_h / std::max(std::abs(wo.z), 1e-8f);

    return {.half_vector = h, .pdf = std::max(pdf, 1e-8f)};
}

// =========================================================================
//  Frame conversion utilities
// =========================================================================

/**
 * @brief Transforms a direction from local shading space to world space.
 * Local convention: +Z = normal, +X = tangent, +Y = bitangent.
 */
[[nodiscard]] inline Vec3<float> local_to_world(const Vec3<float>& local_dir,
                                                const Vec3<float>& tangent,
                                                const Vec3<float>& bitangent,
                                                const Vec3<float>& normal) noexcept
{
    return tangent * local_dir.x + bitangent * local_dir.y + normal * local_dir.z;
}

/**
 * @brief Transforms a direction from world space to local shading space.
 */
[[nodiscard]] inline Vec3<float> world_to_local(const Vec3<float>& world_dir,
                                                const Vec3<float>& tangent,
                                                const Vec3<float>& bitangent,
                                                const Vec3<float>& normal) noexcept
{
    return {
        glm::dot(world_dir, tangent), glm::dot(world_dir, bitangent), glm::dot(world_dir, normal)};
}

} // namespace huira::sampling
