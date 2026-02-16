#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/cameras/psf/psf.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class AiryDisk : public PSF<TSpectral> {
    public:
        AiryDisk(units::Meter focal_length, units::Meter pitch_x, units::Meter pitch_y, units::Meter aperture_diameter, int radius, int banks);
        ~AiryDisk() override = default;

        TSpectral evaluate(float x, float y) override;

    private:
        double f_number_;
        Vec2<float> pixel_pitch_;

        static double bessel_j1(double x);
    };
}

#include "huira_impl/cameras/psf/airy_disk.ipp"
