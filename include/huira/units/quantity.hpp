#pragma once

#include <cmath>
#include <ratio>

#include "huira/units/dimensionality.hpp"

namespace huira {

    template<IsDimensionality Dim, typename Scale>
    class Quantity {
    public:
        using dimension_type = Dim;
        using scale_type = Scale;

        constexpr explicit Unit(double value)
            : value_(value)
        {
            scale_ = static_cast<double>(Scale::num) / static_cast<double>(Scale::den);
        };

        double SIValue() { return scale_ * value_; }

    private:
        double value_;
        double scale_;
    };
}