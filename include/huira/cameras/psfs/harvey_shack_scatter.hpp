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
 * This component is achromatic: all spectral channels receive the same profile. It is
 * intended to be mixed with a diffraction core (e.g. AiryDisk) by energy fraction to form the
 * total system PSF; the mixing is performed by CameraModel::get_psf_convolution_kernel().
 *
 * Note: this component is used for whole-image convolution via generate_convolution_kernel()
 * and does not require (or benefit from) a polyphase cache.
 *
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
class HarveyShackScatter : public PSF<TSpectral> {
  public:
    HarveyShackScatter(float falloff_exponent, float r0, float cutoff_radius = 0.f);
    ~HarveyShackScatter() override = default;

    TSpectral evaluate(float x, float y) override;

  private:
    float falloff_exponent_;
    float r0_inv_sq_;
    float cutoff_radius_sq_;
};
} // namespace huira

#include "huira_impl/cameras/psfs/harvey_shack_scatter.ipp"
