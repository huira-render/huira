
#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

#include "huira/images/image.hpp"
#include "huira/util/logger.hpp"
#include "tbb/blocked_range2d.h"
#include "tbb/parallel_for.h"
#include "tbb/task_arena.h"

namespace huira {

/**
 * @brief Checks that a polyphase cache of the given size is valid and fits in memory.
 *
 * The cache stores (banks * banks) subpixel-shifted kernels, generated from an intermediate
 * super-resolution LUT of (dim * 64)^2 samples. Both scale steeply with radius: this path is
 * designed for the compact PSF core used when stamping unresolved sources (radius ~4-64).
 * Frame-wide kernels for whole-image convolution should use generate_convolution_kernel()
 * instead, which builds a single centered kernel with no subpixel banks and no
 * super-resolution intermediate.
 *
 * @param radius The kernel radius in pixels.
 * @param banks The number of polyphase banks per axis.
 * @throws std::runtime_error if either is less than 1, or the cache would exceed 4 GiB.
 */
template <IsSpectral TSpectral>
void PSF<TSpectral>::check_polyphase_size(int radius, int banks)
{
    if (radius < 1 || banks < 1) {
        HUIRA_THROW_ERROR("PSF - Polyphase radius and banks must both be at least 1: radius " +
                          std::to_string(radius) + ", banks " + std::to_string(banks));
    }
    const std::size_t dim = 2 * static_cast<std::size_t>(radius) + 1;
    const std::size_t lut_res = std::max<std::size_t>(2048, dim * 64);
    const std::size_t lut_bytes = lut_res * lut_res * sizeof(TSpectral);
    const std::size_t bank_bytes = static_cast<std::size_t>(banks) *
                                   static_cast<std::size_t>(banks) * dim * dim * sizeof(TSpectral);
    constexpr std::size_t MAX_BYTES = std::size_t{4} * 1024 * 1024 * 1024; // 4 GiB
    if (lut_bytes + bank_bytes > MAX_BYTES) {
        HUIRA_THROW_ERROR(
            "PSF - Polyphase radius " + std::to_string(radius) + " with " + std::to_string(banks) +
            "x" + std::to_string(banks) + " banks requires " +
            std::to_string((lut_bytes + bank_bytes) >> 30) +
            " GiB. The polyphase cache is intended for the compact stamping core; for "
            "large frame-wide convolution kernels use "
            "CameraModel::set_psf_convolution_radius() / "
            "PSF::generate_convolution_kernel() instead.");
    }
}

/**
 * @brief Sets the size of the polyphase cache, which is built when first used.
 *
 * Discards any cache already built. Use ensure_polyphase_cache() to build it ahead of time.
 *
 * @param radius The kernel radius in pixels.
 * @param banks The number of polyphase banks per axis.
 */
template <IsSpectral TSpectral>
void PSF<TSpectral>::set_polyphase_size(int radius, int banks)
{
    check_polyphase_size(radius, banks);

    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.radius = radius;
    cache_.banks = banks;
    cache_.dim = 2 * radius + 1;
    cache_.kernels.clear();
    cache_built_.store(false, std::memory_order_release);
}

/**
 * @brief Builds the polyphase kernel cache for the PSF now.
 *
 * Allocates and fills the cache with polyphase kernels for efficient PSF evaluation.
 *
 * @param radius The kernel radius in pixels.
 * @param banks The number of polyphase banks.
 */
template <IsSpectral TSpectral>
void PSF<TSpectral>::build_polyphase_cache(int radius, int banks)
{
    set_polyphase_size(radius, banks);
    ensure_polyphase_cache();
}

/**
 * @brief Builds the polyphase cache if it has not been built for its current size.
 *
 * Thread-safe: concurrent callers wait for a single build. Does nothing if no size has been
 * set.
 */
template <IsSpectral TSpectral>
void PSF<TSpectral>::ensure_polyphase_cache() const
{
    if (cache_built_.load(std::memory_order_acquire)) {
        return;
    }
    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (cache_built_.load(std::memory_order_relaxed) || cache_.banks < 1) {
        return;
    }

    // Building fills the cache, which is logically part of the PSF's (lazily computed) value;
    // PSFs are only ever created as non-const objects, so casting away const here is safe.
    auto& self = const_cast<PSF<TSpectral>&>(*this);
    self.cache_.kernels.resize(static_cast<std::size_t>(cache_.banks * cache_.banks));
    for (auto& img : self.cache_.kernels) {
        img = Image<TSpectral>(cache_.dim, cache_.dim);
    }
    // Isolated, so that while this thread waits for the parallel build it cannot pick up
    // another task that also needs this cache (it would deadlock on the lock it holds).
    tbb::this_task_arena::isolate([&] { self.generate_polyphase_data_(); });
    cache_built_.store(true, std::memory_order_release);
}

/**
 * @brief Retrieves the polyphase kernel for the given normalized coordinates.
 *
 * Returns the cached kernel corresponding to the specified subpixel position, building the
 * cache first if needed.
 *
 * @param u Normalized horizontal coordinate in [0, 1].
 * @param v Normalized vertical coordinate in [0, 1].
 * @return Reference to the corresponding kernel image.
 */
template <IsSpectral TSpectral>
const Image<TSpectral>& PSF<TSpectral>::get_kernel(float u, float v) const
{
    ensure_polyphase_cache();
    if (cache_.kernels.empty()) {
        HUIRA_THROW_ERROR("PSF::get_kernel() - No polyphase cache size has been set.");
    }

    // Convert 0.0-1.0 fraction to 0-(banks-1) integer index
    // Clamp avoids floating point epsilon errors
    int bx = std::clamp(static_cast<int>(u * static_cast<float>(cache_.banks)),
                        0,
                        static_cast<int>(cache_.banks) - 1);
    int by = std::clamp(static_cast<int>(v * static_cast<float>(cache_.banks)),
                        0,
                        static_cast<int>(cache_.banks) - 1);

    return cache_.kernels[static_cast<std::size_t>(by * cache_.banks + bx)];
}

/**
 * @brief Generates a single centered kernel suitable for whole-image convolution.
 *
 * Unlike the polyphase cache, this produces exactly one kernel with zero subpixel offset, by
 * directly integrating evaluate() over each pixel with stratified sampling (or, for a PSF that
 * is already integrated over a pixel, see is_pixel_integrated(), taking it at each pixel's
 * center). There is no super-resolution intermediate LUT and no bank multiplicity, so memory is
 * dim^2 pixels regardless of radius - large frame-wide kernels (radius of hundreds to thousands
 * of pixels) are practical. The result is normalized to unit energy per channel.
 *
 * evaluate() must be thread-safe: pixels are integrated in parallel.
 *
 * @param radius The kernel radius in pixels (kernel dimension is 2 * radius + 1).
 * @return The normalized convolution kernel.
 */
template <IsSpectral TSpectral>
Image<TSpectral> PSF<TSpectral>::generate_convolution_kernel(int radius)
{
    if (radius < 1) {
        HUIRA_THROW_ERROR("PSF::generate_convolution_kernel - Radius must be >= 1: " +
                          std::to_string(radius));
    }

    const int dim = 2 * radius + 1;
    Image<TSpectral> kernel(dim, dim);
    const bool pixel_integrated = is_pixel_integrated();

    // Same per-pixel integration quality as the polyphase path (16x16 stratified samples),
    // but sampling evaluate() directly instead of an interpolated super-resolution LUT:
    constexpr int INTEGRATION_STEPS = 16;
    constexpr float INV_SAMPLES_SQ =
        1.0f / static_cast<float>(INTEGRATION_STEPS * INTEGRATION_STEPS);
    constexpr float SAMPLE_STEP = 1.0f / static_cast<float>(INTEGRATION_STEPS);

    tbb::parallel_for(
        tbb::blocked_range2d<int>(0, dim, 0, dim), [&](const tbb::blocked_range2d<int>& r) {
            for (int y = r.rows().begin(); y < r.rows().end(); ++y) {
                const float pixel_center_y = static_cast<float>(y - radius);
                for (int x = r.cols().begin(); x < r.cols().end(); ++x) {
                    const float pixel_center_x = static_cast<float>(x - radius);
                    if (pixel_integrated) {
                        kernel(x, y) = evaluate(pixel_center_x, pixel_center_y);
                        continue;
                    }

                    TSpectral integrated_val{};
                    for (int sy = 0; sy < INTEGRATION_STEPS; ++sy) {
                        const float sub_y = ((static_cast<float>(sy) + 0.5f) * SAMPLE_STEP) - 0.5f;
                        for (int sx = 0; sx < INTEGRATION_STEPS; ++sx) {
                            const float sub_x =
                                ((static_cast<float>(sx) + 0.5f) * SAMPLE_STEP) - 0.5f;
                            integrated_val +=
                                evaluate(pixel_center_x + sub_x, pixel_center_y + sub_y);
                        }
                    }
                    kernel(x, y) = integrated_val * INV_SAMPLES_SQ;
                }
            }
        });

    // Normalize to unit energy per channel (double accumulation for large kernels):
    std::array<double, TSpectral::size()> totals{};
    for (int y = 0; y < dim; ++y) {
        for (int x = 0; x < dim; ++x) {
            for (std::size_t i = 0; i < TSpectral::size(); ++i) {
                totals[i] += static_cast<double>(kernel(x, y)[i]);
            }
        }
    }
    TSpectral scale;
    for (std::size_t i = 0; i < TSpectral::size(); ++i) {
        scale[i] = (totals[i] > 1e-12) ? static_cast<float>(1.0 / totals[i]) : 0.0f;
    }
    for (int y = 0; y < dim; ++y) {
        for (int x = 0; x < dim; ++x) {
            kernel(x, y) *= scale;
        }
    }

    return kernel;
}

/**
 * @brief Generates the polyphase kernel data for the PSF.
 *
 * Fills the polyphase cache by evaluating the PSF at subpixel positions and integrating over each
 * pixel, or, for a PSF that is already integrated over a pixel (see is_pixel_integrated()), by
 * taking it at each pixel's center.
 */
template <IsSpectral TSpectral>
void PSF<TSpectral>::generate_polyphase_data_()
{
    if (is_pixel_integrated()) {
        // Bank (bx, by) is the stamp of a source bx / banks, by / banks of a pixel from the
        // center of the stamp's middle pixel, as below.
        tbb::parallel_for(tbb::blocked_range2d<int>(0, cache_.banks, 0, cache_.banks),
                          [&](const tbb::blocked_range2d<int>& r) {
                              for (int by = r.rows().begin(); by < r.rows().end(); ++by) {
                                  for (int bx = r.cols().begin(); bx < r.cols().end(); ++bx) {
                                      fill_pixel_integrated_bank_(bx, by);
                                  }
                              }
                          });
        return;
    }

    // Dynamic Resolution Calculation
    constexpr int QUALITY_SAMPLES_1D = 64;
    int calculated_res = static_cast<int>(cache_.dim) * QUALITY_SAMPLES_1D;
    int lut_res = std::max(2048, calculated_res);

    std::vector<TSpectral> lut_data(static_cast<std::size_t>(lut_res) *
                                    static_cast<std::size_t>(lut_res));

    float max_radius = static_cast<float>(cache_.radius) + 1.0f;
    float lut_scale_inv = 1.0f / static_cast<float>(lut_res - 1);

    tbb::parallel_for(
        tbb::blocked_range2d<int>(0, lut_res, 0, lut_res), [&](const tbb::blocked_range2d<int>& r) {
            for (int y = r.rows().begin(); y < r.rows().end(); ++y) {
                float v_norm = (static_cast<float>(y) * lut_scale_inv) * 2.0f - 1.0f;
                float v_phys = v_norm * max_radius;

                for (int x = r.cols().begin(); x < r.cols().end(); ++x) {
                    float u_norm = (static_cast<float>(x) * lut_scale_inv) * 2.0f - 1.0f;
                    float u_phys = u_norm * max_radius;

                    std::size_t idx =
                        static_cast<std::size_t>(y) * static_cast<std::size_t>(lut_res) +
                        static_cast<std::size_t>(x);
                    lut_data[idx] = evaluate(u_phys, v_phys);
                }
            }
        });

    // Helper: Bilinear Interpolation from the LUT
    auto sample_lut = [&](float u, float v) -> TSpectral {
        // Map physical coords -> LUT coords
        // (u / max_radius + 1.0f) * 0.5f maps [-R, R] to [0, 1]
        float u_norm = (u / max_radius + 1.0f) * 0.5f;
        float v_norm = (v / max_radius + 1.0f) * 0.5f;

        float x_f = u_norm * static_cast<float>(lut_res - 1);
        float y_f = v_norm * static_cast<float>(lut_res - 1);

        // Bounds check
        if (x_f < 0.0f || x_f >= static_cast<float>(lut_res - 1) || y_f < 0.0f ||
            y_f >= static_cast<float>(lut_res - 1)) {
            return TSpectral{};
        }

        // Explicit cast to int for indices
        int x0 = static_cast<int>(x_f);
        int y0 = static_cast<int>(y_f);

        // Optimization: If exact match
        if (x_f == static_cast<float>(x0) && y_f == static_cast<float>(y0)) {
            std::size_t idx = static_cast<std::size_t>(y0) * static_cast<std::size_t>(lut_res) +
                              static_cast<std::size_t>(x0);
            return lut_data[idx];
        }

        float dx = x_f - static_cast<float>(x0);
        float dy = y_f - static_cast<float>(y0);

        // Pre-calculate indices to clean up the math
        std::size_t row0_idx = static_cast<std::size_t>(y0) * static_cast<std::size_t>(lut_res);
        std::size_t row1_idx = static_cast<std::size_t>(y0 + 1) * static_cast<std::size_t>(lut_res);
        std::size_t col0 = static_cast<std::size_t>(x0);
        std::size_t col1 = static_cast<std::size_t>(x0 + 1);

        const TSpectral& c00 = lut_data[row0_idx + col0];
        const TSpectral& c10 = lut_data[row0_idx + col1];
        const TSpectral& c01 = lut_data[row1_idx + col0];
        const TSpectral& c11 = lut_data[row1_idx + col1];

        return (c00 * (1.0f - dx) + c10 * dx) * (1.0f - dy) + (c01 * (1.0f - dx) + c11 * dx) * dy;
    };

    // Generate polyphase kernel cache from LUT:
    constexpr int INTEGRATION_STEPS = 16;
    constexpr float INV_SAMPLES_SQ =
        1.0f / static_cast<float>(INTEGRATION_STEPS * INTEGRATION_STEPS);
    constexpr float SAMPLE_STEP = 1.0f / static_cast<float>(INTEGRATION_STEPS);

    tbb::parallel_for(
        tbb::blocked_range2d<int>(0, cache_.banks, 0, cache_.banks),
        [&](const tbb::blocked_range2d<int>& r) {
            for (int by = r.rows().begin(); by < r.rows().end(); ++by) {
                for (int bx = r.cols().begin(); bx < r.cols().end(); ++bx) {

                    std::size_t kernel_idx = static_cast<std::size_t>(by * cache_.banks + bx);
                    Image<TSpectral>& kernel = cache_.kernels[kernel_idx];

                    float bank_offset_x = static_cast<float>(bx) / static_cast<float>(cache_.banks);
                    float bank_offset_y = static_cast<float>(by) / static_cast<float>(cache_.banks);

                    TSpectral total_energy{};

                    for (int y = 0; y < cache_.dim; ++y) {
                        for (int x = 0; x < cache_.dim; ++x) {
                            float pixel_center_x = static_cast<float>(x - cache_.radius);
                            float pixel_center_y = static_cast<float>(y - cache_.radius);

                            TSpectral integrated_val{};

                            for (int sy = 0; sy < INTEGRATION_STEPS; ++sy) {
                                for (int sx = 0; sx < INTEGRATION_STEPS; ++sx) {
                                    float sub_x =
                                        ((static_cast<float>(sx) + 0.5f) * SAMPLE_STEP) - 0.5f;
                                    float sub_y =
                                        ((static_cast<float>(sy) + 0.5f) * SAMPLE_STEP) - 0.5f;

                                    float sample_x = pixel_center_x - bank_offset_x + sub_x;
                                    float sample_y = pixel_center_y - bank_offset_y + sub_y;

                                    integrated_val += sample_lut(sample_x, sample_y);
                                }
                            }

                            integrated_val *= INV_SAMPLES_SQ;
                            kernel(x, y) = integrated_val;
                            total_energy += integrated_val;
                        }
                    }
                    normalize_kernel_(kernel, total_energy);
                }
            }
        });
}

/**
 * @brief Fills one polyphase bank from a PSF that is already integrated over a pixel: each
 * pixel's value is the PSF at its center, relative to the source.
 */
template <IsSpectral TSpectral>
void PSF<TSpectral>::fill_pixel_integrated_bank_(int bx, int by)
{
    Image<TSpectral>& kernel = cache_.kernels[static_cast<std::size_t>(by * cache_.banks + bx)];
    const float bank_offset_x = static_cast<float>(bx) / static_cast<float>(cache_.banks);
    const float bank_offset_y = static_cast<float>(by) / static_cast<float>(cache_.banks);

    TSpectral total_energy{};
    for (int y = 0; y < cache_.dim; ++y) {
        for (int x = 0; x < cache_.dim; ++x) {
            const TSpectral value = evaluate(static_cast<float>(x - cache_.radius) - bank_offset_x,
                                             static_cast<float>(y - cache_.radius) - bank_offset_y);
            kernel(x, y) = value;
            total_energy += value;
        }
    }
    normalize_kernel_(kernel, total_energy);
}

/**
 * @brief Normalizes a kernel image so its total energy is unity.
 *
 * Scales the kernel so that the sum of all elements matches unity for each spectral channel.
 *
 * @param kernel The kernel image to normalize.
 * @param total_energy The total energy to normalize by.
 */
template <IsSpectral TSpectral>
void PSF<TSpectral>::normalize_kernel_(Image<TSpectral>& kernel, const TSpectral& total_energy)
{
    // Pre-compute inverse to avoid division in the loop
    TSpectral scale;
    for (std::size_t i = 0; i < TSpectral::size(); ++i) {
        float e = total_energy[i];
        scale[i] = (e > 1e-9f) ? (1.0f / e) : 0.0f;
    }

    // Apply scale
    for (int y = 0; y < cache_.dim; ++y) {
        for (int x = 0; x < cache_.dim; ++x) {
            kernel(x, y) *= scale;
        }
    }
}
} // namespace huira
