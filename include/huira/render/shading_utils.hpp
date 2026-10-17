#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "huira/core/transform.hpp"

namespace huira {

/**
 * @brief Power heuristic (beta = 2) for multiple importance sampling.
 *
 * @param f_pdf The PDF of the sampling strategy that generated the sample.
 * @param g_pdf The PDF of the competing sampling strategy.
 * @return The MIS weight for the sample.
 */
inline float power_heuristic(float f_pdf, float g_pdf)
{
    if (std::isinf(f_pdf * f_pdf)) {
        return 1.0f;
    }
    float f2 = f_pdf * f_pdf;
    float g2 = g_pdf * g_pdf;
    if (f2 + g2 <= 0.0f) {
        return 0.0f;
    }
    return f2 / (f2 + g2);
}

/**
 * @brief The transform Embree gives an instance at a normalized time in the exposure.
 *
 * Between two temporal samples, Embree interpolates an instance's translation and scale
 * linearly, and its rotation along the shorter arc between the two (slerp). This does the same,
 * so that what a ray hits is placed, and its normals turned, as Embree placed it when the ray
 * found it.
 *
 * @param transforms The instance's transforms at uniformly spaced temporal samples.
 * @param t Normalized time in [0, 1] across the sequence.
 * @return The instance's transform at that time.
 */
inline Transform<float> instance_transform_at(const std::vector<Transform<float>>& transforms,
                                              float t)
{
    if (transforms.empty()) {
        return Transform<float>{};
    }
    if (transforms.size() == 1) {
        return transforms[0];
    }

    t = std::clamp(t, 0.0f, 1.0f);
    const float scaled_t = t * static_cast<float>(transforms.size() - 1);
    std::size_t idx = static_cast<std::size_t>(std::floor(scaled_t));
    idx = std::min(idx, transforms.size() - 2);
    const float frac = scaled_t - static_cast<float>(idx);
    const Transform<float>& a = transforms[idx];
    const Transform<float>& b = transforms[idx + 1];

    Transform<float> result;
    result.position = a.position * (1.0f - frac) + b.position * frac;
    result.scale = a.scale * (1.0f - frac) + b.scale * frac;
    // glm::slerp takes the shorter arc, as Embree does:
    result.rotation = Rotation<float>::from_local_to_parent(glm::slerp(
        a.rotation.local_to_parent_quaternion(), b.rotation.local_to_parent_quaternion(), frac));
    return result;
}

/**
 * @brief Interpolates a per-temporal-sample transform sequence at a normalized time.
 *
 * @param transforms Transforms at uniformly spaced temporal samples.
 * @param t Normalized time in [0, 1] across the sequence.
 * @return The interpolated transform (lerped position, slerped rotation).
 */
inline Transform<float> interpolate_transform(const std::vector<Transform<float>>& transforms,
                                              float t)
{
    if (transforms.empty()) {
        return Transform<float>{};
    }
    if (transforms.size() == 1) {
        return transforms[0];
    }

    t = std::clamp(t, 0.0f, 1.0f);

    float scaled_t = t * static_cast<float>(transforms.size() - 1);
    std::size_t idx = static_cast<std::size_t>(std::floor(scaled_t));
    idx = std::min(idx, transforms.size() - 2);
    float frac = scaled_t - static_cast<float>(idx);

    Transform<float> result;

    // Linearly interpolate position
    result.position =
        transforms[idx].position * (1.0f - frac) + transforms[idx + 1].position * frac;

    // Slerp rotation (assuming you are using glm::quat for rotations)
    auto quat = transforms[idx].rotation.local_to_parent_quaternion();
    auto next_quat = transforms[idx + 1].rotation.local_to_parent_quaternion();
    result.rotation.from_local_to_parent(glm::slerp(quat, next_quat, frac));

    // Scale interpolation
    // result.scale = transforms[idx].scale * (1.0f - frac) + transforms[idx + 1].scale * frac;

    return result;
}
} // namespace huira
