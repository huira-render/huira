#pragma once

#include "huira/cameras/psfs/psf.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {

/**
 * @brief Point spread function defined by user-supplied measured (or precomputed) data.
 *
 * Allows the exact response of a real optical system - measured on a bench, extracted from
 * on-orbit star imagery, or produced by an external optical design tool - to be used in place
 * of an analytic model. The measurement is provided as an image of intensity samples on a
 * regular grid, together with the sampling density relative to sensor pixels:
 *
 *   - samples_per_pixel = 1: the measurement is at native sensor resolution.
 *   - samples_per_pixel = N: the measurement is N-times oversampled per axis.
 *
 * The measurement is assumed to be centered on the image: the PSF origin is placed at the
 * geometric center ((width - 1) / 2, (height - 1) / 2), so measurements should be centroided
 * before being supplied. evaluate() bilinearly interpolates the samples and returns zero
 * outside the measured extent. Normalization of the supplied data does not matter; kernels
 * generated from this PSF are normalized to unit energy.
 *
 * Subpixel fidelity of the polyphase stamping cache is limited by the sampling density: with
 * samples_per_pixel = 1 the subpixel banks are pure interpolation, while an oversampled
 * measurement (4x or more per axis) captures genuine subpixel structure. For whole-image
 * convolution this matters far less, since the convolution grid has no subpixel offset.
 *
 * A MeasuredPSF is the "core" component of the total system PSF: because real measurements
 * are dynamic-range limited, they rarely capture the faint far wings, so Harvey-Shack scatter
 * and veiling glare can still be layered on top via the camera model.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class MeasuredPSF : public PSF<TSpectral> {
  public:
    MeasuredPSF(const Image<TSpectral>& data,
                float samples_per_pixel,
                int radius = 0,
                int banks = 16);
    ~MeasuredPSF() override = default;

    TSpectral evaluate(float x, float y) override;

    /**
     * @brief The largest radius (in sensor pixels) covered by the measured data.
     */
    int measured_radius() const { return measured_radius_; }

  private:
    Image<TSpectral> data_;
    float samples_per_pixel_;
    float center_x_;
    float center_y_;
    int measured_radius_;
};
} // namespace huira

#include "huira_impl/cameras/psfs/measured_psf.ipp"
