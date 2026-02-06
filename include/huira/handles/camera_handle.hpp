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
        float focal_length() const;

        void set_fstop(float fstop) const;
        float fstop() const;

        template <IsDistortion TDistortion, typename... Args>
        void set_distortion(Args&&... args);

        void delete_distortion();

        template <IsSensor TSensor, typename... Args>
        void set_sensor(Args&&... args);

        template <IsAperture TAperture, typename... Args>
        void set_aperture(Args&&... args);

        template <IsPSF TPSF, typename... Args>
        void set_psf(Args&&... args);

        void use_aperture_psf(bool use_psf = true) const;
        void delete_psf() const;
        
        Pixel project_point(const Vec3<float>& point_camera_coords) const;

        FrameBuffer<TSpectral> make_frame_buffer() const;

        friend class Scene<TSpectral>;
        friend class SceneView<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };
}

#include "huira_impl/handles/camera_handle.ipp"
