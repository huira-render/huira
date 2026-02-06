#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_focal_length(float focal_length) const {
        this->get()->set_focal_length(focal_length);
    }

    template <IsSpectral TSpectral>
    float CameraModelHandle<TSpectral>::focal_length() const
    {
        return this->get()->focal_length();
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_fstop(float fstop) const
    {
        this->get()->set_fstop(fstop);
    }

    template <IsSpectral TSpectral>
    float CameraModelHandle<TSpectral>::fstop() const
    {
        return this->get()->fstop();
    }


    template <IsSpectral TSpectral>
    template <IsDistortion TDistortion, typename... Args>
    void CameraModelHandle<TSpectral>::set_distortion(Args&&... args)
    {
        this->get()->template set_distortion<TDistortion>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::delete_distortion()
    {
        this->get()->delete_distortion();
    }

    template <IsSpectral TSpectral>
    template <IsSensor TSensor, typename... Args>
    void CameraModelHandle<TSpectral>::set_sensor(Args&&... args)
    {
        this->get()->template set_sensor<TSensor>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral>
    template <IsAperture TAperture, typename... Args>
    void CameraModelHandle<TSpectral>::set_aperture(Args&&... args)
    {
        this->get()->template set_aperture<TAperture>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral>
    template <IsPSF TPSF, typename... Args>
    void CameraModelHandle<TSpectral>::set_psf(Args&&... args)
    {
        this->get()->template set_psf<TPSF>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::use_aperture_psf(bool use_psf) const
    {
        this->get()->use_aperture_psf(use_psf);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::delete_psf() const
    {
        this->get()->delete_psf();
    }

    template <IsSpectral TSpectral>
    Pixel CameraModelHandle<TSpectral>::project_point(const Vec3<float>& point_camera_coords) const
    {
        return this->get()->project_point(point_camera_coords);
    }


    template <IsSpectral TSpectral>
    FrameBuffer<TSpectral> CameraModelHandle<TSpectral>::make_frame_buffer() const {
        return this->get()->make_frame_buffer();
    }
}
