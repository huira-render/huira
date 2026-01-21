#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/handles/point_handle.hpp"
#include "huira/lights/point_light.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class PointLightHandle : public PointHandle<TSpectral, TFloat, PointLight<TSpectral, TFloat>> {
    public:
        PointLightHandle() = delete;
        using PointHandle<TSpectral, TFloat, PointLight<TSpectral, TFloat>>::PointHandle;

        void set_intensity(const TSpectral& intensity) const { this->get()->set_intensity(intensity); }
    };
}
