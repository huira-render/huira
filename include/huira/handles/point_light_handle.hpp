#pragma once

#include "huira/assets/lights/point_light.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/point_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class PointLightHandle : public PointHandle<TSpectral, PointLight<TSpectral>> {
    public:
        PointLightHandle() = delete;
        using PointHandle<TSpectral, PointLight<TSpectral>>::PointHandle;

        void set_intensity(const TSpectral& intensity) const { this->get()->set_intensity(intensity); }
    };
}
