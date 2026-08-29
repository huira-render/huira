#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "fftw3.h"
#include "huira/concepts/pixel_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/images/image.hpp"

namespace huira {

namespace detail {
struct FftScratch;
struct FftScratchPool;
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
 * @brief Reusable FFT-based convolution engine with cached plans and kernel spectra.
 *
 * Performs linear convolution of an image with an arbitrary kernel via zero-padded FFTs.
 *
 * - Rounds padded transform dimensions up to the next 7-smooth size (2^a 3^b 5^c 7^d),
 *   avoiding FFTW's slow generic algorithms for sizes with large prime factors.
 * - Caches FFTW plans globally per transform size, so repeated convolutions (and repeated
 *   construction of convolvers) never re-plan.
 * - Transforms the kernel once in set_kernel() and caches its spectrum per channel, so
 *   applying the same kernel to successive frames costs one forward and one inverse
 *   transform per channel instead of two forward and one inverse.
 * - Processes spectral channels in parallel via TBB.
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
    FftConvolver() = default;
    ~FftConvolver();

    FftConvolver(const FftConvolver&) = delete;
    FftConvolver& operator=(const FftConvolver&) = delete;
    FftConvolver(FftConvolver&& other) noexcept;
    FftConvolver& operator=(FftConvolver&& other) noexcept;

    void set_kernel(const Image<PixelT>& kernel,
                    Resolution image_resolution,
                    FftPlanEffort effort = FftPlanEffort::Estimate);

    void apply(Image<PixelT>& image) const;

    /// Whether set_kernel() has been called successfully.
    bool is_ready() const { return ready_; }

    /// Resolution the convolver was configured for.
    Resolution image_resolution() const { return Resolution{image_width_, image_height_}; }

    static int next_fast_size(int n);

  private:
    void release_();

    bool ready_ = false;

    int image_width_ = 0;
    int image_height_ = 0;
    int padded_width_ = 0;
    int padded_height_ = 0;
    int complex_cols_ = 0;

    // Cached per-channel kernel spectra, pre-scaled by the inverse-FFT normalization factor
    std::vector<fftwf_complex*> kernel_spectra_;

    // Cached plans (owned by the global plan cache; never destroyed here):
    fftwf_plan forward_plan_ = nullptr;
    fftwf_plan inverse_plan_ = nullptr;

    // Per-channel transform buffers, retained between apply() calls.
    mutable std::unique_ptr<detail::FftScratchPool> scratch_pool_;
};
} // namespace huira

#include "huira_impl/images/fft_convolver.ipp"
