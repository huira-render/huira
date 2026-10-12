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
 * @brief Whether a measured PSF is that of a point in focus, or already holds the defocus the
 * camera's focus gives it.
 */
enum class PSFFocus {
    /// The PSF of a point in focus: the camera's focus blurs it as it blurs the aperture's
    /// diffraction, for stars and for each unresolved object at its own depth.
    InFocus,

    /// The PSF as the camera, focused as it is, images a star: no defocus is added to it, and
    /// every unresolved source is drawn with it as it is, whatever its depth. Path tracing still
    /// gives resolved bodies their depth of field, which this PSF, convolved with them, then
    /// blurs again, so their defocus is approximate at best.
    IncludesDefocus,
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
 * before being supplied. Normalization of the supplied data does not matter: the PSF is taken
 * as complete, holding all of a source's light within the measured extent, out to its farthest
 * sample (radius()). To use less of a measurement, crop it before setting it.
 *
 * The renderer draws unresolved sources from the samples on their grid (see GridPsf): the cubic
 * spline through them, integrated over each pixel for point samples, a still source at its
 * exact position and a moving one along its path. How well that follows the real pattern
 * depends on the sampling: a pattern sampled at least twice as finely as its finest detail is
 * followed within a few parts in 10^3. evaluate() bilinearly interpolates the samples and
 * returns zero outside the measured extent.
 *
 * A MeasuredPSF takes the place of the aperture's diffraction pattern. Real measurements are
 * limited in dynamic range and rarely capture the faint far wings, so scattered light (see
 * CameraModel::set_scatter()) and veiling glare are still added to it by the camera model.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class MeasuredPSF : public PSF<TSpectral> {
  public:
    MeasuredPSF(const Image<TSpectral>& data,
                float samples_per_pixel,
                PSFSampling sampling,
                PSFFocus focus = PSFFocus::InFocus);
    ~MeasuredPSF() override = default;

    MeasuredPSF(const MeasuredPSF&) = delete;
    MeasuredPSF& operator=(const MeasuredPSF&) = delete;

    TSpectral evaluate(float x, float y) override;

    [[nodiscard]] bool is_pixel_integrated() const override
    {
        return sampling_ == PSFSampling::PixelIntegrated;
    }

    [[nodiscard]] bool includes_defocus() const override
    {
        return focus_ == PSFFocus::IncludesDefocus;
    }

    /// What the samples are.
    [[nodiscard]] PSFSampling sampling() const { return sampling_; }

    /// The samples, as given.
    [[nodiscard]] const Image<TSpectral>& data() const { return data_; }

    /// Samples per sensor pixel, per axis.
    [[nodiscard]] float samples_per_pixel() const { return samples_per_pixel_; }

  private:
    Image<TSpectral> data_;
    float samples_per_pixel_;
    PSFSampling sampling_;
    PSFFocus focus_;
    float center_x_;
    float center_y_;
};
} // namespace huira

#include "huira_impl/cameras/psfs/measured_psf.ipp"
