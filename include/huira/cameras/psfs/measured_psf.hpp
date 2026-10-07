#pragma once

#include "huira/cameras/psfs/psf.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {

/**
 * @brief What the values of a measured PSF are, so that they become pixel values correctly.
 *
 * The optics spread a point's light into a continuous pattern on the sensor, and each pixel
 * collects that pattern over its own area. A measurement holds samples either of the pattern
 * itself, or of what pixels collect from it. If it came from the sensor being simulated, it is
 * PixelIntegrated; otherwise it is almost always PointSampled.
 */
enum class PSFSampling {
    /// The pattern's brightness at each sample's position: a PSF from optical design software
    /// (e.g. a Huygens or FFT PSF), from a model, or measured with pixels much smaller than the
    /// simulated sensor's. Huira interpolates between the samples and adds up the pattern over
    /// each pixel. Sample it finely, 4 or more samples per pixel.
    PointSampled,

    /// What a pixel of the simulated sensor collects with its center at each sample: a star's
    /// image cut from a frame taken with that sensor (1 sample per pixel), or an effective PSF
    /// combined from many such images at subpixel offsets (more). The pixel's area is already
    /// included, so Huira uses the values as they are.
    PixelIntegrated,
};

/**
 * @brief Point spread function defined by user-supplied measured (or precomputed) data.
 *
 * Allows the exact response of a real optical system - measured on a bench, extracted from
 * on-orbit star imagery, or produced by an external optical design tool - to be used in place
 * of an analytic model. The measurement is provided as an image of samples on a regular grid,
 * together with the sampling density relative to sensor pixels:
 *
 *   - samples_per_pixel = 1: the measurement is at native sensor resolution.
 *   - samples_per_pixel = N: the measurement is N-times oversampled per axis.
 *
 * and what the samples are (see PSFSampling): the PSF's intensity at points, or the light a
 * pixel centered there receives. The two differ by an integration over a pixel, which matters
 * most at the sensor's own resolution: a star imaged into a single pixel, taken as point
 * samples, would be integrated over the pixel once more, and spread 44% of its light into the
 * neighboring pixels.
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
                PSFSampling sampling,
                int radius = 0,
                int banks = 16);
    ~MeasuredPSF() override = default;

    MeasuredPSF(const MeasuredPSF&) = delete;
    MeasuredPSF& operator=(const MeasuredPSF&) = delete;

    TSpectral evaluate(float x, float y) override;

    [[nodiscard]] bool is_pixel_integrated() const override
    {
        return sampling_ == PSFSampling::PixelIntegrated;
    }

    /// What the samples are.
    [[nodiscard]] PSFSampling sampling() const { return sampling_; }

    /**
     * @brief The largest radius (in sensor pixels) covered by the measured data.
     */
    int measured_radius() const { return measured_radius_; }

  private:
    Image<TSpectral> data_;
    float samples_per_pixel_;
    PSFSampling sampling_;
    float center_x_;
    float center_y_;
    int measured_radius_;
};
} // namespace huira

#include "huira_impl/cameras/psfs/measured_psf.ipp"
