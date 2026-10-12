#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include "fftw3.h"
#include "huira/concepts/pixel_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/images/image.hpp"

namespace huira {

namespace detail {
struct FftState;
} // namespace detail

/**
 * @brief Planning effort passed to FFTW when preparing transforms.
 *
 * Estimate uses heuristics and is nearly instant to plan. Measure benchmarks candidate
 * algorithms at planning time (slower to set up, faster to execute). Plans are cached globally
 * per transform size, so Measure's setup cost is paid at most once per size per process.
 */
enum class FftPlanEffort { Estimate, Measure };

/**
 * @brief The precision an FftConvolver transforms in.
 *
 * Single takes about half the memory and, with measured plans, less time. Each output pixel
 * of an FFT sums every frequency, and each frequency every input pixel, so every pixel carries
 * rounding error in proportion to the whole image: in single precision about 1e-7 of its
 * brightest content, in double about 1e-16. See FftConvolver::rounding_error().
 */
enum class FftPrecision { Single, Double };

/**
 * @brief Reusable FFT-based convolution engine with cached plans and kernel spectra.
 *
 * Performs linear convolution of an image with an arbitrary kernel via zero-padded FFTs.
 *
 * - Rounds padded transform dimensions up to the next 7-smooth size (2^a 3^b 5^c 7^d),
 *   avoiding FFTW's slow generic algorithms for sizes with large prime factors.
 * - Caches FFTW plans globally per transform size and precision, so repeated convolutions (and
 *   repeated construction of convolvers) never re-plan.
 * - Transforms the kernel once per precision, in double precision, and caches its spectrum per
 *   channel, so applying the same kernel to successive frames costs one forward and one inverse
 *   transform per channel instead of two forward and one inverse. A precision's spectra are
 *   made the first time it is applied in, so that only those used take memory.
 * - Processes spectral channels in parallel via TBB.
 * - Transforms in place, so each channel needs one buffer, not two.
 *
 * The kernel is treated as centered at pixel (width/2, height/2), and convolution semantics
 * match "stamping" the kernel as-drawn around each source: a kernel whose mass lies left of
 * center shifts image content to the left. Total kernel energy is preserved exactly as given
 * (no implicit normalization).
 *
 * @tparam PixelT The pixel type.
 */
template <IsImagePixel PixelT>
class FftConvolver {
  public:
    static constexpr std::size_t CHANNELS = ImagePixelTraits<PixelT>::channels;
    /// A value per channel. Named, so that definitions outside the class can name their return
    /// type as typename FftConvolver<PixelT>::ChannelValues, which MSVC matches to the
    /// declaration where it does not match std::array<double, FftConvolver<PixelT>::CHANNELS>.
    using ChannelValues = std::array<double, CHANNELS>;

    FftConvolver();
    ~FftConvolver();

    FftConvolver(const FftConvolver&) = delete;
    FftConvolver& operator=(const FftConvolver&) = delete;
    FftConvolver(FftConvolver&& other) noexcept;
    FftConvolver& operator=(FftConvolver&& other) noexcept;

    void set_kernel(const Image<PixelT>& kernel,
                    Resolution image_resolution,
                    FftPlanEffort effort = FftPlanEffort::Estimate);

    void apply(Image<PixelT>& image, FftPrecision precision = FftPrecision::Double) const;
    void apply(Image<PixelT>& image, FftPrecision precision, FftPlanEffort effort) const;

    [[nodiscard]] ChannelValues rounding_error(const Image<PixelT>& image,
                                               FftPrecision precision) const;
    [[nodiscard]] ChannelValues rounding_error(const ChannelValues& largest,
                                               FftPrecision precision) const;

    /// Whether set_kernel() has been called successfully.
    bool is_ready() const { return ready_; }

    /// Resolution the convolver was configured for.
    Resolution image_resolution() const { return Resolution{image_width_, image_height_}; }

    /// The memory the kernel's spectra and the transforms' buffers take now.
    [[nodiscard]] std::size_t memory_bytes() const;

    static int next_fast_size(int n);

  private:
    /// The rounding error where the frame is dark over eps log2(n) max|x| sum|k|, with a margin
    /// (see rounding_error()).
    static constexpr double ROUNDING_FACTOR = 0.75;

    bool ready_ = false;
    FftPlanEffort effort_ = FftPlanEffort::Estimate;

    int image_width_ = 0;
    int image_height_ = 0;
    int padded_width_ = 0;
    int padded_height_ = 0;

    /// The kernel as given, which each precision's spectra are made from when first needed.
    Image<PixelT> kernel_;
    /// Each channel's kernel's sum of absolute values, for rounding_error().
    std::array<double, CHANNELS> kernel_sum_{};

    /// Each precision's plans, kernel spectra and buffers, made as needed.
    mutable std::unique_ptr<detail::FftState> state_;
};
} // namespace huira

#include "huira_impl/images/fft_convolver.ipp"
