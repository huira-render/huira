
#include <cmath>
#include <string>

#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Constructs a HarveyShackScatter profile with the given parameters.
 *
 * @param falloff_exponent Power-law exponent b (typically 2 to 3).
 * @param r0 Shoulder radius in pixels.
 * @param cutoff_radius Hard cutoff radius in pixels (0 means no cutoff).
 */
template <IsSpectral TSpectral>
HarveyShackScatter<TSpectral>::HarveyShackScatter(float falloff_exponent,
                                                  float r0,
                                                  float cutoff_radius)
    : falloff_exponent_(falloff_exponent), r0_inv_sq_(0.f),
      cutoff_radius_sq_(cutoff_radius * cutoff_radius)
{
    if (!(falloff_exponent > 0.f) || std::isnan(falloff_exponent)) {
        HUIRA_THROW_ERROR(
            "HarveyShackScatter - Falloff exponent must be a positive finite value: " +
            std::to_string(falloff_exponent));
    }
    if (!(r0 > 0.f) || std::isnan(r0)) {
        HUIRA_THROW_ERROR("HarveyShackScatter - r0 must be a positive finite value: " +
                          std::to_string(r0));
    }
    if (cutoff_radius < 0.f || std::isnan(cutoff_radius)) {
        HUIRA_THROW_ERROR("HarveyShackScatter - Cutoff radius must be non-negative: " +
                          std::to_string(cutoff_radius));
    }
    r0_inv_sq_ = 1.f / (r0 * r0);
}

/**
 * @brief Evaluates the scatter profile at the given pixel offset from center.
 *
 * @param x Horizontal offset from the center in pixel coordinates.
 * @param y Vertical offset from the center in pixel coordinates.
 * @return The (achromatic) profile value for each spectral channel.
 */
template <IsSpectral TSpectral>
TSpectral HarveyShackScatter<TSpectral>::evaluate(float x, float y)
{
    const float r_sq = x * x + y * y;

    if (cutoff_radius_sq_ > 0.f && r_sq > cutoff_radius_sq_) {
        return TSpectral{0.f};
    }

    // (1 + (r/r0)^2)^(-b/2): flat near the origin, r^-b for r >> r0.
    const float value = std::pow(1.f + r_sq * r0_inv_sq_, -0.5f * falloff_exponent_);
    return TSpectral{value};
}
} // namespace huira
