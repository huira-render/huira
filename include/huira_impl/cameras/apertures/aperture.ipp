#include <algorithm>
#include <cmath>

#include "huira/util/logger.hpp"

namespace huira {
/**
 * @brief Build the polyphase defocus kernel for a given blur radius.
 *
 * A blur radius below half a pixel is treated as in focus and clears the cache.
 *
 * @param blur_radius_pixels Radius of the defocus blur spot on the sensor, in pixels.
 * @param banks Number of polyphase banks per axis used for subpixel stamping.
 */
template <IsSpectral TSpectral>
void Aperture<TSpectral>::build_defocus_kernel(float blur_radius_pixels, int banks)
{
    if (!(blur_radius_pixels >= 0.5f)) {
        defocus_cache_ = PolyphaseCache{};
        return;
    }

    defocus_cache_.radius = blur_radius_pixels;
    defocus_cache_.half_extent = std::max(1, static_cast<int>(std::ceil(blur_radius_pixels)));
    defocus_cache_.banks = banks;
    defocus_cache_.dim = 2 * defocus_cache_.half_extent + 1;

    generate_polyphase_data_();
}

template <IsSpectral TSpectral>
const Image<float>& Aperture<TSpectral>::get_defocus_kernel(float u, float v) const
{
    if (defocus_cache_.kernels.empty()) {
        HUIRA_THROW_ERROR("Aperture::get_defocus_kernel() - Polyphase cache is empty.");
    }

    // Convert 0.0-1.0 fraction to 0-(banks-1) integer index
    // Clamp avoids floating point epsilon errors
    int bx = std::clamp(static_cast<int>(u * static_cast<float>(defocus_cache_.banks)),
                        0,
                        static_cast<int>(defocus_cache_.banks) - 1);
    int by = std::clamp(static_cast<int>(v * static_cast<float>(defocus_cache_.banks)),
                        0,
                        static_cast<int>(defocus_cache_.banks) - 1);

    return defocus_cache_.kernels[static_cast<std::size_t>(by * defocus_cache_.banks + bx)];
}

template <IsSpectral TSpectral>
void Aperture<TSpectral>::generate_polyphase_data_()
{
    const int dim = defocus_cache_.dim;
    const int banks = defocus_cache_.banks;

    defocus_cache_.kernels.assign(static_cast<std::size_t>(banks * banks), Image<float>(dim, dim));

    for (int j = 0; j < banks; ++j) {
        for (int i = 0; i < banks; ++i) {
            float offset_x = static_cast<float>(i) / static_cast<float>(banks);
            float offset_y = static_cast<float>(j) / static_cast<float>(banks);

            auto& kernel = defocus_cache_.kernels[static_cast<std::size_t>(j * banks + i)];
            kernel.fill(0.f);

            rasterize_kernel_(kernel, defocus_cache_.radius, offset_x, offset_y);

            // normalize so kernel sums to 1
            float sum = 0.f;
            for (int y = 0; y < dim; ++y) {
                for (int x = 0; x < dim; ++x) {
                    sum += kernel(x, y);
                }
            }

            float inv = 1.f / sum;
            for (int y = 0; y < dim; ++y) {
                for (int x = 0; x < dim; ++x) {
                    kernel(x, y) *= inv;
                }
            }
        }
    }
}
} // namespace huira
