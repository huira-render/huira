#pragma once

#include "huira/core/types.hpp"

namespace huira {
    class Aperture {
    public:
        virtual ~Aperture() = default;

        virtual float get_area() const = 0;
        virtual void set_area(float area) = 0;
        
    };
}
