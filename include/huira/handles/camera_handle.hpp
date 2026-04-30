
#pragma once

#include <optional>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/units/units.hpp"

#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"
#include "huira/handles/handle.hpp"


namespace huira {
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class SceneView;

    template <IsSpectral TSpectral>
    class FrameHandle;


    /**
     * @brief Handle for manipulating a CameraModel in a scene.
     *
     * Provides a safe, reference-like interface for configuring and querying a CameraModel instance
     * within a scene graph. All operations are forwarded to the underlying CameraModel.
     *
     * @tparam TSpectral The spectral type (e.g., float, Vec3f, etc.)
     */
    template <IsSpectral TSpectral>
    class CameraModelHandle : public Handle<CameraModel<TSpectral>> {
    public:
        CameraModelHandle() = delete;
        using Handle<CameraModel<TSpectral>>::Handle;

        void set_focal_length(units::Millimeter focal_length) const;
        units::Millimeter focal_length() const;

        void set_fstop(float fstop) const;
        float fstop() const;

        template <IsDistortion<TSpectral> TDistortion, typename... Args>
        void set_distortion(Args&&... args) const;

        void set_brown_conrady_distortion(BrownCoefficients coeffs) const;
        void set_opencv_distortion(OpenCVCoefficients coeffs) const;
        void set_owen_distortion(OwenCoefficients coeffs) const;

        void delete_distortion() const;

        template <IsSensor<TSpectral> TSensor, typename... Args>
        void set_sensor(Args&&... args) const;
        
        void configure_sensor_from_pitch(const Resolution& resolution,
            units::Micrometer pitch_x, std::optional<units::Micrometer> pitch_y = std::nullopt,
            std::optional<float> cx = std::nullopt,
            std::optional<float> cy = std::nullopt);

        void configure_sensor_from_size(const Resolution& resolution,
            units::Millimeter width, std::optional<units::Millimeter> height = std::nullopt,
            std::optional<float> cx = std::nullopt,
            std::optional<float> cy = std::nullopt);

        void set_intrinsic_matrix(const Mat3<float>& intrinsic_matrix, const Resolution& resolution,
            units::Millimeter anchor_focal_length);
        void set_intrinsics(float fx, float fy, float cx, float cy, const Resolution& resolution,
            units::Millimeter anchor_focal_length);

        void set_sensor_quantum_efficiency(double qe) const;
        void set_sensor_quantum_efficiency(TSpectral qe) const;
        void set_sensor_full_well_capacity(float fwc) const;
        void set_sensor_simulate_noise(bool simulate_noise) const;
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

        void use_aperture_psf(bool value) const;
        void use_aperture_psf(int radius = 64, int banks = 16) const;
        void enable_psf_convolution(bool convolve_psf = true) const;
        void delete_psf() const;

        void set_veiling_glare(float alpha) const;
        void disable_veiling_glare() const;
        void set_harvey_shack_scatter(float scatter_fraction, float falloff_exponent, float r0 = 0.5f, float radius = 0.f) const;
        void disable_harvey_shack_scatter() const;

        void enable_depth_of_field(bool depth_of_field = true) const;
        void set_focus_distance(units::Meter focus_distance) const;
        units::Meter get_focus_distance() const;
        void set_diopters(units::Diopter diopters) const;
        units::Diopter get_diopters() const;

        Pixel project_point(const Vec3<float>& point_camera_coords) const;

        FrameBuffer<TSpectral> make_frame_buffer() const;

        void use_blender_convention(bool value = true) const;

        friend class Scene<TSpectral>;
        friend class SceneView<TSpectral>;
        friend class FrameHandle<TSpectral>;


        // DEPRECATED
        [[deprecated("set_sensor_resolution was removed in v0.9.4.  Use configure_sensor_from_pitch() or configure_sensor_from_size() instead.")]]
        void set_sensor_resolution(Resolution resolution) const;

        [[deprecated("set_sensor_resolution was removed in v0.9.4.  Use configure_sensor_from_pitch() or configure_sensor_from_size() instead.")]]
        void set_sensor_resolution(int width, int height) const;

        [[deprecated("set_sensor_pixel_pitch was removed in v0.9.4.  Use configure_sensor_from_pitch() instead.")]]
        void set_sensor_pixel_pitch(units::Millimeter pitch_x, units::Millimeter pitch_y) const;

        [[deprecated("set_sensor_pixel_pitch was removed in v0.9.4.  Use configure_sensor_from_pitch() instead.")]]
        void set_sensor_pixel_pitch(units::Millimeter pitch) const;

        [[deprecated("set_sensor_size was removed in v0.9.4.  Use configure_sensor_from_size() instead.")]]
        void set_sensor_size(units::Millimeter width, units::Millimeter height) const;

        [[deprecated("set_sensor_size was removed in v0.9.4.  Use configure_sensor_from_size() instead.")]]
        void set_sensor_size(units::Millimeter width) const;
    };
}

#include "huira_impl/handles/camera_handle.ipp"
