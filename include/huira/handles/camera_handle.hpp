#pragma once

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/cameras/camera_model.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class SceneView;

    template <IsSpectral TSpectral>
    class FrameHandle;


    template <IsSpectral TSpectral>
    class CameraModelHandle : public Handle<CameraModel<TSpectral>> {
    public:
        CameraModelHandle() = delete;
        using Handle<CameraModel<TSpectral>>::Handle;

        void set_focal_length(double focal_length) const;

        friend class Scene<TSpectral>;
        friend class SceneView<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };
}

#include "huira_impl/handles/camera_handle.ipp"
