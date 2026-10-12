#pragma once

#include "huira/cameras/psfs/psf.hpp"
#include "huira/concepts/spectral_concepts.hpp"

namespace huira {

/**
 * @brief Harvey-Shack style scattered-light point spread component.
 *
 * Models the broad, smooth "wings" of a real optical system's PSF caused by surface
 * micro-roughness, coating defects, and particulate contamination, using an image-plane
 * power-law profile:
 *
 *     I(r) = (1 + (r / r0)^2)^(-b / 2)
 *
 * which is flat near the origin (finite, integrable core) and falls off as r^-b for r >> r0.
 * Exponents b of roughly 2 to 3 match measured stellar PSF wings (King 1971, Racine 1996).
 *
 * This component is achromatic: all spectral channels receive the same profile. Set as a PSF
 * (see CameraModel::set_psf()), it is taken as complete within its radius. The camera's own
 * scattered light, added to diffraction or to a PSF that was set, is set in angles with
 * CameraModel::set_scatter() instead, and reaches across the frame.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class HarveyShackScatter : public PSF<TSpectral> {
  public:
    HarveyShackScatter(float falloff_exponent, float r0, float cutoff_radius = 0.f);
    ~HarveyShackScatter() override = default;

    HarveyShackScatter(const HarveyShackScatter&) = delete;
    HarveyShackScatter& operator=(const HarveyShackScatter&) = delete;

    TSpectral evaluate(float x, float y) override;

  private:
    float falloff_exponent_;
    float r0_inv_sq_;
    float cutoff_radius_sq_;
};
} // namespace huira

#include "huira_impl/cameras/psfs/harvey_shack_scatter.ipp"
