
#pragma once

#include <memory>

#include "huira/cameras/psfs/psf.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/units/units.hpp"

namespace huira {

    /**
     * @brief Abstract base class for optical apertures.
     *
     * Defines the interface for all aperture types, including area management and PSF creation.
     *
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class Aperture {
    public:
        virtual ~Aperture() = default;

        virtual float get_area() const = 0;
        virtual void set_area(units::SquareMeter area) = 0;

        virtual std::unique_ptr<PSF<TSpectral>> make_psf(units::Meter focal_length, units::Meter pitch_x, units::Meter pitch_y, int radius, int banks) = 0;
    };

    template <typename T>
    struct is_aperture : std::false_type {};

    template <template <typename> class Derived, typename TSpectral>
        requires std::derived_from<Derived<TSpectral>, Aperture<TSpectral>>
    struct is_aperture<Derived<TSpectral>> : std::true_type {};

    template <typename T>
    concept IsAperture = is_aperture<T>::value;
}
