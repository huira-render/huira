#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include "huira/util/logger.hpp"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

namespace huira {

/**
 * @brief Builds the stamps for a blur of the given radius.
 *
 * @param aperture Aperture whose shape the blur takes.
 * @param radius_pixels Blur radius in pixels; must be positive.
 * @param banks Subpixel banks per axis; must be positive.
 */
template <IsSpectral TSpectral>
void DefocusKernel<TSpectral>::build(const Aperture<TSpectral>& aperture,
                                     float radius_pixels,
                                     int banks)
{
    if (!(radius_pixels > 0.f) || !std::isfinite(radius_pixels) || banks < 1) {
        HUIRA_THROW_ERROR("DefocusKernel::build - Radius must be positive and finite, and banks "
                          "at least 1: radius " +
                          std::to_string(radius_pixels) + ", banks " + std::to_string(banks));
    }

    // The shape, with its one-pixel antialiasing ramp, reaches radius + 0.5 from its center, and
    // the center sits up to (banks - 1) / banks of a pixel off the kernel's center pixel. A
    // half extent of ceil(radius + 0.5) holds all of it.
    const int half_extent = std::max(1, static_cast<int>(std::ceil(radius_pixels + 0.5f)));
    const int dim = 2 * half_extent + 1;

    const std::size_t bytes = static_cast<std::size_t>(banks) * static_cast<std::size_t>(banks) *
                              static_cast<std::size_t>(dim) * static_cast<std::size_t>(dim) *
                              sizeof(float);
    constexpr std::size_t MAX_BYTES = std::size_t{4} * 1024 * 1024 * 1024; // 4 GiB
    if (bytes > MAX_BYTES) {
        HUIRA_THROW_ERROR("DefocusKernel::build - A blur radius of " +
                          std::to_string(radius_pixels) + " pixels with " + std::to_string(banks) +
                          "x" + std::to_string(banks) + " banks needs " +
                          std::to_string(bytes >> 30) +
                          " GiB of stamps. Check the focus setting: a blur this large is far out "
                          "of focus.");
    }

    radius_ = radius_pixels;
    half_extent_ = half_extent;
    banks_ = banks;
    kernels_.assign(static_cast<std::size_t>(banks) * static_cast<std::size_t>(banks),
                    Image<float>(dim, dim, 0.f));

    const int count = banks * banks;
    tbb::parallel_for(tbb::blocked_range<int>(0, count), [&](const tbb::blocked_range<int>& r) {
        for (int index = r.begin(); index < r.end(); ++index) {
            const float offset_x = static_cast<float>(index % banks) / static_cast<float>(banks);
            const float offset_y = static_cast<float>(index / banks) / static_cast<float>(banks);

            Image<float>& kernel = kernels_[static_cast<std::size_t>(index)];
            aperture.rasterize_shape(kernel, radius_pixels, offset_x, offset_y);

            double sum = 0.0;
            for (int y = 0; y < dim; ++y) {
                for (int x = 0; x < dim; ++x) {
                    sum += static_cast<double>(kernel(x, y));
                }
            }
            const float inv = static_cast<float>(1.0 / sum);
            for (int y = 0; y < dim; ++y) {
                for (int x = 0; x < dim; ++x) {
                    kernel(x, y) *= inv;
                }
            }
        }
    });
}

/// Removes the stamps: no blur.
template <IsSpectral TSpectral>
void DefocusKernel<TSpectral>::clear()
{
    radius_ = 0.f;
    half_extent_ = 0;
    banks_ = 0;
    kernels_.clear();
}

/**
 * @brief The stamp for a source at the given subpixel offset.
 *
 * @param u Horizontal offset in [0, 1), in fractions of a pixel.
 * @param v Vertical offset in [0, 1), in fractions of a pixel.
 */
template <IsSpectral TSpectral>
const Image<float>& DefocusKernel<TSpectral>::get(float u, float v) const
{
    if (kernels_.empty()) {
        HUIRA_THROW_ERROR("DefocusKernel::get - No stamps have been built");
    }

    // Convert the 0-1 fraction to a 0-(banks-1) index; the clamp absorbs rounding at 1.
    const int bx = std::clamp(static_cast<int>(u * static_cast<float>(banks_)), 0, banks_ - 1);
    const int by = std::clamp(static_cast<int>(v * static_cast<float>(banks_)), 0, banks_ - 1);
    return kernels_[static_cast<std::size_t>(by * banks_ + bx)];
}

} // namespace huira
