#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "embree4/rtcore.h"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"

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
 * Between two temporal samples, Embree interpolates an instance's translation, scale and shear
 * linearly, and its rotation along the shorter arc between the two (slerp). This does the same,
 * so that a light is sampled where rays find it. The rotation is not quite Embree's, which
 * approximates the slerp's trigonometry and falls back to normalized linear interpolation over
 * small turns: what a ray hits is placed with the transform Embree drew it with instead (see
 * SceneView::resolve_hit()).
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
    result.shear = a.shear * (1.0f - frac) + b.shear * frac;
    // glm::slerp takes the shorter arc, as Embree does:
    result.rotation = Rotation<float>::from_local_to_parent(glm::slerp(
        a.rotation.local_to_parent_quaternion(), b.rotation.local_to_parent_quaternion(), frac));
    return result;
}

/// An affine transform as Embree holds one: a linear part (rotation and scale) and a translation.
struct EmbreeTransform {
    Mat3<float> linear{};
    Vec3<float> translation{};
};

/**
 * @brief The transform Embree draws an instance with at a normalized time in the exposure.
 *
 * Between two temporal samples, this is Embree's own interpolation of the instance's poses,
 * which instance_transform_at() only approximates.
 *
 * @param instance The instance's geometry, attached to a committed scene.
 * @param time Normalized time in [0, 1] across the exposure.
 * @return The transform, local to world.
 */
inline EmbreeTransform embree_transform_at(RTCGeometry instance, float time)
{
    std::array<float, 12> matrix{};
    rtcGetGeometryTransform(instance, time, RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR, matrix.data());
    // Column by column, with the translation last, as Transform::to_embree_matrix() gives one:
    EmbreeTransform transform;
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            transform.linear[column][row] = matrix[static_cast<std::size_t>(3 * column + row)];
        }
    }
    transform.translation = {matrix[9], matrix[10], matrix[11]};
    return transform;
}
} // namespace huira
