#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/handles/handle.hpp"

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
        void set_distortion(Args&&... args) const;

        void delete_distortion() const;

        template <IsSensor TSensor, typename... Args>
        void set_sensor(Args&&... args) const;

        void set_sensor_resolution(Resolution resolution) const;
        void set_sensor_resolution(int width, int height) const;

        void set_sensor_pixel_pitch(Vec2<float> pixel_pitch) const;
        void set_sensor_pixel_pitch(float pixel_pitch_x, float pixel_pitch_y) const;
        void set_sensor_pixel_pitch(float pixel_pitch) const;

        void set_sensor_size(Vec2<float> size) const;
        void set_sensor_size(float width, float height) const;
        void set_sensor_size(float width) const;

        void set_sensor_quantum_efficiency(TSpectral qe) const;
        void set_sensor_full_well_capacity(float fwc) const;
        void set_sensor_read_noise(float read_noise) const;
        void set_sensor_dark_current(float dark_current) const;
        void set_sensor_bias_level(float bias_level) const;
        void set_sensor_bit_depth(int bit_depth) const;
        void set_sensor_gain(float gain) const;
        void set_sensor_gain_db(float gain_db) const;
        void set_sensor_uinty_db(float unity_db) const;

        void set_sensor_rotation(units::Radian angle) const;

        template <IsAperture TAperture, typename... Args>
        void set_aperture(Args&&... args) const;

        template <IsPSF TPSF, typename... Args>
        void set_psf(Args&&... args) const;

        void use_aperture_psf(bool use_psf = true) const;
        void delete_psf() const;
        
        Pixel project_point(const Vec3<float>& point_camera_coords) const;

        FrameBuffer<TSpectral> make_frame_buffer() const;

        void use_blender_convention(bool value = true) const;

        friend class Scene<TSpectral>;
        friend class SceneView<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };
}

#include "huira_impl/handles/camera_handle.ipp"
