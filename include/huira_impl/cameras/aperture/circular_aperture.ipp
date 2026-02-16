
#include <cmath>
#include <memory>

#include "huira/cameras/aperture/aperture.hpp"
#include "huira/cameras/psf/airy_disk.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/units/units.hpp"

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
        this->area_ = static_cast<float>(area.to_si());
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
        float d = static_cast<float>(diameter.to_si());
        this->area_ = PI<float>() * (d * d) / 4.f; 
    }


    /**
     * @brief Returns the diameter of the aperture.
     *
     * Computes the diameter from the current area value.
     *
     * @return The diameter in meters.
     */
    template <IsSpectral TSpectral>
    float CircularAperture<TSpectral>::get_diameter() const 
    {
        return std::sqrt(4.f * this->area_ / PI<float>());
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
    std::unique_ptr<PSF<TSpectral>> CircularAperture<TSpectral>::make_psf(units::Meter focal_length, units::Meter pitch_x, units::Meter pitch_y, int radius, int banks)
    {
        units::Meter diameter(get_diameter());
        return std::make_unique<AiryDisk<TSpectral>>(focal_length, pitch_x, pitch_y, diameter, radius, banks);
    }
}
