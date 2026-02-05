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

        virtual std::unique_ptr<PSF<TSpectral>> make_psf(float focal_length, Vec2<float> pixel_pitch) = 0;
        
    };
}
