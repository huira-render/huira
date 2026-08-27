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
#include "huira/cameras/optics.hpp"
#include "huira/cameras/psfs/psf.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"
#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/geometry/ray.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/render/frustum.hpp"
#include "huira/sampling/sampler.hpp"
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
 * to the image plane and compute projected aperture area. All units are SI unless otherwise noted.
 *
 * The optical response is described by an Optics object covering the PSF core, scattered-light
 * wings, and veiling glare. It defaults to a diffraction-limited core derived from the aperture,
 * and everything it describes is applied to both path-traced geometry and unresolved point
 * sources. Kernels are built lazily, so the optics and the quantities they depend on may be set
 * in any order. Use Optics::ideal() to render without any PSF.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class CameraModel : public SceneObject<CameraModel<TSpectral>> {
  public:
    CameraModel();

    CameraModel(const CameraModel&) = delete;
    CameraModel& operator=(const CameraModel&) = delete;

    void set_focal_length(units::Millimeter focal_length);

    /// Get the focal length of the camera in millimeters.
    units::Millimeter focal_length() const { return units::Millimeter(1000 * focal_length_); }

    void set_fstop(float fstop);
    float fstop() const;

    template <IsDistortion<TSpectral> TDistortion, typename... Args>
    void set_distortion(Args&&... args);

    void set_brown_conrady_distortion(BrownCoefficients coeffs);
    void set_opencv_distortion(OpenCVCoefficients coeffs);
    void set_owen_distortion(OwenCoefficients coeffs);

    /// Delete the current distortion model.
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

    void set_optics(Optics<TSpectral> optics);

    /// Get the camera's optical description.
    const Optics<TSpectral>& optics() const { return optics_; }

    void set_core(typename Optics<TSpectral>::Core core);
    void set_scatter(HarveyShack scatter);
    void set_veiling_glare(float fraction);
    void clear_scatter();
    void set_ideal_optics();

    /// Check whether a PSF core is present. Requires build_optics() or a render first.
    bool has_core_psf() const { return psf_ != nullptr; }

    /// Check whether the optics include scattered-light wings.
    bool has_scatter() const { return optics_.stray_light.scatter.has_value(); }

    /// Get the fraction of energy diverted into the scattered-light wings.
    float scatter_fraction() const
    {
        return has_scatter() ? optics_.stray_light.scatter->fraction : 0.f;
    }

    /// Get the fraction of energy redistributed uniformly across the frame.
    float veiling_glare() const { return optics_.stray_light.veiling_glare; }

    /// Get the PSF kernel at the specified subpixel offset.
    const Image<TSpectral>& get_psf_kernel(float u, float v) const
    {
        return psf_->get_kernel(u, v);
    }

    /// Get the PSF stamping radius in pixels.
    int get_psf_radius() const { return psf_->get_radius(); }

    const Image<TSpectral>& psf_convolution_kernel() const;
    const Image<TSpectral>& psf_wings_kernel() const;

    void build_optics() const;

    /// Monotonic counter identifying the current PSF/scatter kernel configuration.
    [[nodiscard]] std::uint64_t psf_kernel_version() const noexcept { return psf_kernel_version_; }

    void set_focus(units::Meter distance);
    void set_focus(units::Diopter vergence);

    units::Meter focus_distance() const;

    /// Get the focus setting as a vergence, the reciprocal of the focus distance.
    units::Diopter focus_vergence() const { return units::Diopter(focus_vergence_); }

    float defocus_blur_pixels() const;

    /// Check whether the camera is defocused enough to blur sources at infinity.
    bool has_defocus() const { return aperture_->has_defocus(); }

    /// Check whether camera rays are sampled across the aperture.
    bool aperture_sampling_active() const { return aperture_sampling_active_; }

    Pixel project_point(const Vec3<float>& point_camera_coords) const;
    Pixel try_project_point(const Vec3<float>& point_camera_coords) const;

    Ray<TSpectral> cast_ray(const Pixel& pixel, Sampler<float>& sampler) const;
    Ray<TSpectral> cast_ray(const Pixel& pixel) const;
    Ray<TSpectral> cast_ray(int x, int y) const;

    /// Get the frustum representing the camera's field of view.
    const Frustum<TSpectral>& view_frustum() const { return view_frustum_; }

    float pixel_radiance_to_power(int x, int y) const;

    bool in_fov(const Vec3<float>& point) const;

    /// Read out the sensor into the given frame buffer with the specified exposure time.
    void readout(FrameBuffer<TSpectral>& fb, units::Second exposure_time) const
    {
        sensor_->readout(fb, exposure_time);
    }

    float get_projected_aperture_area(const Vec3<float>& direction) const;

    /// Get the sensor resolution.
    Resolution resolution() const { return sensor_->resolution(); }
    Resolution res() const { return sensor_->resolution(); }

    /// Get the type of the camera model.
    std::string type() const override { return "CameraModel"; }

    /// Create a frame buffer matching the sensor resolution.
    FrameBuffer<TSpectral> make_frame_buffer() const { return FrameBuffer<TSpectral>(res()); }

    /// Enable or disable the Blender convention for the camera model.
    void use_blender_convention(bool value = true) { blender_convention_ = value; }

    /// Check if the Blender convention is enabled for the camera model.
    bool is_blender_convention() const { return blender_convention_; }

  protected:
    float focal_length_ = .05f;

    std::unique_ptr<SensorModel<TSpectral>> sensor_;
    std::unique_ptr<Aperture<TSpectral>> aperture_;
    std::unique_ptr<Distortion<TSpectral>> distortion_ = nullptr;

    Optics<TSpectral> optics_{};

    /// Focus setting as a vergence in diopters. Zero is infinity, negative is beyond it.
    float focus_vergence_ = 0.f;

    bool aperture_sampling_active_ = false;

    // Lazily built optical state. Every accessor below builds on demand, so the camera may
    // be configured in any order; Renderer calls prepare_optics_() before any parallel work.
    mutable std::shared_ptr<PSF<TSpectral>> psf_ = nullptr;
    mutable bool optics_built_ = false;
    mutable int max_kernel_radius_ = 1024;
    mutable int scatter_kernel_radius_ = 0;

    mutable Image<TSpectral> psf_convolution_kernel_;
    mutable bool psf_convolution_kernel_valid_ = false;

    mutable Image<TSpectral> psf_wings_kernel_;
    mutable bool psf_wings_kernel_valid_ = false;

    /// See psf_kernel_version(). Starts at 1 so that 0 is usable as "never seen".
    mutable std::uint64_t psf_kernel_version_ = 1;

    /// Polyphase banks per axis used for the defocus kernel.
    static constexpr int DEFOCUS_BANKS = 16;

    void invalidate_optics_();
    void ensure_optics_built_() const;
    void build_core_psf_() const;
    void build_defocus_() const;
    int resolve_scatter_radius_() const;
    bool auto_aperture_sampling_() const;
    void prepare_optics_(const OpticsBudget& budget);

    float fx_;
    float fy_;
    float cx_;
    float cy_;
    float rx_;
    float ry_;

    bool is_explicit_matrix_ = false;
    void compute_intrinsics_();

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
