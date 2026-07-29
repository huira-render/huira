
#include <algorithm>
#include <cmath>
#include <string>

#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Constructs a MeasuredPSF from sampled data and builds its polyphase cache.
 *
 * @param data Measured PSF samples, centered on the image.
 * @param samples_per_pixel Measurement samples per sensor pixel per axis (>= 1 recommended; must
 *                          be positive).
 * @param radius Polyphase stamping kernel radius in sensor pixels. A value of 0 selects the largest
 *               radius covered by the measurement, capped at 64.
 * @param banks Number of polyphase banks per axis for subpixel stamping.
 */
template <IsSpectral TSpectral>
MeasuredPSF<TSpectral>::MeasuredPSF(const Image<TSpectral>& data,
                                    float samples_per_pixel,
                                    int radius,
                                    int banks)
    : data_(data), samples_per_pixel_(samples_per_pixel),
      center_x_(static_cast<float>(data.width() - 1) * 0.5f),
      center_y_(static_cast<float>(data.height() - 1) * 0.5f), measured_radius_(0)
{
    if (data_.width() < 2 || data_.height() < 2) {
        HUIRA_THROW_ERROR("MeasuredPSF - Measured data must be at least 2x2 samples");
    }
    if (!(samples_per_pixel_ > 0.f) || std::isnan(samples_per_pixel_) ||
        std::isinf(samples_per_pixel_)) {
        HUIRA_THROW_ERROR("MeasuredPSF - samples_per_pixel must be a positive finite value: " +
                          std::to_string(samples_per_pixel_));
    }

    // Largest radius (in sensor pixels) fully covered by the measurement:
    const float extent_samples = std::min(center_x_, center_y_);
    measured_radius_ = static_cast<int>(std::floor(extent_samples / samples_per_pixel_));
    if (measured_radius_ < 1) {
        HUIRA_THROW_ERROR("MeasuredPSF - Measured data covers less than one sensor pixel of "
                          "radius at samples_per_pixel = " +
                          std::to_string(samples_per_pixel_));
    }

    if (radius == 0) {
        radius = std::min(measured_radius_, 64);
    } else if (radius > measured_radius_) {
        HUIRA_THROW_ERROR("MeasuredPSF - Requested polyphase radius " + std::to_string(radius) +
                          " exceeds the measured extent of " + std::to_string(measured_radius_) +
                          " sensor pixels");
    }

    this->build_polyphase_cache(radius, banks);
}

/**
 * @brief Evaluates the measured PSF at the given sensor-pixel offset from center.
 *
 * Bilinearly interpolates the measured samples; returns zero outside the measured extent.
 *
 * @param x Horizontal offset from the PSF center in sensor pixel coordinates.
 * @param y Vertical offset from the PSF center in sensor pixel coordinates.
 * @return The interpolated PSF intensity for each spectral channel.
 */
template <IsSpectral TSpectral>
TSpectral MeasuredPSF<TSpectral>::evaluate(float x, float y)
{
    // Sensor-pixel offset -> measurement sample coordinates:
    const float sx = center_x_ + x * samples_per_pixel_;
    const float sy = center_y_ + y * samples_per_pixel_;

    if (sx < 0.f || sy < 0.f || sx >= static_cast<float>(data_.width() - 1) ||
        sy >= static_cast<float>(data_.height() - 1)) {
        return TSpectral{0.f};
    }

    const int x0 = static_cast<int>(sx);
    const int y0 = static_cast<int>(sy);
    const float dx = sx - static_cast<float>(x0);
    const float dy = sy - static_cast<float>(y0);

    const TSpectral& c00 = data_(x0, y0);
    const TSpectral& c10 = data_(x0 + 1, y0);
    const TSpectral& c01 = data_(x0, y0 + 1);
    const TSpectral& c11 = data_(x0 + 1, y0 + 1);

    return (c00 * (1.f - dx) + c10 * dx) * (1.f - dy) + (c01 * (1.f - dx) + c11 * dx) * dy;
}
} // namespace huira
