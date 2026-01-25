#pragma once

#include "huira/assets/lights/point_light.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"

namespace huira {
    // Forward Declarations
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class FrameHandle;

    template <IsSpectral TSpectral>
    class PointLightHandle : public Handle<PointLight<TSpectral>> {
    public:
        PointLightHandle() = delete;
        using Handle<PointLight<TSpectral>>::Handle;

        void set_intensity(const TSpectral& intensity) const { this->get()->set_intensity(intensity); }

        friend class Scene<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };
}
