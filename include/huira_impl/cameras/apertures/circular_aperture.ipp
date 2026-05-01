
#include <cmath>
#include <memory>

#include "huira/cameras/apertures/aperture.hpp"
#include "huira/cameras/psfs/airy_disk.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/constants.hpp"
#include "huira/units/units.hpp"

namespace huira {

/**
 * @brief Constructs a circular aperture with the given diameter.
 *
 * Initializes the aperture and computes its area from the specified diameter.
 *
 * @param diameter The diameter of the aperture in meters.
 */
template <IsSpectral TSpectral>
CircularAperture<TSpectral>::CircularAperture(units::Meter diameter)
{
    this->set_diameter(diameter);
}

/**
 * @brief Sets the area of the aperture.
 *
 * Directly sets the area value (in square meters) for the aperture.
 *
 * @param area The new area in square meters.
 */
template <IsSpectral TSpectral>
void CircularAperture<TSpectral>::set_area(units::SquareMeter area)
{
    this->area_ = area.to_si_f();
    diameter_ = std::sqrt(4.f * this->area_ / PI<float>());
    radius_ = diameter_ / 2.f;
}

template <IsSpectral TSpectral>
Vec2<float> CircularAperture<TSpectral>::sample(Sampler<float>& sampler) const
{
    Vec2<float> u = sampler.get_2d();

    // Map from [0,1] to [-1,1]
    float a = 2.f * u.x - 1.f;
    float b = 2.f * u.y - 1.f;

    float r, theta;
    if (a * a > b * b) {
        r = a;
        theta = (PI<float>() / 4.f) * (b / a);
    } else if (b != 0.f) {
        r = b;
        theta = (PI<float>() / 2.f) - (PI<float>() / 4.f) * (a / b);
    } else {
        return {0.f, 0.f};
    }

    return {radius_ * r * std::cos(theta), radius_ * r * std::sin(theta)};
}

/**
 * @brief Sets the diameter of the aperture and updates the area.
 *
 * Computes the area from the specified diameter and updates the internal area value.
 *
 * @param diameter The new diameter in meters.
 */
template <IsSpectral TSpectral>
void CircularAperture<TSpectral>::set_diameter(units::Meter diameter)
{
    float d = diameter.to_si_f();
    this->area_ = PI<float>() * (d * d) / 4.f;
    this->diameter_ = d;
    this->radius_ = d / 2.f;
}

/**
 * @brief Returns the diameter of the aperture.
 *
 * Computes the diameter from the current area value.
 *
 * @return The diameter in meters.
 */
template <IsSpectral TSpectral>
units::Meter CircularAperture<TSpectral>::get_diameter() const
{
    return units::Meter(diameter_);
}

/**
 * @brief Creates a point spread function (PSF) for the circular aperture.
 *
 * Constructs an Airy disk PSF using the aperture's diameter and the provided optical parameters.
 *
 * @param focal_length The focal length of the optical system in meters.
 * @param pitch_x The pixel pitch in the x direction in meters.
 * @param pitch_y The pixel pitch in the y direction in meters.
 * @param radius The radius of the PSF kernel in pixels.
 * @param banks The number of spectral banks.
 * @return A unique pointer to the created PSF object.
 */
template <IsSpectral TSpectral>
std::unique_ptr<PSF<TSpectral>> CircularAperture<TSpectral>::make_psf(
    units::Meter focal_length, units::Meter pitch_x, units::Meter pitch_y, int radius, int banks)
{
    units::Meter diameter(diameter_);
    return std::make_unique<AiryDisk<TSpectral>>(
        focal_length, pitch_x, pitch_y, diameter, radius, banks);
}

template <IsSpectral TSpectral>
units::Meter CircularAperture<TSpectral>::get_bounding_radius() const
{
    return units::Meter(radius_);
}

template <IsSpectral TSpectral>
void CircularAperture<TSpectral>::rasterize_kernel_(Image<float>& kernel,
                                                    float radius_pixels,
                                                    float offset_x,
                                                    float offset_y)
{
    const int dim = kernel.width();
    const float cx = static_cast<float>(dim / 2) + 0.5f + offset_x;
    const float cy = static_cast<float>(dim / 2) + 0.5f + offset_y;

    for (int y = 0; y < dim; ++y) {
        for (int x = 0; x < dim; ++x) {
            // Distance from pixel center to disc center
            float dx = (static_cast<float>(x) + 0.5f) - cx;
            float dy = (static_cast<float>(y) + 0.5f) - cy;
            float dist = std::sqrt(dx * dx + dy * dy);

            // Antialiased coverage: fully inside, fully outside,
            // or linear ramp over a 1-pixel transition band at the edge
            float coverage = std::clamp(radius_pixels - dist + 0.5f, 0.f, 1.f);

            kernel(x, y) = coverage;
        }
    }
}
} // namespace huira
