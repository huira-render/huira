#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
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

        void set_focal_length(float focal_length) const;
        void set_fstop(float fstop) const;

        void disable_psf() const { this->get()->disable_psf(); }
        void use_aperture_psf(bool use_psf = true) const { this->get()->use_aperture_psf(use_psf); }
        void set_custom_psf(std::unique_ptr<PSF<TSpectral>> psf) const { this->get()->set_custom_psf(std::move(psf)); }

        FrameBuffer<TSpectral> make_frame_buffer() const;

        friend class Scene<TSpectral>;
        friend class SceneView<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };
}

#include "huira_impl/handles/camera_handle.ipp"
