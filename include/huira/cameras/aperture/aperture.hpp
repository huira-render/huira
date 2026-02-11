#pragma once

#include <memory>

#include "huira/core/types.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/cameras/psf/psf.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Aperture {
    public:
        virtual ~Aperture() = default;

        virtual float get_area() const = 0;
        virtual void set_area(float area) = 0;

        virtual std::unique_ptr<PSF<TSpectral>> make_psf(float focal_length, Vec2<float> pixel_pitch, int radius, int banks) = 0;
        
    };

    template <typename T>
    struct is_aperture : std::false_type {};

    template <template <typename> class Derived, typename TSpectral>
        requires std::derived_from<Derived<TSpectral>, Aperture<TSpectral>>
    struct is_aperture<Derived<TSpectral>> : std::true_type {};

    template <typename T>
    concept IsAperture = is_aperture<T>::value;
}
