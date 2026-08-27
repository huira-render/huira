#pragma once

#include <memory>
#include <optional>
#include <variant>

#include "huira/cameras/psfs/psf.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {

/**
 * @brief Perfect optics: point sources land as delta functions.
 *
 * Select this to render an idealized pinhole response, for example when validating
 * astrometry or projection geometry where any blur is unwanted.
 */
struct IdealCore {};

/**
 * @brief Diffraction core derived from the aperture, focal length, and pixel pitch.
 *
 * This is the default core. The kernel is rebuilt automatically whenever any of the
 * quantities it depends on change.
 */
struct DiffractionCore {
    /// Stamping kernel radius in pixels. Unset derives the radius from the Airy radius.
    std::optional<int> radius;

    /// Number of polyphase banks per axis used for subpixel stamping.
    int banks = 16;

    static float airy_radius_pixels(double max_wavelength, double f_number, float min_pitch);
    static int derive_radius(double max_wavelength, double f_number, float min_pitch);
};

/**
 * @brief Core built from user-supplied measured PSF data.
 *
 * See MeasuredPSF for the measurement conventions: samples must be centered, and
 * samples_per_pixel gives the measurement density relative to sensor pixels.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
struct MeasuredCore {
    /// Measured PSF samples, centered on the image.
    Image<TSpectral> data;

    /// Measurement samples per sensor pixel per axis.
    float samples_per_pixel = 1.f;

    /// Stamping kernel radius in pixels. Unset uses the measured extent.
    std::optional<int> radius;

    /// Number of polyphase banks per axis used for subpixel stamping.
    int banks = 16;
};

/**
 * @brief Core supplied as an already-constructed PSF instance.
 *
 * Use this for PSF types outside the library. The instance is shared rather than copied,
 * so it must remain valid for the lifetime of the camera and must be safe to read from
 * multiple threads once its polyphase cache has been built.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
struct CustomCore {
    std::shared_ptr<PSF<TSpectral>> psf;
};

/**
 * @brief Harvey-Shack scattered-light wings.
 *
 * Models the broad halo produced by surface micro-roughness, coating defects, and
 * contamination, as the image-plane profile
 *
 *     I(r) = (1 + (r / r0)^2)^(-b / 2)
 *
 * `fraction` of the total system energy follows this profile; the remainder stays in the
 * core. Exponents between 2 and 3 match measured stellar PSF wings. The profile only
 * carries finite energy for b > 2, so a smaller exponent requires an explicit
 * `cutoff_radius`.
 */
struct HarveyShack {
    /// Fraction of total energy diverted into the wings, in [0, 1).
    float fraction = 0.f;

    /// Power-law falloff exponent b. Must exceed 2 unless cutoff_radius is set.
    float exponent = 2.5f;

    /// Shoulder radius in pixels, inside which the profile is flat.
    float r0 = 0.5f;

    /// Fraction of the scattered energy the convolution kernel must span, in (0, 1).
    float captured_energy = 0.98f;

    /// Convolution kernel radius in pixels. Unset derives the radius from captured_energy.
    std::optional<int> kernel_radius;

    /// Radius in pixels beyond which the profile is zero. Unset leaves it untruncated.
    std::optional<float> cutoff_radius;

    static float radius_for_energy(float captured_energy, float exponent, float r0);
    static float energy_within(float radius, float exponent, float r0);

    void validate() const;
};

/**
 * @brief Energy diverted out of the PSF core by scattering and glare.
 *
 * Both components describe the same stray-light budget: the core retains
 * 1 - scatter.fraction - veiling_glare of the total energy.
 */
struct StrayLight {
    /// Scattered-light wings. Unset means no scatter component.
    std::optional<HarveyShack> scatter;

    /// Fraction of energy redistributed uniformly across the frame, in [0, 1].
    float veiling_glare = 0.f;

    void validate() const;
};

/**
 * @brief Complete optical description of a camera.
 *
 * Everything present here is applied to both path-traced geometry and unresolved point
 * sources. Use Renderer::set_psf_application() to trade fidelity for speed; the camera
 * itself carries no enable flags.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
struct Optics {
    using Core =
        std::variant<IdealCore, DiffractionCore, MeasuredCore<TSpectral>, CustomCore<TSpectral>>;

    Core core = DiffractionCore{};
    StrayLight stray_light{};

    static Optics ideal();
    static Optics diffraction_limited();
    static Optics realistic();

    void validate() const;
};

/**
 * @brief Selects which parts of the image the optical model is applied to.
 *
 * `Full` is the default and matches the camera's optical description. The other modes are
 * speed controls and produce a knowingly incomplete image.
 */
enum class PSFApplication {
    Full,       ///< Apply to path-traced geometry and unresolved sources.
    PointsOnly, ///< Apply to unresolved sources only; skip the whole-image convolution.
    Off         ///< Apply nothing; render as if the optics were ideal.
};

/**
 * @brief Render-time constraints applied when the camera builds its optical kernels.
 */
struct OpticsBudget {
    /// Upper bound on any generated kernel radius, in pixels.
    int max_kernel_radius = 1024;

    /// Whether the whole-image convolution kernel is needed for this render.
    bool build_convolution_kernel = true;

    /// Forces aperture sampling on or off. Unset leaves the camera's automatic choice.
    std::optional<bool> aperture_sampling;
};
} // namespace huira

#include "huira_impl/cameras/optics.ipp"
