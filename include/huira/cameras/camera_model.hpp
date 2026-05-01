#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "huira/cameras/apertures/aperture.hpp"
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"
#include "huira/cameras/psfs/psf.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"
#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/geometry/ray.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/render/frustum.hpp"
#include "huira/render/sampler.hpp"
#include "huira/scene/node.hpp"
#include "huira/scene/scene_object.hpp"

namespace huira {
template <IsSpectral TSpectral>
class CameraModelHandle;

template <IsSpectral TSpectral>
class Renderer;

/**
 * @brief CameraModel represents a pinhole or thin-lens camera with configurable sensor, aperture,
 * and distortion models.
 *
 * This class provides a flexible camera abstraction for rendering and simulation, supporting
 * various sensor types, aperture shapes, and lens distortion models. It allows configuration of
 * focal length, f-stop, sensor resolution, pixel pitch, and more. The camera can project 3D points
 * to the image plane, compute projected aperture area, and supports both analytic and PSF-based
 * point spread functions. All units are SI unless otherwise noted.
 *
 * @tparam TSpectral The spectral type (e.g., float, Vec3f, etc.)
 */
template <IsSpectral TSpectral>
class CameraModel : public SceneObject<CameraModel<TSpectral>> {
  public:
    CameraModel();

    CameraModel(const CameraModel&) = delete;
    CameraModel& operator=(const CameraModel&) = delete;

    void set_focal_length(units::Millimeter focal_length);
    units::Millimeter focal_length() const { return units::Millimeter(1000 * focal_length_); }

    void set_fstop(float fstop);
    float fstop() const;

    template <IsDistortion<TSpectral> TDistortion, typename... Args>
    void set_distortion(Args&&... args);

    void set_brown_conrady_distortion(BrownCoefficients coeffs);
    void set_opencv_distortion(OpenCVCoefficients coeffs);
    void set_owen_distortion(OwenCoefficients coeffs);

    void delete_distortion() { distortion_ = nullptr; }

    template <IsSensor<TSpectral> TSensor, typename... Args>
    void set_sensor(Args&&... args);

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

    Rotation<double> sensor_rotation() const;

    template <IsAperture TAperture, typename... Args>
    void set_aperture(Args&&... args);

    template <IsPSF TPSF, typename... Args>
    void set_psf(Args&&... args);

    void use_aperture_psf(int radius = 64, int banks = 16);
    void enable_psf_convolution(bool convolve_psf = true) { convolve_psf_ = convolve_psf; }
    void delete_psf();

    void set_veiling_glare(float alpha);
    void disable_veiling_glare();
    void set_harvey_shack_scatter(float scatter_fraction,
                                  float falloff_exponent,
                                  float r0 = 0.5f,
                                  float radius = 0.f);
    void disable_harvey_shack_scatter();

    bool has_psf() const { return psf_ != nullptr; }
    const Image<TSpectral>& get_psf_kernel(float u, float v) const
    {
        return psf_->get_kernel(u, v);
    }
    int get_psf_radius() const { return psf_->get_radius(); }

    void enable_depth_of_field(bool depth_of_field = true) { depth_of_field_ = depth_of_field; }
    void set_focus_distance(units::Meter focus_distance);
    units::Meter get_focus_distance() const { return units::Meter(d_); }
    void set_diopters(units::Diopter diopters);
    units::Diopter get_diopters() const;

    Pixel project_point(const Vec3<float>& point_camera_coords) const;
    Pixel try_project_point(const Vec3<float>& point_camera_coords) const;

    Ray<TSpectral> cast_ray(const Pixel& pixel, Sampler<float>& sampler) const;
    Ray<TSpectral> cast_ray(const Pixel& pixel) const;
    Ray<TSpectral> cast_ray(int x, int y) const;

    const Frustum<TSpectral>& view_frustum() const { return view_frustum_; }

    float pixel_radiance_to_power(int x, int y) const;

    bool in_fov(const Vec3<float>& point) const;

    void readout(FrameBuffer<TSpectral>& fb, units::Second exposure_time) const
    {
        sensor_->readout(fb, exposure_time);
    }

    float get_projected_aperture_area(const Vec3<float>& direction) const;

    Resolution resolution() const { return sensor_->resolution(); }

    std::string type() const override { return "CameraModel"; }

    FrameBuffer<TSpectral> make_frame_buffer() const
    {
        return FrameBuffer<TSpectral>(resolution());
    }

    void use_blender_convention(bool value = true) { blender_convention_ = value; }
    bool is_blender_convention() const { return blender_convention_; }

  protected:
    float focal_length_ = .05f;

    std::unique_ptr<SensorModel<TSpectral>> sensor_;
    std::unique_ptr<Aperture<TSpectral>> aperture_;
    std::unique_ptr<Distortion<TSpectral>> distortion_ = nullptr;
    std::unique_ptr<PSF<TSpectral>> psf_ = nullptr;
    bool convolve_psf_ = false;

    bool use_aperture_psf_ = false;
    float d_ = std::numeric_limits<float>::infinity();

    float veiling_alpha_ = 0.f;
    bool veiling_glare_enabled_ = false;

    float scatter_fraction_ = 0.f;
    float scatter_falloff_exponent_ = 2.f;
    float r0_ = 0.5f;
    float scatter_radius_ = 0.f;
    bool scatter_enabled_ = false;

    float fx_;
    float fy_;
    float cx_;
    float cy_;
    float rx_;
    float ry_;

    bool is_explicit_matrix_ = false;
    void compute_intrinsics_();

    bool depth_of_field_ = false;

    template <IsFloatingPoint TFloat>
    Vec3<TFloat> pixel_to_direction_(const Pixel& pixel) const;

    Image<Vec3<float>> distortion_field_;
    void compute_distortion_field_();

    Image<float> pixel_solid_angles_;
    void compute_pixel_solid_angles_();

    Vec3<double> tangent_(const Vec3<double>& p0, const Vec3<double>& p1) const;
    double triangle_solid_angle_(const Vec3<double>& c0,
                                 const Vec3<double>& c1,
                                 const Vec3<double>& c2) const;

    Frustum<TSpectral> view_frustum_;
    void compute_frustum_();

    bool blender_convention_ = false;

    friend class CameraModelHandle<TSpectral>;
    friend class Renderer<TSpectral>;
};
} // namespace huira

#include "huira_impl/cameras/camera_model.ipp"
