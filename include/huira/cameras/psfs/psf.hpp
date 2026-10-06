
#pragma once

#include <atomic>
#include <mutex>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {

/**
 * @brief Abstract base class for point spread functions (PSF).
 *
 * Defines the interface and cache management for all PSF types, supporting polyphase kernel
 * generation.
 *
 * The polyphase cache (the stamps used for unresolved sources) is built on first use rather than
 * on construction: set_polyphase_size() only records its size. It can be built ahead of time
 * with ensure_polyphase_cache(), which CameraModel::precompute() does. Building is
 * thread-safe, so concurrent first uses build it once.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class PSF {
  public:
    PSF() = default;
    virtual ~PSF() = default;

    PSF(const PSF&) = delete;
    PSF& operator=(const PSF&) = delete;

    virtual TSpectral evaluate(float x, float y) = 0;

    /**
     * @brief Whether evaluate() gives the light a pixel centered at (x, y) receives, rather than
     * the PSF's intensity at that point.
     *
     * Stamps and convolution kernels integrate a PSF's intensity over each pixel. One that is
     * already integrated over a pixel (see PSFSampling::PixelIntegrated) is instead taken at
     * each pixel's center, so that it is not integrated twice.
     */
    [[nodiscard]] virtual bool is_pixel_integrated() const { return false; }

    void set_polyphase_size(int radius, int banks);
    void build_polyphase_cache(int radius, int banks);
    void ensure_polyphase_cache() const;

    /// True when the polyphase cache has been built for its current size.
    [[nodiscard]] bool has_polyphase_cache() const
    {
        return cache_built_.load(std::memory_order_acquire);
    }

    static void check_polyphase_size(int radius, int banks);

    Image<TSpectral> generate_convolution_kernel(int radius);

    const Image<TSpectral>& get_kernel(float u, float v) const;
    std::vector<Image<TSpectral>> get_all_kernels() const
    {
        ensure_polyphase_cache();
        return cache_.kernels;
    }

    int get_radius() const { return cache_.radius; }
    int get_banks() const { return cache_.banks; }

  protected:
    struct PolyphaseCache {
        int radius = 0;
        int dim = 0;
        int banks = 0;
        std::vector<Image<TSpectral>> kernels;
    } cache_;

  private:
    void generate_polyphase_data_();
    void fill_pixel_integrated_bank_(int bx, int by);
    void normalize_kernel_(Image<TSpectral>& kernel, const TSpectral& total_energy);

    mutable std::mutex cache_mutex_;
    mutable std::atomic<bool> cache_built_{false};
};

template <typename T>
struct is_psf : std::false_type {};

template <template <typename> class Derived, typename TSpectral>
    requires std::derived_from<Derived<TSpectral>, PSF<TSpectral>>
struct is_psf<Derived<TSpectral>> : std::true_type {};

template <typename T>
concept IsPSF = is_psf<T>::value;
} // namespace huira

#include "huira_impl/cameras/psfs/psf.ipp"
