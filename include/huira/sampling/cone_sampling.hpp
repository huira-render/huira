#pragma once

#include <cmath>
#include <optional>

#include "glm/glm.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/types.hpp"
#include "huira/sampling/sampler.hpp"

namespace huira {
/**
 * @brief A direction sampled toward a spherical region, with its solid-angle PDF.
 */
struct SphereConeSample {
    Vec3<float> wi; ///< Sampled direction (normalized, world space)
    float pdf;      ///< Solid-angle probability density of the sample
    float distance; ///< Distance to the sphere surface along wi (infinity for
                    ///< uniform-sphere fallback samples)
};

/**
 * @brief Uniformly samples the cone of directions subtended by a sphere.
 *
 * Samples a direction from point p toward the sphere (center, radius) with a
 * uniform density over the subtended cone's solid angle. This is the sampling
 * strategy shared by SphereLight and indirect-source (reflector) sampling.
 *
 * @param p Shading/reference point (world space).
 * @param center Sphere center (world space).
 * @param radius Sphere radius.
 * @param sampler Random sampler (consumes two 1D samples).
 * @return The sampled direction, PDF, and distance, or std::nullopt if p lies
 *         inside the sphere.
 */
inline std::optional<SphereConeSample> sample_sphere_cone(const Vec3<float>& p,
                                                          const Vec3<float>& center,
                                                          float radius,
                                                          Sampler<float>& sampler)
{
    Vec3<float> wc = center - p;
    float d2 = glm::dot(wc, wc);
    float d = std::sqrt(d2);

    // If the point is inside the sphere, the cone is undefined
    if (d <= radius) {
        return std::nullopt;
    }

    // Calculate the cone subtended by the sphere. Work in one_minus_cos space:
    // computing (1 - cos_theta_max) directly avoids the catastrophic float
    // cancellation of "1.0f - sqrt(1 - sin^2)" for small cones (the spacing of
    // float near 1.0 is ~6e-8, which would quantize the sampled limb angle by
    // up to several 1e-4 rad and the PDF by up to ~10% for sun-sized cones).
    float sin_theta_max2 = (radius * radius) / d2;
    float cos_theta_max = std::sqrt(std::max(0.0f, 1.0f - sin_theta_max2));
    // Stable: 1 - cos = sin^2 / (1 + cos), no cancellation:
    float one_minus_cos_theta_max = sin_theta_max2 / (1.0f + cos_theta_max);

    // Uniformly sample the cone
    float u1 = sampler.get_1d();
    float u2 = sampler.get_1d();

    float one_minus_cos_theta = u1 * one_minus_cos_theta_max;
    float cos_theta = 1.0f - one_minus_cos_theta;
    // Stable: sin^2 = (1 - cos)(1 + cos) = omc * (2 - omc), full float precision
    // even when cos_theta rounds to 1:
    float sin_theta = std::sqrt(std::max(0.0f, one_minus_cos_theta * (2.0f - one_minus_cos_theta)));
    float phi = u2 * 2.0f * PI<float>();

    // Build a local coordinate system around the vector to the center (wc)
    Vec3<float> w = wc / d;
    Vec3<float> u, v;
    if (std::abs(w.z) < 0.999f) {
        u = glm::normalize(Vec3<float>(-w.y, w.x, 0.0f));
    } else {
        u = glm::normalize(Vec3<float>(0.0f, -w.z, w.y));
    }
    v = glm::cross(w, u);

    // Convert sampled local direction to world space
    Vec3<float> wi =
        u * (sin_theta * std::cos(phi)) + v * (sin_theta * std::sin(phi)) + w * cos_theta;

    // Calculate Solid Angle and PDF. one_minus_cos_theta_max is exact to float
    // precision, so no small-cone fallback is needed (2*pi*omc -> pi*sin^2 in
    // the limit automatically):
    float solid_angle = 2.0f * PI<float>() * one_minus_cos_theta_max;

    // Calculate distance to the sphere surface along wi using standard ray-sphere intersection:
    float b = glm::dot(wc, wi);
    float discriminant = b * b - d2 + (radius * radius);
    float distance = b - std::sqrt(std::max(0.0f, discriminant));

    SphereConeSample sample;
    sample.wi = wi;
    sample.pdf = 1.0f / solid_angle;
    sample.distance = distance;

    return sample;
}

/**
 * @brief Evaluates the PDF of sample_sphere_cone() for a given direction.
 *
 * @param p Shading/reference point (world space).
 * @param center Sphere center (world space).
 * @param radius Sphere radius.
 * @param wi Direction to evaluate (normalized).
 * @return The solid-angle PDF, or 0 if p is inside the sphere or wi misses the cone.
 */
inline float pdf_sphere_cone(const Vec3<float>& p,
                             const Vec3<float>& center,
                             float radius,
                             const Vec3<float>& wi)
{
    Vec3<float> wc = center - p;
    float d2 = glm::dot(wc, wc);

    // If inside the sphere, probability is 0
    if (d2 <= radius * radius) {
        return 0.0f;
    }

    float sin_theta_max2 = (radius * radius) / d2;
    float cos_theta_max = std::sqrt(std::max(0.0f, 1.0f - sin_theta_max2));
    float one_minus_cos_theta_max = sin_theta_max2 / (1.0f + cos_theta_max);

    // Check if the given direction `wi` actually lies within the cone. For
    // narrow cones (cos_theta_max > 0) test in sin^2 space via the cross
    // product, which retains full float precision where the cos-space test is
    // quantized to ~6e-8 near 1:
    Vec3<float> w = glm::normalize(wc);
    if (cos_theta_max > 0.0f) {
        Vec3<float> c = glm::cross(wi, w);
        if (glm::dot(wi, w) <= 0.0f || glm::dot(c, c) > sin_theta_max2) {
            return 0.0f; // Ray missed the sphere
        }
    } else {
        if (glm::dot(wi, w) < cos_theta_max) {
            return 0.0f; // Ray missed the sphere
        }
    }

    float solid_angle = 2.0f * PI<float>() * one_minus_cos_theta_max;

    return 1.0f / solid_angle;
}

/**
 * @brief Uniformly samples a direction on the full unit sphere.
 *
 * @param sampler Random sampler (consumes two 1D samples).
 * @return A uniformly distributed unit direction.
 */
inline Vec3<float> sample_uniform_sphere(Sampler<float>& sampler)
{
    float u1 = sampler.get_1d();
    float u2 = sampler.get_1d();

    float z = 1.0f - 2.0f * u1;
    float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    float phi = 2.0f * PI<float>() * u2;

    return Vec3<float>{r * std::cos(phi), r * std::sin(phi), z};
}

/**
 * @brief The solid-angle PDF of sample_uniform_sphere().
 */
inline float uniform_sphere_pdf()
{
    return 1.0f / (4.0f * PI<float>());
}
} // namespace huira
