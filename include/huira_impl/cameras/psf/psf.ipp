#include <cstddef>
#include <algorithm>

#include "huira/images/image.hpp"
#include "huira/util/logger.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void PSF<TSpectral>::build_polyphase_cache(int radius, int banks) {
        cache_.radius = radius;
        cache_.banks = banks;
        cache_.dim = 2 * radius + 1;

        // Resize the vector to hold (banks * banks) images
        cache_.kernels.resize(banks * banks);
        
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
        // Quality Settings
        constexpr int SAMPLES = 8;
        constexpr float INV_SAMPLES = 1.0f / SAMPLES;
        constexpr float SAMPLE_STEP = 1.0f / SAMPLES;

        for (int by = 0; by < cache_.banks; ++by) {
            for (int bx = 0; bx < cache_.banks; ++bx) {

                Image<TSpectral>& kernel = cache_.kernels[by * cache_.banks + bx];

                // Calculate the sub-pixel shift for this bank
                // (These are the 'fine' shifts of the entire grid)
                float bank_offset_x = static_cast<float>(bx) / cache_.banks;
                float bank_offset_y = static_cast<float>(by) / cache_.banks;

                TSpectral total_energy{};

                for (int y = 0; y < cache_.dim; ++y) {
                    for (int x = 0; x < cache_.dim; ++x) {

                        // The center of the current kernel pixel (relative to kernel center)
                        float pixel_center_x = static_cast<float>(x - cache_.radius);
                        float pixel_center_y = static_cast<float>(y - cache_.radius);

                        TSpectral integrated_val{};
                        for (int sy = 0; sy < SAMPLES; ++sy) {
                            for (int sx = 0; sx < SAMPLES; ++sx) {
                                // Calculate sub-pixel offset within the pixel square [-0.5, 0.5]
                                // (sx + 0.5) puts us in the center of the sub-sample
                                float sub_x = ((sx + 0.5f) * SAMPLE_STEP) - 0.5f;
                                float sub_y = ((sy + 0.5f) * SAMPLE_STEP) - 0.5f;

                                float sample_x = pixel_center_x - bank_offset_x + sub_x;
                                float sample_y = pixel_center_y - bank_offset_y + sub_y;

                                integrated_val += evaluate(sample_x, sample_y);
                            }
                        }
                        integrated_val *= (INV_SAMPLES * INV_SAMPLES);

                        kernel(y, x) = integrated_val;
                        total_energy += integrated_val;
                    }
                }

                // Normalize so the kernel sums to 1.0 (conserves energy)
                normalize_kernel_(kernel, total_energy);
            }
        }
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
