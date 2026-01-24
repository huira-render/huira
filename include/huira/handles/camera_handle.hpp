#pragma once

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/cameras/camera.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class CameraHandle : public NodeHandle<TSpectral, Camera<TSpectral>> {
    public:
        CameraHandle() = delete;
        using NodeHandle<TSpectral, Camera<TSpectral>>::NodeHandle;

        void set_focal_length(double focal_length) const;

    };
}

#include "huira_impl/handles/camera_handle.ipp"
