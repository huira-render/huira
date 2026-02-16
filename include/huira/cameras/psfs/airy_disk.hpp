
#pragma once

#include "huira/cameras/psfs/psf.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

    /**
     * @brief Airy disk point spread function (PSF).
     *
     * Models the PSF of a circular aperture using the Airy disk formula for diffraction-limited optics.
     *
     * @tparam TSpectral The spectral representation type.
     */
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

#include "huira_impl/cameras/psfs/airy_disk.ipp"
