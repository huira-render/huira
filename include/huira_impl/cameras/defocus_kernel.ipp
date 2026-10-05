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
 * A stamp only needs to reach as far as a source can put light on the image. Sources are
 * stamped where they project onto the image, so a stamp reaching max_half_extent pixels each
 * way (the image's larger dimension, plus one) covers everything; a larger blur is cut there,
 * and normalized as the whole blur, so that the light falling beyond is lost as it should be.
 *
 * Stamps are made for max_banks x max_banks subpixel positions, unless that would take more
 * than budget_bytes: a large blur then gets fewer, down to one (whole-pixel placement), which is
 * logged. For a blur that large, placement to a fraction of a pixel makes no visible
 * difference.
 *
 * @param aperture Aperture whose shape the blur takes.
 * @param radius_pixels Blur radius in pixels; must be positive.
 * @param max_banks Subpixel banks per axis wanted; must be positive.
 * @param max_half_extent Furthest a stamp needs to reach from its center, in pixels.
 * @param budget_bytes Memory the stamps may take before fewer banks are used.
 * @throws std::runtime_error if a single stamp would take more than 4 GiB, which needs an image
 *         over 16000 pixels wide as well as a blur that large.
 */
template <IsSpectral TSpectral>
void DefocusKernel<TSpectral>::build(const Aperture<TSpectral>& aperture,
                                     float radius_pixels,
                                     int max_banks,
                                     int max_half_extent,
                                     std::size_t budget_bytes)
{
    if (!(radius_pixels > 0.f) || !std::isfinite(radius_pixels) || max_banks < 1 ||
        max_half_extent < 1) {
        HUIRA_THROW_ERROR("DefocusKernel::build - Radius must be positive and finite, and banks "
                          "and extent at least 1: radius " +
                          std::to_string(radius_pixels) + ", banks " + std::to_string(max_banks) +
                          ", extent " + std::to_string(max_half_extent));
    }

    // The shape, with its one-pixel antialiasing ramp, reaches radius + 0.5 from its center, and
    // the center sits up to (banks - 1) / banks of a pixel off the kernel's center pixel. A
    // half extent of ceil(radius + 0.5) holds all of it, unless it is cut short:
    const double full_extent = std::ceil(static_cast<double>(radius_pixels) + 0.5);
    const bool cut = full_extent > static_cast<double>(max_half_extent);
    const int half_extent = cut ? max_half_extent : std::max(1, static_cast<int>(full_extent));
    const std::size_t dim = 2 * static_cast<std::size_t>(half_extent) + 1;
    const std::size_t stamp_bytes = dim * dim * sizeof(float);

    constexpr std::size_t MAX_STAMP_BYTES = std::size_t{4} * 1024 * 1024 * 1024;
    if (stamp_bytes > MAX_STAMP_BYTES) {
        HUIRA_THROW_ERROR("DefocusKernel::build - A blur radius of " +
                          std::to_string(radius_pixels) +
                          " pixels needs stamps of over 4 GiB each on an image this large. "
                          "Check the focus setting: a blur this large is far out of focus.");
    }

    int banks = max_banks;
    while (banks > 1 &&
           static_cast<std::size_t>(banks) * static_cast<std::size_t>(banks) * stamp_bytes >
               budget_bytes) {
        --banks;
    }
    if (banks < max_banks) {
        HUIRA_LOG_INFO("DefocusKernel - A blur radius of " + std::to_string(radius_pixels) +
                       " pixels places unresolved sources to 1/" + std::to_string(banks) +
                       " pixel rather than 1/" + std::to_string(max_banks) +
                       ", to keep its stamps within " + std::to_string(budget_bytes >> 20) +
                       " MiB");
    }

    // The area of the whole blur, in pixels, for a stamp that is cut short. (Rasterizing
    // reproduces it to within a part in 10 r^2.)
    const double bounding_radius = aperture.get_bounding_radius().to_si();
    const double whole_area = aperture.get_area().to_si() *
                              std::pow(static_cast<double>(radius_pixels) / bounding_radius, 2.0);

    // Old stamps go first, and new ones are made in place, so that at most one set exists:
    kernels_.clear();
    radius_ = radius_pixels;
    half_extent_ = half_extent;
    banks_ = banks;
    const int count = banks * banks;
    kernels_.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        kernels_.emplace_back(static_cast<int>(dim), static_cast<int>(dim), 0.f);
    }

    tbb::parallel_for(tbb::blocked_range<int>(0, count), [&](const tbb::blocked_range<int>& r) {
        for (int index = r.begin(); index < r.end(); ++index) {
            const float offset_x = static_cast<float>(index % banks) / static_cast<float>(banks);
            const float offset_y = static_cast<float>(index / banks) / static_cast<float>(banks);

            Image<float>& kernel = kernels_[static_cast<std::size_t>(index)];
            aperture.rasterize_shape(kernel, radius_pixels, offset_x, offset_y);

            double sum = 0.0;
            if (cut) {
                sum = whole_area;
            } else {
                for (int y = 0; y < kernel.height(); ++y) {
                    for (int x = 0; x < kernel.width(); ++x) {
                        sum += static_cast<double>(kernel(x, y));
                    }
                }
            }
            const float inv = static_cast<float>(1.0 / sum);
            for (int y = 0; y < kernel.height(); ++y) {
                for (int x = 0; x < kernel.width(); ++x) {
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
