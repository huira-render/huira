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
    void CameraModelHandle<TSpectral>::set_distortion(Args&&... args) const
    {
        this->get()->template set_distortion<TDistortion>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::delete_distortion() const
    {
        this->get()->delete_distortion();
    }

    template <IsSpectral TSpectral>
    template <IsSensor TSensor, typename... Args>
    void CameraModelHandle<TSpectral>::set_sensor(Args&&... args) const
    {
        this->get()->template set_sensor<TSensor>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_resolution(Resolution resolution) const
    {
        this->get()->set_sensor_resolution(resolution);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_resolution(int width, int height) const
    {
        this->get()->set_sensor_resolution(Resolution{ width, height });
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_pixel_pitch(Vec2<float> pixel_pitch) const
    {
        this->get()->set_sensor_pixel_pitch(pixel_pitch);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_pixel_pitch(float pixel_pitch) const
    {
        this->get()->set_sensor_pixel_pitch(Vec2<float>{ pixel_pitch, pixel_pitch });
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_pixel_pitch(float pixel_pitch_x, float pixel_pitch_y) const
    {
        this->get()->set_sensor_pixel_pitch(Vec2<float>{ pixel_pitch_x, pixel_pitch_y });
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_size(Vec2<float> size) const
    {
        this->get()->set_sensor_size(size);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_size(float width, float height) const
    {
        this->get()->set_sensor_size(Vec2<float>{ width, height });
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_size(float width) const
    {
        this->get()->set_sensor_size(width);
    }


    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_quantum_efficiency(TSpectral qe) const
    {
        this->get()->sensor_->set_quantum_efficiency(qe);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_full_well_capacity(float fwc) const
    {
        this->get()->sensor_->set_full_well_capacity(fwc);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_read_noise(float read_noise) const
    {
        this->get()->sensor_->set_read_noise(read_noise);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_dark_current(float dark_current) const
    {
        this->get()->sensor_->set_dark_current(dark_current);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_bias_level(float bias_level) const
    {
        this->get()->sensor_->set_bias_level_dn(bias_level);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_bit_depth(int bit_depth) const
    {
        this->get()->sensor_->set_bit_depth(bit_depth);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_gain(float gain) const
    {
        this->get()->sensor_->set_gain_adu(gain);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_gain_db(float gain_db) const
    {
        this->get()->sensor_->set_gain_db(gain_db);
    }

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_uinty_db(float unity_db) const
    {
        this->get()->sensor_->set_unity_db(unity_db);
    }

    template <IsSpectral TSpectral>
    template <IsAperture TAperture, typename... Args>
    void CameraModelHandle<TSpectral>::set_aperture(Args&&... args) const
    {
        this->get()->template set_aperture<TAperture>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral>
    template <IsPSF TPSF, typename... Args>
    void CameraModelHandle<TSpectral>::set_psf(Args&&... args) const
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

    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::use_blender_convention(bool value) const
    {
        this->get()->use_blender_convention(value);
    }
}
