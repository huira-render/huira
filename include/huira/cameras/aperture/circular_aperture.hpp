#pragma once

#include <memory>
#include <cmath>

#include "huira/core/constants.hpp"
#include "huira/cameras/aperture/aperture.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/cameras/psf/airy_disk.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class CircularAperture : public Aperture<TSpectral> {
    public:
        CircularAperture(float diameter)
        {
            this->set_diameter(diameter);
        }

        ~CircularAperture() override = default;

        float get_area() const override { return area_; }
        void set_area(float area) override { area_ = area; }

        void set_diameter(float diameter) { area_ = PI<float>() * (diameter * diameter) / 4.f; }

        float get_diameter() const { return std::sqrt(4.f * area_ / PI<float>()); }

        std::unique_ptr<PSF<TSpectral>> make_psf(float focal_length, Vec2<float> pixel_pitch, int radius, int banks) override
        {
            return std::make_unique<AiryDisk<TSpectral>>(focal_length, pixel_pitch, get_diameter(), radius, banks);
        }

    private:
        float area_ = 1.f;
    };
}
