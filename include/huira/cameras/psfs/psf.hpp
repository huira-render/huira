
#pragma once

#include <concepts>
#include <string>
#include <type_traits>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"
#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Abstract base class for point spread functions (PSF) that can be set on a camera.
 *
 * A PSF set with CameraModel::set_psf() takes the place of the aperture's diffraction pattern.
 * It is given by evaluate(), its value at an offset from the source in pixels, and is taken as
 * complete within radius() pixels of the source: the renderer samples it on a grid out to there
 * (see GridPsf) and normalizes what it finds, so that every source keeps all its light. A
 * MeasuredPSF is instead sampled where its own samples lie.
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
     * A PSF's intensity is integrated over each pixel. One that is already integrated over a
     * pixel (see PSFSampling::PixelIntegrated) is instead taken at each pixel's center, so that
     * it is not integrated twice.
     */
    [[nodiscard]] virtual bool is_pixel_integrated() const { return false; }

    /**
     * @brief Whether the PSF already holds the defocus the camera's focus gives, rather than
     * being a point's in focus, which the focus then blurs as it blurs diffraction (see
     * PSFFocus).
     */
    [[nodiscard]] virtual bool includes_defocus() const { return false; }

    /// The largest radius a PSF can have, in pixels. On its grid (see GridPsf), a PSF is sampled
    /// more coarsely the farther it reaches, to keep within GridPsf::MEMORY_BUDGET: at this
    /// radius, only every several pixels.
    static constexpr int MAX_RADIUS = 4096;

    /// How far from the source, in pixels, the PSF holds light; 0 if not set, in which case
    /// CameraModel::set_psf() gives it CameraModel::DEFAULT_PSF_RADIUS.
    [[nodiscard]] int radius() const { return radius_; }

    /**
     * @brief Set how far from the source, in pixels, the PSF holds light.
     *
     * @throws std::runtime_error if the radius is less than 1 or more than MAX_RADIUS.
     */
    void set_radius(int radius)
    {
        if (radius < 1 || radius > MAX_RADIUS) {
            HUIRA_THROW_ERROR("PSF::set_radius - The radius must be from 1 to " +
                              std::to_string(MAX_RADIUS) + " pixels: " + std::to_string(radius));
        }
        radius_ = radius;
    }

  private:
    int radius_ = 0;
};

template <typename T>
struct is_psf : std::false_type {};

template <template <typename> class Derived, typename TSpectral>
    requires std::derived_from<Derived<TSpectral>, PSF<TSpectral>>
struct is_psf<Derived<TSpectral>> : std::true_type {};

template <typename T>
concept IsPSF = is_psf<T>::value;
} // namespace huira
