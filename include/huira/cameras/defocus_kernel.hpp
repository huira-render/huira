#pragma once

#include <cstddef>
#include <limits>
#include <vector>

#include "huira/cameras/apertures/aperture.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {

/**
 * @brief Polyphase stamps of the defocus blur of a point source.
 *
 * The defocus blur of a point is the aperture's shape scaled to the blur radius (see
 * Aperture::rasterize_shape()). It is stored pre-shifted by bank / banks of a pixel in each
 * axis, so that each source can be stamped at its subpixel position, and each stamp is
 * normalized to unit sum.
 *
 * Owned by CameraModel, which builds it when it is needed and out of date: see
 * CameraModel::precompute().
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class DefocusKernel {
  public:
    /// Memory the stamps of one blur may take before fewer banks are used. See build().
    static constexpr std::size_t DEFAULT_BUDGET_BYTES = std::size_t{256} * 1024 * 1024;

    void build(const Aperture<TSpectral>& aperture,
               float radius_pixels,
               int max_banks,
               int max_half_extent = std::numeric_limits<int>::max(),
               std::size_t budget_bytes = DEFAULT_BUDGET_BYTES);
    void clear();

    /// True when there is no blur to apply (in focus, or not built).
    [[nodiscard]] bool empty() const { return kernels_.empty(); }

    /// Blur radius in pixels.
    [[nodiscard]] float radius() const { return radius_; }

    /// Kernels are (2 * half_extent() + 1) pixels square.
    [[nodiscard]] int half_extent() const { return half_extent_; }

    /// Subpixel banks per axis.
    [[nodiscard]] int banks() const { return banks_; }

    [[nodiscard]] const Image<float>& get(float u, float v) const;

  private:
    float radius_ = 0.f;
    int half_extent_ = 0;
    int banks_ = 0;
    std::vector<Image<float>> kernels_;
};

} // namespace huira

#include "huira_impl/cameras/defocus_kernel.ipp"
