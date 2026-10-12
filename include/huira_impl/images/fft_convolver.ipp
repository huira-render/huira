#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fftw3.h"
#include "huira/util/logger.hpp"
#include "huira/util/macros.hpp"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

namespace huira {

namespace detail {

/// FFTW's interface in one precision: fftw_* for double, fftwf_* for float.
template <class Real>
struct Fftw;

template <>
struct Fftw<double> {
    using Complex = fftw_complex;
    using Plan = fftw_plan;
    static Complex* alloc(std::size_t count) { return fftw_alloc_complex(count); }
    static void free(Complex* buffer) { fftw_free(buffer); }
    static Plan plan_r2c(int height, int width, double* real, Complex* complex, unsigned flags)
    {
        return fftw_plan_dft_r2c_2d(height, width, real, complex, flags);
    }
    static Plan plan_c2r(int height, int width, Complex* complex, double* real, unsigned flags)
    {
        return fftw_plan_dft_c2r_2d(height, width, complex, real, flags);
    }
    static void r2c(Plan plan, double* real, Complex* complex)
    {
        fftw_execute_dft_r2c(plan, real, complex);
    }
    static void c2r(Plan plan, Complex* complex, double* real)
    {
        fftw_execute_dft_c2r(plan, complex, real);
    }
    static void destroy(Plan plan) { fftw_destroy_plan(plan); }
};

template <>
struct Fftw<float> {
    using Complex = fftwf_complex;
    using Plan = fftwf_plan;
    static Complex* alloc(std::size_t count) { return fftwf_alloc_complex(count); }
    static void free(Complex* buffer) { fftwf_free(buffer); }
    static Plan plan_r2c(int height, int width, float* real, Complex* complex, unsigned flags)
    {
        return fftwf_plan_dft_r2c_2d(height, width, real, complex, flags);
    }
    static Plan plan_c2r(int height, int width, Complex* complex, float* real, unsigned flags)
    {
        return fftwf_plan_dft_c2r_2d(height, width, complex, real, flags);
    }
    static void r2c(Plan plan, float* real, Complex* complex)
    {
        fftwf_execute_dft_r2c(plan, real, complex);
    }
    static void c2r(Plan plan, Complex* complex, float* real)
    {
        fftwf_execute_dft_c2r(plan, complex, real);
    }
    static void destroy(Plan plan) { fftwf_destroy_plan(plan); }
};

/**
 * @brief The real view of a buffer transformed in place: each of its rows of padded_width reals
 * starts a complex row, padded_width / 2 + 1 complex values or twice as many reals, from the
 * last. Transforming in place halves the buffers.
 */
template <class Complex>
inline auto in_place_real(Complex* buffer)
{
    using Real = std::remove_extent_t<Complex>;
    return reinterpret_cast<Real*>(buffer);
}

/**
 * @brief Process-wide cache of FFTW plans, keyed by padded transform size, precision and
 * planning effort.
 *
 * FFTW plan creation is not thread-safe and (with FFTW_MEASURE) can be expensive, so plans
 * are created once per (height, width, precision, effort) under a mutex and reused for the
 * lifetime of the process. Execution uses the new-array interface (fftw_execute_dft_r2c /
 * _c2r), which is thread-safe as long as each thread supplies its own buffers, so a single
 * cached plan serves any number of concurrent convolutions.
 *
 * Buffers used during planning are allocated with fftw_alloc_* and freed immediately after
 * planning; all subsequent buffers are allocated the same way, guaranteeing the alignment
 * that the plans were created with.
 */
class FftwPlanCache {
  public:
    FftwPlanCache(const FftwPlanCache&) = delete;
    FftwPlanCache& operator=(const FftwPlanCache&) = delete;

    template <class Real>
    struct Plans {
        typename Fftw<Real>::Plan forward = nullptr; // real-to-complex
        typename Fftw<Real>::Plan inverse = nullptr; // complex-to-real
    };

    static FftwPlanCache& instance()
    {
        HUIRA_PER_MODULE_STATE_BEGIN
        static FftwPlanCache cache;
        HUIRA_PER_MODULE_STATE_END
        return cache;
    }

    /// Double precision's plans, as most callers want.
    Plans<double> get(int padded_height, int padded_width, bool measure)
    {
        return get<double>(padded_height, padded_width, measure);
    }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-negative"
#endif
    template <class Real>
    Plans<Real> get(int padded_height, int padded_width, bool measure)
    {
        using Api = Fftw<Real>;
        const std::uint64_t key = (static_cast<std::uint64_t>(padded_height) << 33) |
                                  (static_cast<std::uint64_t>(padded_width) << 1) |
                                  static_cast<std::uint64_t>(measure ? 1 : 0);

        std::lock_guard<std::mutex> lock(mutex_);
        auto& plans_by_key = table<Real>();
        auto it = plans_by_key.find(key);
        if (it != plans_by_key.end()) {
            return it->second;
        }

        const std::size_t complex_size = static_cast<std::size_t>(padded_width / 2 + 1) *
                                         static_cast<std::size_t>(padded_height);

        // In place: the real rows are padded to the complex rows' width (see in_place_real()).
        typename Api::Complex* complex_buf = Api::alloc(complex_size);
        if (complex_buf == nullptr) {
            HUIRA_THROW_ERROR("FftwPlanCache::get - Failed to allocate planning buffers");
        }
        Real* real_buf = in_place_real(complex_buf);

        const unsigned flags = measure ? FFTW_MEASURE : FFTW_ESTIMATE;

        Plans<Real> plans;
        plans.forward = Api::plan_r2c(padded_height, padded_width, real_buf, complex_buf, flags);
        plans.inverse = Api::plan_c2r(padded_height, padded_width, complex_buf, real_buf, flags);

        Api::free(complex_buf);

        if (plans.forward == nullptr || plans.inverse == nullptr) {
            if (plans.forward != nullptr) {
                Api::destroy(plans.forward);
            }
            if (plans.inverse != nullptr) {
                Api::destroy(plans.inverse);
            }
            HUIRA_THROW_ERROR("FftwPlanCache::get - FFTW plan creation failed");
        }

        plans_by_key[key] = plans;
        return plans;
    }
#ifdef __clang__
#pragma clang diagnostic pop
#endif

  private:
    FftwPlanCache() = default;
    ~FftwPlanCache()
    {
        for (auto& [key, plans] : doubles_) {
            Fftw<double>::destroy(plans.forward);
            Fftw<double>::destroy(plans.inverse);
        }
        for (auto& [key, plans] : singles_) {
            Fftw<float>::destroy(plans.forward);
            Fftw<float>::destroy(plans.inverse);
        }
    }

    template <class Real>
    std::unordered_map<std::uint64_t, Plans<Real>>& table()
    {
        if constexpr (std::is_same_v<Real, double>) {
            return doubles_;
        } else {
            return singles_;
        }
    }

    std::mutex mutex_;
    std::unordered_map<std::uint64_t, Plans<double>> doubles_;
    std::unordered_map<std::uint64_t, Plans<float>> singles_;
};

/**
 * @brief What an FftConvolver keeps for each precision it has applied in: its plans, its
 * kernel's spectra per channel (pre-scaled by the inverse transform's normalization), and a
 * buffer per channel for the transforms.
 */
template <class Real>
struct FftPrepared {
    using Api = Fftw<Real>;
    bool ready = false;
    std::vector<typename Api::Complex*> spectra;
    std::vector<typename Api::Complex*> scratch;
    std::size_t complex_size = 0;

    FftPrepared() = default;
    FftPrepared(const FftPrepared&) = delete;
    FftPrepared& operator=(const FftPrepared&) = delete;
    ~FftPrepared()
    {
        for (auto* buffer : spectra) {
            Api::free(buffer);
        }
        for (auto* buffer : scratch) {
            Api::free(buffer);
        }
    }

    [[nodiscard]] std::size_t memory_bytes() const
    {
        return (spectra.size() + scratch.size()) * complex_size * sizeof(typename Api::Complex);
    }
};

/// Both precisions' FftPrepared, behind a pointer so that FftConvolver stays movable.
struct FftState {
    FftPrepared<double> doubles;
    FftPrepared<float> singles;

    template <class Real>
    FftPrepared<Real>& prepared()
    {
        if constexpr (std::is_same_v<Real, double>) {
            return doubles;
        } else {
            return singles;
        }
    }
};

template <typename PixelT>
inline double get_pixel_channel(const PixelT& pixel, std::size_t c)
{
    if constexpr (ImagePixelTraits<PixelT>::channels == 1) {
        (void)c;
        return static_cast<double>(pixel);
    } else if constexpr (IsVec<PixelT>) {
        return static_cast<double>(pixel[static_cast<int>(c)]);
    } else {
        return static_cast<double>(pixel[c]);
    }
}

template <typename PixelT>
inline void set_pixel_channel(PixelT& pixel, std::size_t c, double val)
{
    if constexpr (ImagePixelTraits<PixelT>::channels == 1) {
        (void)c;
        pixel = static_cast<PixelT>(val);
    } else if constexpr (IsVec<PixelT>) {
        using Channel = std::remove_cvref_t<decltype(pixel[0])>;
        pixel[static_cast<int>(c)] = static_cast<Channel>(val);
    } else {
        using Channel = std::remove_cvref_t<decltype(pixel[0])>;
        pixel[c] = static_cast<Channel>(val);
    }
}

/**
 * @brief Makes a precision's plans, kernel spectra and buffers. The spectra are transformed in
 * double precision whatever the precision applied in, so that only the frame's transforms round
 * in single precision.
 */
template <class Real, class PixelT>
void prepare_fft(FftPrepared<Real>& prepared,
                 const Image<PixelT>& kernel,
                 int padded_width,
                 int padded_height)
{
    using Api = Fftw<Real>;
    constexpr std::size_t channels = ImagePixelTraits<PixelT>::channels;
    // The kernel is transformed once, so its plans need not be measured:
    const auto kernel_plans =
        FftwPlanCache::instance().get<double>(padded_height, padded_width, false);
    const auto complex_cols = static_cast<std::size_t>(padded_width / 2 + 1);
    const std::size_t complex_size = complex_cols * static_cast<std::size_t>(padded_height);
    const std::size_t real_stride = 2 * complex_cols;
    const int kw = kernel.width();
    const int kh = kernel.height();
    const int kcx = kw / 2;
    const int kcy = kh / 2;
    // The inverse transform's scale is folded into the spectra, so that applying them needs no
    // separate normalization pass:
    const double norm =
        1.0 / (static_cast<double>(padded_width) * static_cast<double>(padded_height));

    prepared.complex_size = complex_size;
    fftw_complex* transform = fftw_alloc_complex(complex_size);
    if (transform == nullptr) {
        HUIRA_THROW_ERROR("FftConvolver - Failed to allocate the kernel's transform");
    }
    double* real = in_place_real(transform);
    for (std::size_t c = 0; c < channels; ++c) {
        // Pack the kernel centered at the origin with wrap-around, so that the "same" region of
        // the linear convolution lands at the top-left of the padded output:
        std::memset(transform, 0, complex_size * sizeof(fftw_complex));
        for (int ky = 0; ky < kh; ++ky) {
            const int dst_y = (ky - kcy + padded_height) % padded_height;
            for (int kx = 0; kx < kw; ++kx) {
                const int dst_x = (kx - kcx + padded_width) % padded_width;
                real[static_cast<std::size_t>(dst_y) * real_stride +
                     static_cast<std::size_t>(dst_x)] = get_pixel_channel(kernel(kx, ky), c);
            }
        }
        fftw_execute_dft_r2c(kernel_plans.forward, real, transform);

        typename Api::Complex* spectrum = Api::alloc(complex_size);
        typename Api::Complex* scratch = Api::alloc(complex_size);
        if (spectrum == nullptr || scratch == nullptr) {
            Api::free(spectrum);
            Api::free(scratch);
            fftw_free(transform);
            HUIRA_THROW_ERROR("FftConvolver - Failed to allocate the kernel's spectrum");
        }
        prepared.spectra.push_back(spectrum);
        prepared.scratch.push_back(scratch);
        for (std::size_t i = 0; i < complex_size; ++i) {
            spectrum[i][0] = static_cast<Real>(transform[i][0] * norm);
            spectrum[i][1] = static_cast<Real>(transform[i][1] * norm);
        }
    }
    fftw_free(transform);
    prepared.ready = true;
}

/// Convolves an image in place in one precision; see FftConvolver::apply().
template <class Real, class PixelT>
void apply_fft(const FftPrepared<Real>& prepared,
               Image<PixelT>& image,
               int padded_width,
               int padded_height,
               bool measure)
{
    using Api = Fftw<Real>;
    const auto plans = FftwPlanCache::instance().get<Real>(padded_height, padded_width, measure);
    constexpr std::size_t channels = ImagePixelTraits<PixelT>::channels;
    const int width = image.width();
    const int height = image.height();
    const std::size_t complex_size = prepared.complex_size;
    // The real rows' stride, transformed in place (see in_place_real()):
    const std::size_t real_stride = 2 * static_cast<std::size_t>(padded_width / 2 + 1);

    // Each spectral channel is independent; process them in parallel, each against its own
    // scratch buffer. Plan execution via the new-array interface is thread-safe.
    //
    // Channel count caps the outer parallelism at three for RGB, which leaves most of a modern
    // machine idle, so the pack, pointwise-multiply and unpack passes are additionally split
    // over rows. All three are elementwise, so splitting them changes no result - only the
    // transforms themselves impose an ordering, and those are left to FFTW.
    //
    // The buffers are indexed by channel rather than drawn from thread-local storage: the
    // nested loops below block, and a blocked worker may pick up another channel's task on the
    // same thread, which would otherwise alias two channels onto one buffer.
    tbb::parallel_for(
        tbb::blocked_range<std::size_t>(0, channels, 1),
        [&](const tbb::blocked_range<std::size_t>& range) {
            for (std::size_t c = range.begin(); c < range.end(); ++c) {
                typename Api::Complex* buffer = prepared.scratch[c];
                Real* real = in_place_real(buffer);

                // Pack this channel top-left into the zero-padded buffer:
                tbb::parallel_for(
                    tbb::blocked_range<int>(0, padded_height),
                    [&](const tbb::blocked_range<int>& rows) {
                        for (int y = rows.begin(); y < rows.end(); ++y) {
                            Real* row = real + static_cast<std::size_t>(y) * real_stride;
                            if (y < height) {
                                for (int x = 0; x < width; ++x) {
                                    row[x] = static_cast<Real>(get_pixel_channel(image(x, y), c));
                                }
                                std::fill(row + width, row + real_stride, Real{0});
                            } else {
                                std::fill(row, row + real_stride, Real{0});
                            }
                        }
                    });

                Api::r2c(plans.forward, real, buffer);

                // Pointwise multiply with the cached (pre-normalized) kernel spectrum:
                const typename Api::Complex* k_spec = prepared.spectra[c];
                tbb::parallel_for(tbb::blocked_range<std::size_t>(0, complex_size),
                                  [&](const tbb::blocked_range<std::size_t>& block) {
                                      for (std::size_t i = block.begin(); i < block.end(); ++i) {
                                          const Real re = buffer[i][0] * k_spec[i][0] -
                                                          buffer[i][1] * k_spec[i][1];
                                          const Real im = buffer[i][0] * k_spec[i][1] +
                                                          buffer[i][1] * k_spec[i][0];
                                          buffer[i][0] = re;
                                          buffer[i][1] = im;
                                      }
                                  });

                Api::c2r(plans.inverse, buffer, real);

                // Unpack the valid "same" region from the top-left of the padded result:
                tbb::parallel_for(
                    tbb::blocked_range<int>(0, height), [&](const tbb::blocked_range<int>& rows) {
                        for (int y = rows.begin(); y < rows.end(); ++y) {
                            const Real* row = real + static_cast<std::size_t>(y) * real_stride;
                            for (int x = 0; x < width; ++x) {
                                set_pixel_channel(image(x, y), c, static_cast<double>(row[x]));
                            }
                        }
                    });
            }
        });
}
} // namespace detail

/**
 * @brief Returns the smallest 7-smooth integer (2^a 3^b 5^c 7^d) that is >= n.
 *
 * FFTW is fast for sizes whose prime factors are all small; sizes containing large prime
 * factors fall back to generic algorithms that can be an order of magnitude slower.
 */
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
FftConvolver<PixelT>::FftConvolver() = default;

template <IsImagePixel PixelT>
FftConvolver<PixelT>::~FftConvolver() = default;

template <IsImagePixel PixelT>
FftConvolver<PixelT>::FftConvolver(FftConvolver&& other) noexcept = default;

template <IsImagePixel PixelT>
FftConvolver<PixelT>& FftConvolver<PixelT>::operator=(FftConvolver&& other) noexcept = default;

/**
 * @brief Prepares the convolver for a given kernel and image resolution.
 *
 * Computes padded transform dimensions and keeps the kernel; each precision's plans and the
 * kernel's spectra in it are made the first time it is applied in. Safe to call again to change
 * the kernel or target resolution.
 *
 * @param kernel The convolution kernel (any size, including larger than the image)
 * @param image_resolution Resolution of the images that will be passed to apply()
 * @param effort FFTW planning effort (see FftPlanEffort)
 */
template <IsImagePixel PixelT>
void FftConvolver<PixelT>::set_kernel(const Image<PixelT>& kernel,
                                      Resolution image_resolution,
                                      FftPlanEffort effort)
{
    if constexpr (IsInteger<PixelT>) {
        HUIRA_THROW_ERROR("FftConvolver::set_kernel - Requires a floating-point pixel type");
    } else {
        ready_ = false;
        state_.reset();

        const int kw = kernel.width();
        const int kh = kernel.height();
        if (kw <= 0 || kh <= 0 || image_resolution.width <= 0 || image_resolution.height <= 0) {
            HUIRA_THROW_ERROR("FftConvolver::set_kernel - Empty kernel or image");
        }

        image_width_ = image_resolution.width;
        image_height_ = image_resolution.height;
        effort_ = effort;

        // Support for exact linear convolution, rounded up to FFT-friendly sizes. The extra
        // zero padding beyond (image + kernel - 1) has no effect on the result:
        padded_width_ = next_fast_size(image_width_ + kw - 1);
        padded_height_ = next_fast_size(image_height_ + kh - 1);

        kernel_ = kernel;
        kernel_sum_.fill(0.0);
        for (std::size_t i = 0; i < kernel.size(); ++i) {
            for (std::size_t c = 0; c < CHANNELS; ++c) {
                kernel_sum_[c] += std::abs(detail::get_pixel_channel(kernel[i], c));
            }
        }
        state_ = std::make_unique<detail::FftState>();
        ready_ = true;
    }
}

/**
 * @brief Convolves an image in place with the kernel given to set_kernel(), in a precision,
 * planned with the effort given to set_kernel().
 *
 * @param image The image to convolve. Must match the resolution given to set_kernel().
 * @param precision The precision to transform in (see rounding_error()).
 */
template <IsImagePixel PixelT>
void FftConvolver<PixelT>::apply(Image<PixelT>& image, FftPrecision precision) const
{
    apply(image, precision, effort_);
}

/**
 * @brief Convolves an image in place with the kernel given to set_kernel(), in a precision,
 * with plans made with an effort. The kernel's spectra in a precision are made the first time
 * it is applied in, and the plans the first time any convolver in the process uses them.
 *
 * @param image The image to convolve. Must match the resolution given to set_kernel().
 * @param precision The precision to transform in (see rounding_error()).
 * @param effort How hard FFTW plans the transforms (see FftPlanEffort).
 */
template <IsImagePixel PixelT>
void FftConvolver<PixelT>::apply(Image<PixelT>& image,
                                 FftPrecision precision,
                                 FftPlanEffort effort) const
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
        const bool measure = effort == FftPlanEffort::Measure;
        const auto run = [&](auto& prepared) {
            if (!prepared.ready) {
                detail::prepare_fft(prepared, kernel_, padded_width_, padded_height_);
            }
            detail::apply_fft(prepared, image, padded_width_, padded_height_, measure);
        };
        if (precision == FftPrecision::Single) {
            run(state_->singles);
        } else {
            run(state_->doubles);
        }
    }
}

/**
 * @brief About the most the rounding of a precision puts in each channel of an image's pixels
 * where the convolved image is dark: ROUNDING_FACTOR eps log2(n) max|x| sum|k|, for the image's
 * largest value max|x|, the kernel's sum of absolute values sum|k|, the n points transformed,
 * and the precision's unit roundoff eps.
 *
 * Each output pixel of an FFT sums every frequency, and each frequency every input pixel, so
 * rounding puts error everywhere in the frame, in proportion to its brightest content, where
 * nothing else is. (Bright pixels are off by their own value times eps, as with any arithmetic.)
 * Against double precision, the most a single-precision convolution was off where it was dark
 * (under 1e-5 of the brightest) was 0.1 to 0.5 eps max|x| sum|k| for points, and 1 to 7 for
 * discs, strips and half the frame, from 64 x 64 to 1920 x 1080 pixels and kernels 17 to 333
 * wide, rising with log2(n): ROUNDING_FACTOR holds the largest with a margin of 2.5.
 */
template <IsImagePixel PixelT>
typename FftConvolver<PixelT>::ChannelValues
FftConvolver<PixelT>::rounding_error(const Image<PixelT>& image, FftPrecision precision) const
{
    ChannelValues largest{};
    for (std::size_t i = 0; i < image.size(); ++i) {
        for (std::size_t c = 0; c < CHANNELS; ++c) {
            largest[c] = std::max(largest[c], std::abs(detail::get_pixel_channel(image[i], c)));
        }
    }
    return rounding_error(largest, precision);
}

/// The same for an image whose largest value in each channel is given.
template <IsImagePixel PixelT>
typename FftConvolver<PixelT>::ChannelValues
FftConvolver<PixelT>::rounding_error(const ChannelValues& largest, FftPrecision precision) const
{
    const double eps = precision == FftPrecision::Single ? 0.5 * 1.1920928955078125e-7
                                                         : 0.5 * 2.220446049250313e-16;
    const double points = static_cast<double>(padded_width_) * static_cast<double>(padded_height_);
    const double scale = ROUNDING_FACTOR * eps * std::log2(std::max(points, 2.0));
    ChannelValues error{};
    for (std::size_t c = 0; c < CHANNELS; ++c) {
        error[c] = scale * largest[c] * kernel_sum_[c];
    }
    return error;
}

/// The memory the kernel's spectra and the transforms' buffers take now.
template <IsImagePixel PixelT>
std::size_t FftConvolver<PixelT>::memory_bytes() const
{
    if (state_ == nullptr) {
        return 0;
    }
    return state_->doubles.memory_bytes() + state_->singles.memory_bytes();
}
} // namespace huira
