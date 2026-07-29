#pragma once

#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "fftw3.h"
#include "huira/util/logger.hpp"
#include "tbb/blocked_range.h"
#include "tbb/enumerable_thread_specific.h"
#include "tbb/parallel_for.h"

namespace huira {

namespace detail {

/**
 * @brief Process-wide cache of FFTW plans, keyed by padded transform size.
 *
 * FFTW plan creation is not thread-safe and (with FFTW_MEASURE) can be expensive, so plans
 * are created once per (height, width, effort) under a mutex and reused for the lifetime of
 * the process. Execution uses the new-array interface (fftwf_execute_dft_r2c / _c2r), which
 * is thread-safe as long as each thread supplies its own buffers, so a single cached plan
 * serves any number of concurrent convolutions.
 *
 * Buffers used during planning are allocated with fftwf_alloc_* and freed immediately after
 * planning; all subsequent buffers are allocated the same way, guaranteeing the alignment
 * that the plans were created with.
 */
class FftwPlanCache {
  public:
    struct Plans {
        fftwf_plan forward = nullptr; // real-to-complex
        fftwf_plan inverse = nullptr; // complex-to-real
    };

    static FftwPlanCache& instance()
    {
        static FftwPlanCache cache;
        return cache;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-negative"
    Plans get(int padded_height, int padded_width, bool measure)
    {
        const std::uint64_t key = (static_cast<std::uint64_t>(padded_height) << 33) |
                                  (static_cast<std::uint64_t>(padded_width) << 1) |
                                  static_cast<std::uint64_t>(measure ? 1 : 0);

        std::lock_guard<std::mutex> lock(mutex_);

        auto it = plans_.find(key);
        if (it != plans_.end()) {
            return it->second;
        }

        const std::size_t real_size =
            static_cast<std::size_t>(padded_width) * static_cast<std::size_t>(padded_height);
        const std::size_t complex_size = static_cast<std::size_t>(padded_width / 2 + 1) *
                                         static_cast<std::size_t>(padded_height);

        float* real_buf = fftwf_alloc_real(real_size);
        fftwf_complex* complex_buf = fftwf_alloc_complex(complex_size);
        if (real_buf == nullptr || complex_buf == nullptr) {
            fftwf_free(real_buf);
            fftwf_free(complex_buf);
            HUIRA_THROW_ERROR("FftwPlanCache::get - Failed to allocate planning buffers");
        }

        const unsigned flags = measure ? FFTW_MEASURE : FFTW_ESTIMATE;

        Plans plans;
        plans.forward =
            fftwf_plan_dft_r2c_2d(padded_height, padded_width, real_buf, complex_buf, flags);
        plans.inverse =
            fftwf_plan_dft_c2r_2d(padded_height, padded_width, complex_buf, real_buf, flags);

        fftwf_free(real_buf);
        fftwf_free(complex_buf);

        if (plans.forward == nullptr || plans.inverse == nullptr) {
            if (plans.forward != nullptr) {
                fftwf_destroy_plan(plans.forward);
            }
            if (plans.inverse != nullptr) {
                fftwf_destroy_plan(plans.inverse);
            }
            HUIRA_THROW_ERROR("FftwPlanCache::get - FFTW plan creation failed");
        }

        plans_[key] = plans;
        return plans;
    }
#pragma clang diagnostic pop

  private:
    FftwPlanCache() = default;
    ~FftwPlanCache()
    {
        for (auto& [key, plans] : plans_) {
            fftwf_destroy_plan(plans.forward);
            fftwf_destroy_plan(plans.inverse);
        }
    }

    std::mutex mutex_;
    std::unordered_map<std::uint64_t, Plans> plans_;
};

/**
 * @brief Per-thread scratch buffers for FFT convolution.
 */
struct FftScratch {
    float* real = nullptr;
    fftwf_complex* complex_buf = nullptr;
    std::size_t real_size = 0;
    std::size_t complex_size = 0;

    void ensure(std::size_t new_real_size, std::size_t new_complex_size)
    {
        if (real_size < new_real_size) {
            fftwf_free(real);
            real = fftwf_alloc_real(new_real_size);
            real_size = new_real_size;
        }
        if (complex_size < new_complex_size) {
            fftwf_free(complex_buf);
            complex_buf = fftwf_alloc_complex(new_complex_size);
            complex_size = new_complex_size;
        }
        if (real == nullptr || complex_buf == nullptr) {
            HUIRA_THROW_ERROR("FftScratch::ensure - Failed to allocate scratch buffers");
        }
    }

    ~FftScratch()
    {
        fftwf_free(real);
        fftwf_free(complex_buf);
    }

    FftScratch() = default;
    FftScratch(const FftScratch&) = delete;
    FftScratch& operator=(const FftScratch&) = delete;
};

template <typename PixelT>
inline float get_pixel_channel(const PixelT& pixel, std::size_t c)
{
    if constexpr (ImagePixelTraits<PixelT>::channels == 1) {
        (void)c;
        return static_cast<float>(pixel);
    } else if constexpr (IsVec<PixelT>) {
        return static_cast<float>(pixel[static_cast<int>(c)]);
    } else {
        return static_cast<float>(pixel[c]);
    }
}

template <typename PixelT>
inline void set_pixel_channel(PixelT& pixel, std::size_t c, float val)
{
    if constexpr (ImagePixelTraits<PixelT>::channels == 1) {
        (void)c;
        pixel = static_cast<PixelT>(val);
    } else if constexpr (IsVec<PixelT>) {
        pixel[static_cast<int>(c)] = val;
    } else {
        pixel[c] = val;
    }
}
} // namespace detail

template <IsImagePixel PixelT>
int FftConvolver<PixelT>::next_fast_size(int n)
{
    if (n <= 1) {
        return 1;
    }
    for (;; ++n) {
        int m = n;
        while (m % 2 == 0) {
            m /= 2;
        }
        while (m % 3 == 0) {
            m /= 3;
        }
        while (m % 5 == 0) {
            m /= 5;
        }
        while (m % 7 == 0) {
            m /= 7;
        }
        if (m == 1) {
            return n;
        }
    }
}

template <IsImagePixel PixelT>
FftConvolver<PixelT>::~FftConvolver()
{
    release_();
}

template <IsImagePixel PixelT>
FftConvolver<PixelT>::FftConvolver(FftConvolver&& other) noexcept
    : ready_(other.ready_), image_width_(other.image_width_), image_height_(other.image_height_),
      padded_width_(other.padded_width_), padded_height_(other.padded_height_),
      complex_cols_(other.complex_cols_), kernel_spectra_(std::move(other.kernel_spectra_)),
      forward_plan_(other.forward_plan_), inverse_plan_(other.inverse_plan_)
{
    other.kernel_spectra_.clear();
    other.ready_ = false;
}

template <IsImagePixel PixelT>
FftConvolver<PixelT>& FftConvolver<PixelT>::operator=(FftConvolver&& other) noexcept
{
    if (this != &other) {
        release_();
        ready_ = other.ready_;
        image_width_ = other.image_width_;
        image_height_ = other.image_height_;
        padded_width_ = other.padded_width_;
        padded_height_ = other.padded_height_;
        complex_cols_ = other.complex_cols_;
        kernel_spectra_ = std::move(other.kernel_spectra_);
        forward_plan_ = other.forward_plan_;
        inverse_plan_ = other.inverse_plan_;
        other.kernel_spectra_.clear();
        other.ready_ = false;
    }
    return *this;
}

template <IsImagePixel PixelT>
void FftConvolver<PixelT>::release_()
{
    for (fftwf_complex* spectrum : kernel_spectra_) {
        fftwf_free(spectrum);
    }
    kernel_spectra_.clear();
    ready_ = false;
    // Plans are owned by the global cache and are intentionally not destroyed here.
}

template <IsImagePixel PixelT>
void FftConvolver<PixelT>::set_kernel(const Image<PixelT>& kernel,
                                      Resolution image_resolution,
                                      FftPlanEffort effort)
{
    if constexpr (IsInteger<PixelT>) {
        HUIRA_THROW_ERROR("FftConvolver::set_kernel - Requires a floating-point pixel type");
    } else {
        release_();

        const int kw = kernel.width();
        const int kh = kernel.height();
        if (kw <= 0 || kh <= 0 || image_resolution.width <= 0 || image_resolution.height <= 0) {
            HUIRA_THROW_ERROR("FftConvolver::set_kernel - Empty kernel or image");
        }

        image_width_ = image_resolution.width;
        image_height_ = image_resolution.height;

        // Support for exact linear convolution, rounded up to FFT-friendly sizes. The extra
        // zero padding beyond (image + kernel - 1) has no effect on the result:
        padded_width_ = next_fast_size(image_width_ + kw - 1);
        padded_height_ = next_fast_size(image_height_ + kh - 1);
        complex_cols_ = padded_width_ / 2 + 1;

        const bool measure = (effort == FftPlanEffort::Measure);
        detail::FftwPlanCache::Plans plans =
            detail::FftwPlanCache::instance().get(padded_height_, padded_width_, measure);
        forward_plan_ = plans.forward;
        inverse_plan_ = plans.inverse;

        const std::size_t real_size =
            static_cast<std::size_t>(padded_width_) * static_cast<std::size_t>(padded_height_);
        const std::size_t complex_size =
            static_cast<std::size_t>(complex_cols_) * static_cast<std::size_t>(padded_height_);

        constexpr std::size_t num_channels = ImagePixelTraits<PixelT>::channels;
        const int kcx = kw / 2;
        const int kcy = kh / 2;

        // The inverse transform scale factor is folded into the cached kernel spectra so that
        // apply() does not need a separate normalization pass:
        const float norm =
            1.0f / (static_cast<float>(padded_width_) * static_cast<float>(padded_height_));

        kernel_spectra_.resize(num_channels, nullptr);

        float* real_buf = fftwf_alloc_real(real_size);
        if (real_buf == nullptr) {
            HUIRA_THROW_ERROR("FftConvolver::set_kernel - Failed to allocate kernel buffer");
        }

        for (std::size_t c = 0; c < num_channels; ++c) {
            fftwf_complex* spectrum = fftwf_alloc_complex(complex_size);
            if (spectrum == nullptr) {
                fftwf_free(real_buf);
                release_();
                HUIRA_THROW_ERROR("FftConvolver::set_kernel - Failed to allocate spectrum");
            }
            kernel_spectra_[c] = spectrum;

            // Pack the kernel centered at the origin with wrap-around, so that the "same"
            // region of the linear convolution lands at the top-left of the padded output:
            std::memset(real_buf, 0, real_size * sizeof(float));
            for (int ky = 0; ky < kh; ++ky) {
                const int dst_y = (ky - kcy + padded_height_) % padded_height_;
                for (int kx = 0; kx < kw; ++kx) {
                    const int dst_x = (kx - kcx + padded_width_) % padded_width_;
                    real_buf[static_cast<std::size_t>(dst_y) *
                                 static_cast<std::size_t>(padded_width_) +
                             static_cast<std::size_t>(dst_x)] =
                        detail::get_pixel_channel(kernel(kx, ky), c);
                }
            }

            fftwf_execute_dft_r2c(forward_plan_, real_buf, spectrum);

            for (std::size_t i = 0; i < complex_size; ++i) {
                spectrum[i][0] *= norm;
                spectrum[i][1] *= norm;
            }
        }

        fftwf_free(real_buf);
        ready_ = true;
    }
}

template <IsImagePixel PixelT>
void FftConvolver<PixelT>::apply(Image<PixelT>& image) const
{
    if constexpr (IsInteger<PixelT>) {
        HUIRA_THROW_ERROR("FftConvolver::apply - Requires a floating-point pixel type");
    } else {
        if (!ready_) {
            HUIRA_THROW_ERROR("FftConvolver::apply - set_kernel() has not been called");
        }
        if (image.width() != image_width_ || image.height() != image_height_) {
            HUIRA_THROW_ERROR("FftConvolver::apply - Image resolution does not match "
                              "the resolution given to set_kernel()");
        }

        const std::size_t real_size =
            static_cast<std::size_t>(padded_width_) * static_cast<std::size_t>(padded_height_);
        const std::size_t complex_size =
            static_cast<std::size_t>(complex_cols_) * static_cast<std::size_t>(padded_height_);

        constexpr std::size_t num_channels = ImagePixelTraits<PixelT>::channels;

        // Each spectral channel is independent; process them in parallel with per-thread
        // scratch buffers. Plan execution via the new-array interface is thread-safe:
        tbb::enumerable_thread_specific<detail::FftScratch> scratch_pool;

        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(0, num_channels, 1),
            [&](const tbb::blocked_range<std::size_t>& range) {
                detail::FftScratch& scratch = scratch_pool.local();
                scratch.ensure(real_size, complex_size);

                for (std::size_t c = range.begin(); c < range.end(); ++c) {
                    // Pack this channel top-left into the zero-padded buffer:
                    std::memset(scratch.real, 0, real_size * sizeof(float));
                    for (int y = 0; y < image_height_; ++y) {
                        float* row = scratch.real + static_cast<std::size_t>(y) *
                                                        static_cast<std::size_t>(padded_width_);
                        for (int x = 0; x < image_width_; ++x) {
                            row[x] = detail::get_pixel_channel(image(x, y), c);
                        }
                    }

                    fftwf_execute_dft_r2c(forward_plan_, scratch.real, scratch.complex_buf);

                    // Pointwise multiply with the cached (pre-normalized) kernel spectrum:
                    const fftwf_complex* k_spec = kernel_spectra_[c];
                    for (std::size_t i = 0; i < complex_size; ++i) {
                        const float re = scratch.complex_buf[i][0] * k_spec[i][0] -
                                         scratch.complex_buf[i][1] * k_spec[i][1];
                        const float im = scratch.complex_buf[i][0] * k_spec[i][1] +
                                         scratch.complex_buf[i][1] * k_spec[i][0];
                        scratch.complex_buf[i][0] = re;
                        scratch.complex_buf[i][1] = im;
                    }

                    fftwf_execute_dft_c2r(inverse_plan_, scratch.complex_buf, scratch.real);

                    // Unpack the valid "same" region from the top-left of the padded result:
                    for (int y = 0; y < image_height_; ++y) {
                        const float* row =
                            scratch.real +
                            static_cast<std::size_t>(y) * static_cast<std::size_t>(padded_width_);
                        for (int x = 0; x < image_width_; ++x) {
                            detail::set_pixel_channel(image(x, y), c, row[x]);
                        }
                    }
                }
            });
    }
}
} // namespace huira
