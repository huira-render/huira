#pragma once

#include <memory>

#include "huira/cameras/psfs/psf.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/images/image.hpp"
#include "huira/sampling/sampler.hpp"
#include "huira/units/units.hpp"

namespace huira {

/**
 * @brief Abstract base class for optical apertures.
 *
 * Defines the interface for all aperture types: area, sampling (for depth of field), the
 * diffraction PSF it produces, and its shape (for defocus blur).
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class Aperture {
  public:
    virtual ~Aperture() = default;

    virtual units::SquareMeter get_area() const = 0;
    virtual void set_area(units::SquareMeter area) = 0;

    virtual Vec2<float> sample(Sampler<float>& sample) const = 0;

    virtual std::unique_ptr<PSF<TSpectral>> make_psf(units::Meter focal_length,
                                                     units::Meter pitch_x,
                                                     units::Meter pitch_y,
                                                     int radius,
                                                     int banks) = 0;

    virtual units::Meter get_bounding_radius() const = 0;

    /**
     * @brief Rasterize the aperture's shape as antialiased pixel coverage.
     *
     * The defocus blur of a point source is the aperture's shape, scaled. The shape is drawn
     * with its bounding radius scaled to radius_pixels, centered offset_x, offset_y pixels
     * beyond the center of the kernel's center pixel. The kernel is not cleared or normalized.
     *
     * @param kernel Square kernel to draw into, of odd size.
     * @param radius_pixels Bounding radius of the drawn shape, in pixels.
     * @param offset_x Horizontal offset of the shape's center from the center pixel, in pixels.
     * @param offset_y Vertical offset of the shape's center from the center pixel, in pixels.
     */
    virtual void rasterize_shape(Image<float>& kernel,
                                 float radius_pixels,
                                 float offset_x,
                                 float offset_y) const = 0;
};

template <typename T>
struct is_aperture : std::false_type {};

template <template <typename> class Derived, typename TSpectral>
    requires std::derived_from<Derived<TSpectral>, Aperture<TSpectral>>
struct is_aperture<Derived<TSpectral>> : std::true_type {};

template <typename T>
concept IsAperture = is_aperture<T>::value;
} // namespace huira
