
#pragma once

#include <optional>

#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"
#include "huira/cameras/optics.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/images/image.hpp"
#include "huira/units/units.hpp"

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
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
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
                                     units::Micrometer pitch_x,
                                     std::optional<units::Micrometer> pitch_y = std::nullopt,
                                     std::optional<float> cx = std::nullopt,
                                     std::optional<float> cy = std::nullopt);

    void configure_sensor_from_size(const Resolution& resolution,
                                    units::Millimeter width,
                                    std::optional<units::Millimeter> height = std::nullopt,
                                    std::optional<float> cx = std::nullopt,
                                    std::optional<float> cy = std::nullopt);

    void set_intrinsic_matrix(const Mat3<float>& intrinsic_matrix,
                              const Resolution& resolution,
                              units::Millimeter anchor_focal_length);
    void set_intrinsics(float fx,
                        float fy,
                        float cx,
                        float cy,
                        const Resolution& resolution,
                        units::Millimeter anchor_focal_length);

    void set_sensor_quantum_efficiency(double qe) const;
    void set_sensor_quantum_efficiency(TSpectral qe) const;
    void set_sensor_full_well_capacity(float fwc) const;
    void set_sensor_simulate_noise(bool simulate_noise) const;
    void set_sensor_read_noise(float read_noise) const;
    void set_sensor_dark_current(float dark_current) const;
    void set_sensor_bias_level(float bias_level) const;
    void set_sensor_bit_depth(int bit_depth) const;
    void set_sensor_conversion_gain(float gain) const;
    void set_sensor_gain_db(float gain_db) const;
    void set_sensor_unity_db(float unity_db) const;

    void set_sensor_rotation(units::Radian angle) const;

    template <IsAperture TAperture, typename... Args>
    void set_aperture(Args&&... args) const;

    void set_optics(Optics<TSpectral> optics) const;
    const Optics<TSpectral>& optics() const;

    void set_core(typename Optics<TSpectral>::Core core) const;
    void set_scatter(HarveyShack scatter) const;
    void set_veiling_glare(float fraction) const;
    void clear_scatter() const;
    void set_ideal_optics() const;

    bool has_core_psf() const;
    bool has_scatter() const;
    float scatter_fraction() const;
    float veiling_glare() const;

    int get_psf_radius() const;
    const Image<TSpectral>& psf_convolution_kernel() const;
    const Image<TSpectral>& psf_wings_kernel() const;
    void build_optics() const;

    void set_focus(units::Meter distance) const;
    void set_focus(units::Diopter vergence) const;
    units::Meter focus_distance() const;
    units::Diopter focus_vergence() const;
    float defocus_blur_pixels() const;
    bool has_defocus() const;

    Pixel project_point(const Vec3<float>& point_camera_coords) const;

    FrameBuffer<TSpectral> make_frame_buffer() const;

    void use_blender_convention(bool value = true) const;

    friend class Scene<TSpectral>;
    friend class SceneView<TSpectral>;
    friend class FrameHandle<TSpectral>;

    // DEPRECATED
    [[deprecated("use_aperture_psf was removed in v0.10.0.  Set an Optics with a "
                 "DiffractionCore instead, which is now the default.")]]
    void use_aperture_psf(bool value) const;

    [[deprecated("use_aperture_psf was removed in v0.10.0.  Set an Optics with a "
                 "DiffractionCore instead, which is now the default.")]]
    void use_aperture_psf(int radius = 64, int banks = 16) const;

    [[deprecated("enable_psf_convolution was removed in v0.10.0.  The optics are applied to "
                 "the whole image by default; use Renderer::set_psf_application() to skip it.")]]
    void enable_psf_convolution(bool convolve_psf = true) const;

    [[deprecated("set_psf_convolution_radius was removed in v0.10.0.  Set "
                 "HarveyShack::captured_energy, or HarveyShack::kernel_radius for an explicit "
                 "radius.")]]
    void set_psf_convolution_radius(int radius) const;

    [[deprecated("delete_psf was removed in v0.10.0.  Use set_ideal_optics(), or set an "
                 "Optics with an IdealCore.")]]
    void delete_psf() const;

    [[deprecated("set_measured_psf was removed in v0.10.0.  Use set_core() with a "
                 "MeasuredCore instead.")]]
    void set_measured_psf(const Image<TSpectral>& data,
                          float samples_per_pixel,
                          int radius = 0,
                          int banks = 16) const;

    [[deprecated("set_harvey_shack_scatter was removed in v0.10.0.  Use set_scatter() with a "
                 "HarveyShack instead.")]]
    void set_harvey_shack_scatter(float scatter_fraction,
                                  float falloff_exponent,
                                  float r0 = 0.5f,
                                  float radius = 0.f) const;

    [[deprecated("disable_harvey_shack_scatter was removed in v0.10.0.  Use clear_scatter() "
                 "instead.")]]
    void disable_harvey_shack_scatter() const;

    [[deprecated("disable_veiling_glare was removed in v0.10.0.  Use set_veiling_glare(0.f) "
                 "instead.")]]
    void disable_veiling_glare() const;

    [[deprecated("enable_depth_of_field was removed in v0.10.0.  Aperture sampling now "
                 "follows the focus setting; use Renderer::set_aperture_sampling() to "
                 "override it.")]]
    void enable_depth_of_field(bool depth_of_field = true) const;

    [[deprecated("set_focus_distance was removed in v0.10.0.  Use set_focus() instead.")]]
    void set_focus_distance(units::Meter focus_distance) const;

    [[deprecated("get_focus_distance was removed in v0.10.0.  Use focus_distance() instead.")]]
    units::Meter get_focus_distance() const;

    [[deprecated("set_diopters was removed in v0.10.0.  Use set_focus() with a Diopter "
                 "argument instead.")]]
    void set_diopters(units::Diopter diopters) const;

    [[deprecated("get_diopters was removed in v0.10.0.  Use focus_vergence() instead.")]]
    units::Diopter get_diopters() const;

    [[deprecated("set_sensor_resolution was removed in v0.9.4.  Use configure_sensor_from_pitch() "
                 "or configure_sensor_from_size() instead.")]]
    void set_sensor_resolution(Resolution resolution) const;

    [[deprecated("set_sensor_resolution was removed in v0.9.4.  Use configure_sensor_from_pitch() "
                 "or configure_sensor_from_size() instead.")]]
    void set_sensor_resolution(int width, int height) const;

    [[deprecated("set_sensor_pixel_pitch was removed in v0.9.4.  Use configure_sensor_from_pitch() "
                 "instead.")]]
    void set_sensor_pixel_pitch(units::Millimeter pitch_x, units::Millimeter pitch_y) const;

    [[deprecated("set_sensor_pixel_pitch was removed in v0.9.4.  Use configure_sensor_from_pitch() "
                 "instead.")]]
    void set_sensor_pixel_pitch(units::Millimeter pitch) const;

    [[deprecated(
        "set_sensor_size was removed in v0.9.4.  Use configure_sensor_from_size() instead.")]]
    void set_sensor_size(units::Millimeter width, units::Millimeter height) const;

    [[deprecated(
        "set_sensor_size was removed in v0.9.4.  Use configure_sensor_from_size() instead.")]]
    void set_sensor_size(units::Millimeter width) const;
};
} // namespace huira

#include "huira_impl/handles/camera_handle.ipp"
