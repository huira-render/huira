#pragma once

// How CookTorranceBSDF reads its albedo table, cook_torrance_albedo.ipp, which
// tools/build_cook_torrance_albedo builds for exactly this interpolation: change it, and the table
// must be rebuilt (the table records a fingerprint of this file; see that tool).

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "huira_impl/materials/bsdfs/cook_torrance_albedo.ipp"

namespace huira::detail {

/**
 * @brief The directional albedo E of CookTorranceBSDF's dielectric specular lobe at one
 * roughness, and its cosine-weighted average E_avg, from the table in cook_torrance_albedo.ipp:
 * bilinear in sqrt(cos) and sqrt(roughness), and never below the lobe's.
 */
class DielectricSpecularAlbedo {
  public:
    explicit DielectricSpecularAlbedo(float roughness) noexcept
    {
        const float lowest = std::sqrt(DIELECTRIC_ALBEDO_MIN_ROUGHNESS);
        const float y =
            (std::sqrt(std::clamp(roughness, DIELECTRIC_ALBEDO_MIN_ROUGHNESS, 1.0f)) - lowest) /
            (1.0f - lowest) * static_cast<float>(DIELECTRIC_ALBEDO_ROUGHNESS_NODES - 1);
        const std::size_t j =
            std::min(static_cast<std::size_t>(y), DIELECTRIC_ALBEDO_ROUGHNESS_NODES - 2);
        t_ = y - static_cast<float>(j);
        row_ = &DIELECTRIC_SPECULAR_ALBEDO[j * DIELECTRIC_ALBEDO_COS_NODES];
        average_ = DIELECTRIC_SPECULAR_AVERAGE_ALBEDO[j] +
                   t_ * (DIELECTRIC_SPECULAR_AVERAGE_ALBEDO[j + 1] -
                         DIELECTRIC_SPECULAR_AVERAGE_ALBEDO[j]);
    }

    /// E at the given cosine to the normal.
    [[nodiscard]] float operator()(float cos_theta) const noexcept
    {
        const float x = std::sqrt(std::clamp(cos_theta, 0.0f, 1.0f)) *
                        static_cast<float>(DIELECTRIC_ALBEDO_COS_NODES - 1);
        const std::size_t i =
            std::min(static_cast<std::size_t>(x), DIELECTRIC_ALBEDO_COS_NODES - 2);
        const float a = x - static_cast<float>(i);
        const float* next_row = row_ + DIELECTRIC_ALBEDO_COS_NODES;
        const float e0 = row_[i] + a * (row_[i + 1] - row_[i]);
        const float e1 = next_row[i] + a * (next_row[i + 1] - next_row[i]);
        return e0 + t_ * (e1 - e0);
    }

    /// E_avg, twice the integral of E(mu) mu over mu from 0 to 1.
    [[nodiscard]] float average() const noexcept { return average_; }

  private:
    const float* row_;
    float t_;
    float average_;
};

} // namespace huira::detail
