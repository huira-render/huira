
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/images/image.hpp"
#include "huira/units/units.hpp"
#include "huira/util/removed_api.hpp"

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

    void set_aperture_diameter(units::Millimeter diameter) const;
    units::Millimeter aperture_diameter() const;

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
                        units::Millimeter anchor_focal_length,
                        float skew = 0.f);

    Mat3<float> intrinsic_matrix() const;
    Resolution resolution() const;
    std::pair<units::Micrometer, units::Micrometer> pixel_pitch() const;

    void set_sensor_quantum_efficiency(double qe) const;
    void set_sensor_quantum_efficiency(TSpectral qe) const;
    void set_sensor_full_well_capacity(float fwc) const;
    void enable_sensor_noise(bool noise = true) const;
    void set_sensor_read_noise(float read_noise) const;
    void set_sensor_dark_current(float dark_current) const;
    void set_sensor_bias_level(float bias_level) const;
    void set_sensor_bit_depth(int bit_depth) const;
    void set_sensor_conversion_gain(float gain) const;
    void set_sensor_gain_db(float gain_db) const;
    void set_sensor_unity_db(float unity_db) const;

    void set_sensor_roll(units::Radian angle) const;

    void set_sensor_noise_seed(std::uint64_t seed) const;
    std::uint64_t sensor_noise_seed() const;

    TSpectral sensor_quantum_efficiency() const;
    float sensor_full_well_capacity() const;
    bool sensor_noise_enabled() const;
    float sensor_read_noise() const;
    float sensor_dark_current() const;
    float sensor_bias_level() const;
    int sensor_bit_depth() const;
    float sensor_conversion_gain() const;
    float sensor_gain_db() const;
    float sensor_unity_db() const;
    units::Radian sensor_roll() const;
    Rotation<double> sensor_orientation() const;

    template <IsAperture TAperture, typename... Args>
    void set_aperture(Args&&... args) const;

    template <IsPSF TPSF, typename... Args>
    void set_psf(Args&&... args) const;

    void set_measured_psf(const Image<TSpectral>& data,
                          float samples_per_pixel,
                          PSFSampling sampling,
                          int radius = 0,
                          int banks = CameraModel<TSpectral>::DEFAULT_PSF_BANKS) const;

    void use_aperture_psf(bool value) const;
    void use_aperture_psf(int radius = 0,
                          int banks = CameraModel<TSpectral>::DEFAULT_PSF_BANKS) const;
    bool uses_aperture_psf() const;
    bool has_psf() const;
    void enable_psf_convolution(bool convolve_psf = true) const;
    bool psf_convolution_enabled() const;
    void set_psf_convolution_radius(int radius) const;
    void delete_psf() const;

    void set_veiling_glare(float alpha) const;
    void disable_veiling_glare() const;
    void set_harvey_shack_scatter(float scatter_fraction,
                                  float falloff_exponent,
                                  float r0 = 0.5f,
                                  float radius = 0.f) const;
    void disable_harvey_shack_scatter() const;
    void set_scatter(float fraction,
                     float slope,
                     units::Radian shoulder_angle,
                     std::optional<units::Radian> outer_angle = std::nullopt) const;

    int get_psf_radius() const;
    const Image<TSpectral>& get_psf_kernel(float u, float v) const;
    const Image<TSpectral>& get_psf_convolution_kernel() const;
    const Image<TSpectral>& get_psf_wings_kernel() const;
    Image<TSpectral> psf_image(int radius,
                               float x_offset = 0.f,
                               float y_offset = 0.f,
                               std::optional<units::Meter> range = std::nullopt) const;

    void precompute() const;
    bool is_precomputed() const;
    void enable_auto_precompute(bool auto_precompute = true) const;
    bool auto_precompute_enabled() const;

    void enable_depth_of_field(bool depth_of_field = true) const;
    bool depth_of_field_enabled() const;

    void set_focus_distance(units::Meter focus_distance) const;
    void set_focus_diopters(units::Diopter diopters) const;
    void set_focus_sensor_offset(units::Micrometer offset) const;

    units::Meter focus_distance() const;
    units::Diopter focus_diopters() const;
    units::Micrometer focus_sensor_offset() const;

    float defocus_blur_radius() const;

    void set_pixel_convention(PixelConvention convention) const;
    PixelConvention pixel_convention() const;

    Pixel project_point(const Vec3<float>& point_camera_coords) const;
    Pixel try_project_point(const Vec3<float>& point_camera_coords) const;
    bool in_fov(const Vec3<float>& point_camera_coords) const;

    Ray<TSpectral> cast_ray(const Pixel& pixel) const;
    Ray<TSpectral> cast_ray(const Pixel& pixel, Sampler<float>& sampler) const;
    Ray<TSpectral> cast_ray(int x, int y) const;
    std::optional<Ray<TSpectral>> try_cast_ray(const Pixel& pixel) const;

    FrameBuffer<TSpectral> make_frame_buffer() const;

    void use_blender_convention(bool value = true) const;
    bool uses_blender_convention() const;

    std::string describe() const;

    friend class Scene<TSpectral>;
    friend class SceneView<TSpectral>;
    friend class FrameHandle<TSpectral>;

    // Removed: calling these fails to compile, with what to use instead.
    template <typename... Args>
    void set_diopters(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "set_diopters() was removed in v0.9.10. Use set_focus_diopters() instead.");
    }

    template <typename... Args>
    units::Diopter get_diopters(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "get_diopters() was removed in v0.9.10. Use focus_diopters() instead.");
        return {};
    }

    template <typename... Args>
    units::Meter get_focus_distance(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "get_focus_distance() was removed in v0.9.10. Use focus_distance() instead.");
        return {};
    }

    template <typename... Args>
    void set_sensor_rotation(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "set_sensor_rotation() was removed in v0.9.10. Use set_sensor_roll() "
                      "instead.");
    }

    template <typename... Args>
    void set_sensor_simulate_noise(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "set_sensor_simulate_noise() was removed in v0.9.10. Use "
                      "enable_sensor_noise() instead.");
    }

    template <typename... Args>
    void set_sensor_resolution(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "set_sensor_resolution() was removed in v0.9.4. Use "
                      "configure_sensor_from_pitch() or configure_sensor_from_size() instead.");
    }

    template <typename... Args>
    void set_sensor_pixel_pitch(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "set_sensor_pixel_pitch() was removed in v0.9.4. Use "
                      "configure_sensor_from_pitch() instead.");
    }

    template <typename... Args>
    void set_sensor_size(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "set_sensor_size() was removed in v0.9.4. Use configure_sensor_from_size() "
                      "instead.");
    }
};
} // namespace huira

#include "huira_impl/handles/camera_handle.ipp"
