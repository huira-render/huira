#include <cstddef>
#include <algorithm>

#include "tbb/parallel_for.h"
#include "tbb/blocked_range2d.h"

#include "huira/images/image.hpp"
#include "huira/util/logger.hpp"

#include "huira/images/io/png_io.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void PSF<TSpectral>::build_polyphase_cache(int radius, int banks) {
        cache_.radius = radius;
        cache_.banks = banks;
        cache_.dim = 2 * radius + 1;

        // Resize the vector to hold (banks * banks) images
        cache_.kernels.resize(static_cast<std::size_t>(banks * banks));
        
        for (auto& img : cache_.kernels) {
            img = Image<TSpectral>(cache_.dim, cache_.dim);
        }

        generate_polyphase_data_();
    }

    template <IsSpectral TSpectral>
    const Image<TSpectral>& PSF<TSpectral>::get_kernel(float u, float v) const {
        if (cache_.kernels.empty()) {
            HUIRA_THROW_ERROR("PSF::get_kernel() - Polyphase cache is empty.");
        }



        // Convert 0.0-1.0 fraction to 0-(banks-1) integer index
        // Clamp avoids floating point epsilon errors
        int bx = std::clamp(static_cast<int>(u * static_cast<float>(cache_.banks)), 0, static_cast<int>(cache_.banks) - 1);
        int by = std::clamp(static_cast<int>(v * static_cast<float>(cache_.banks)), 0, static_cast<int>(cache_.banks) - 1);

        return cache_.kernels[static_cast<std::size_t>(by * cache_.banks + bx)];
    }

    template <IsSpectral TSpectral>
    void PSF<TSpectral>::generate_polyphase_data_() {
        // Dynamic Resolution Calculation
        constexpr int QUALITY_SAMPLES_1D = 64;
        int calculated_res = static_cast<int>(cache_.dim) * QUALITY_SAMPLES_1D;
        int lut_res = std::max(2048, calculated_res);

        std::vector<TSpectral> lut_data(static_cast<std::size_t>(lut_res) * static_cast<std::size_t>(lut_res));

        float max_radius = static_cast<float>(cache_.radius) + 1.0f;
        float lut_scale_inv = 1.0f / static_cast<float>(lut_res - 1);

        tbb::parallel_for(tbb::blocked_range2d<int>(0, lut_res, 0, lut_res),
            [&](const tbb::blocked_range2d<int>& r) {
                for (int y = r.rows().begin(); y < r.rows().end(); ++y) {
                    float v_norm = (static_cast<float>(y) * lut_scale_inv) * 2.0f - 1.0f;
                    float v_phys = v_norm * max_radius;

                    for (int x = r.cols().begin(); x < r.cols().end(); ++x) {
                        float u_norm = (static_cast<float>(x) * lut_scale_inv) * 2.0f - 1.0f;
                        float u_phys = u_norm * max_radius;
                        
                        std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(lut_res) + static_cast<std::size_t>(x);
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
            if (x_f < 0.0f || x_f >= static_cast<float>(lut_res - 1) ||
                y_f < 0.0f || y_f >= static_cast<float>(lut_res - 1)) {
                return TSpectral{};
            }

            // Explicit cast to int for indices
            int x0 = static_cast<int>(x_f);
            int y0 = static_cast<int>(y_f);

            // Optimization: If exact match
            if (x_f == static_cast<float>(x0) && y_f == static_cast<float>(y0)) {
                std::size_t idx = static_cast<std::size_t>(y0) * static_cast<std::size_t>(lut_res) + static_cast<std::size_t>(x0);
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

            return (c00 * (1.0f - dx) + c10 * dx) * (1.0f - dy) +
                (c01 * (1.0f - dx) + c11 * dx) * dy;
            };


        // Generate polyphase kernel cache from LUT:
        constexpr int INTEGRATION_STEPS = 16;
        constexpr float INV_SAMPLES_SQ = 1.0f / static_cast<float>(INTEGRATION_STEPS * INTEGRATION_STEPS);
        constexpr float SAMPLE_STEP = 1.0f / static_cast<float>(INTEGRATION_STEPS);

        tbb::parallel_for(tbb::blocked_range2d<int>(0, cache_.banks, 0, cache_.banks),
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
                                        float sub_x = ((static_cast<float>(sx) + 0.5f) * SAMPLE_STEP) - 0.5f;
                                        float sub_y = ((static_cast<float>(sy) + 0.5f) * SAMPLE_STEP) - 0.5f;

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

    template <IsSpectral TSpectral>
    void PSF<TSpectral>::normalize_kernel_(Image<TSpectral>& kernel, const TSpectral& total_energy) {
        // Pre-compute inverse to avoid division in the loop
        TSpectral scale;
        for (std::size_t i = 0; i < TSpectral::size(); ++i) {
            float e = total_energy[i];
            scale[i] = (e > 1e-9f) ? (1.0f / e) : 0.0f;
        }

        // Apply scale
        for (int y = 0; y < cache_.dim; ++y) {
            for (int x = 0; x < cache_.dim; ++x) {
                kernel(y, x) *= scale;
            }
        }
    }
}
