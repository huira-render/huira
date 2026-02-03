#pragma once

#include "huira/core/constants.hpp"
#include "huira/cameras/aperture/aperture.hpp"

namespace huira {
    class CircularAperture : public Aperture {
    public:
        CircularAperture(float diameter)
        {
            this->set_diameter(diameter);
        }

        ~CircularAperture() override = default;

        float get_area() const override { return area_; }
        void set_area(float area) override { area_ = area; }

        void set_diameter(float diameter) {
            area_ = PI<float>() * (diameter * diameter) / 4.f;
        }

    private:
        float area_ = 1.f;
    };
}
