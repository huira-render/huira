#pragma once

#include <memory>

#include "huira/cameras/apertures/aperture.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/units/units.hpp"

namespace huira {

    /**
     * @brief Circular optical aperture.
     *
     * Models a circular aperture with a specified diameter and area, supporting PSF creation.
     *
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class CircularAperture : public Aperture<TSpectral> {
    public:
        CircularAperture(units::Meter diameter);

        ~CircularAperture() override = default;

        void set_area(units::SquareMeter area) override;
        float get_area() const override { return area_; }

        void set_diameter(units::Meter diameter);
        float get_diameter() const;

        std::unique_ptr<PSF<TSpectral>> make_psf(units::Meter focal_length, units::Meter pitch_x, units::Meter pitch_y, int radius, int banks) override;

    private:
        float area_ = 1.f;
    };
}

#include "huira_impl/cameras/apertures/circular_aperture.ipp"
