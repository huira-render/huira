#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "huira/cameras/apertures/aperture.hpp"
#include "huira/cameras/defocus_kernel.hpp"
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"
#include "huira/cameras/pixel_convention.hpp"
#include "huira/cameras/psfs/psf.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"
#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/geometry/ray.hpp"
#include "huira/images/fft_convolver.hpp"
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
 * to the image plane, compute projected aperture area, and supports both analytic and PSF-based
 * point spread functions. All units are SI unless otherwise noted.
 *
 * The kernels the camera's optics need for rendering (PSF stamps, defocus stamps, convolution
 * kernels and their spectra) are derived from its settings and built when first needed, not
 * by the setters. precompute() builds them ahead of time; see there for what a render does
 * when they are out of date.
 *
 * Settings can be made in any order, with one exception: the aperture. Until it is set, the
 * camera is f/2.8 at whatever focal length it has. set_fstop() and set_aperture_diameter() fix
 * the aperture's diameter, which a later focal length change keeps, changing the f-number
 * (and logging a warning if the f-stop was set directly). See set_fstop().
 *
 * Every setter checks its values and throws for ones that are not physically meaningful,
 * leaving the camera as it was.
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

    void set_aperture_diameter(units::Millimeter diameter);
    units::Millimeter aperture_diameter() const;

    template <IsDistortion<TSpectral> TDistortion, typename... Args>
    void set_distortion(Args&&... args);

    void set_brown_conrady_distortion(BrownCoefficients coeffs);
    void set_opencv_distortion(OpenCVCoefficients coeffs);
    void set_owen_distortion(OwenCoefficients coeffs);

    void delete_distortion();

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
                        units::Millimeter anchor_focal_length,
                        float skew = 0.f);

    Rotation<double> sensor_rotation() const;

    template <IsAperture TAperture, typename... Args>
    void set_aperture(Args&&... args);

    template <IsPSF TPSF, typename... Args>
    void set_psf(Args&&... args);

    void set_measured_psf(const Image<TSpectral>& data,
                          float samples_per_pixel,
                          int radius = 0,
                          int banks = 16);

    void use_aperture_psf(int radius = 64, int banks = 16);

    /// Enable or disable PSF convolution.
    void enable_psf_convolution(bool convolve_psf = true) { convolve_psf_ = convolve_psf; }
    void set_psf_convolution_radius(int radius);
    void delete_psf();

    void set_veiling_glare(float alpha);
    void disable_veiling_glare();
    void set_harvey_shack_scatter(float scatter_fraction,
                                  float falloff_exponent,
                                  float r0 = 0.5f,
                                  float radius = 0.f);
    void disable_harvey_shack_scatter();

    /// Check if the camera model has a PSF: the aperture's (see use_aperture_psf()), or one that
    /// was set.
    bool has_psf() const { return use_aperture_psf_ || psf_ != nullptr; }

    const Image<TSpectral>& get_psf_kernel(float u, float v);
    int get_psf_radius() const;

    const Image<TSpectral>& get_psf_convolution_kernel();
    const Image<TSpectral>& get_psf_wings_kernel();

    void precompute();
    [[nodiscard]] bool is_precomputed() const;
    void set_auto_precompute(bool auto_precompute = true);

    /// Whether a render builds out-of-date optics kernels itself. See set_auto_precompute().
    [[nodiscard]] bool auto_precompute() const { return auto_precompute_; }

    /// Enable or disable depth of field for the camera model.
    void enable_depth_of_field(bool depth_of_field = true) { depth_of_field_ = depth_of_field; }

    // Focus. The three setters are alternative ways of specifying the same state, and all
    // three getters are always available and mutually consistent, whichever was set.
    void set_focus_distance(units::Meter focus_distance);
    void set_focus_diopters(units::Diopter diopters);
    void set_focus_sensor_offset(units::Micrometer offset);

    units::Meter focus_distance() const;
    units::Diopter focus_diopters() const;
    units::Micrometer focus_sensor_offset() const;

    float defocus_blur_radius() const;

    void set_pixel_convention(PixelConvention convention);

    /// Get the pixel coordinate convention used for the principal point, project_point() and
    /// cast_ray(). See PixelConvention.
    PixelConvention pixel_convention() const { return pixel_convention_; }

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
    ///
    /// Does nothing when the frame buffer's sensor response is disabled: there is nowhere to
    /// put the result, and sensor models write into it unconditionally.
    void readout(FrameBuffer<TSpectral>& fb, units::Second exposure_time) const
    {
        if (!fb.has_sensor_response()) {
            return;
        }
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

    void use_blender_convention(bool value = true);

    /// Check if the Blender convention is enabled for the camera model.
    bool is_blender_convention() const { return blender_convention_; }

  protected:
    float focal_length_ = .05f;

    std::unique_ptr<SensorModel<TSpectral>> sensor_;
    std::unique_ptr<Aperture<TSpectral>> aperture_;
    std::unique_ptr<Distortion<TSpectral>> distortion_ = nullptr;
    /// The core PSF. With use_aperture_psf_ it is the aperture's diffraction PSF, made from the
    /// current optics when needed (see ensure_diffraction_()); otherwise it is the one set.
    std::unique_ptr<PSF<TSpectral>> psf_ = nullptr;
    bool use_aperture_psf_ = false;
    int aperture_psf_radius_ = 0;
    int aperture_psf_banks_ = 0;
    bool convolve_psf_ = false;

    // Whole-image convolution kernel: a single centered kernel, made from psf_ and the
    // scattered-light wings, and its spectrum at the sensor's resolution.
    int psf_convolution_radius_ = 0;
    Image<TSpectral> psf_convolution_kernel_;
    FftConvolver<TSpectral> psf_convolver_;

    // Scattered-light wings kernel alone (unit energy), used by the renderer to apply wings
    // to unresolved sources whose compact core is stamped rather than convolved.
    Image<TSpectral> psf_wings_kernel_;
    FftConvolver<TSpectral> wings_convolver_;

    /// Stamps for the defocus blur of unresolved sources; empty when in focus.
    DefocusKernel<TSpectral> defocus_kernel_;
    static constexpr int DEFOCUS_BANKS_ = 16;

    /// The settings the optics kernels are derived from. Each kernel depends on some of them,
    /// and is rebuilt only when one of those has changed since it was built.
    enum class OpticsInput : std::size_t {
        FocalLength,
        PixelPitch,
        Resolution,
        Aperture,
        Focus,
        CorePSF,
        Convolution,
        Scatter,
        Count
    };

    /// Counts optics changes. Starts at 1, so that a built_at of 0 means never built.
    std::uint64_t optics_version_ = 1;

    /// The version at which each input last changed.
    std::array<std::uint64_t, static_cast<std::size_t>(OpticsInput::Count)> optics_changed_at_{};

    void optics_changed_(OpticsInput input)
    {
        optics_changed_at_[static_cast<std::size_t>(input)] = ++optics_version_;
    }

    bool stale_(std::uint64_t built_at, std::initializer_list<OpticsInput> inputs) const;
    bool diffraction_stale_(std::uint64_t built_at) const;
    bool convolution_stale_(std::uint64_t built_at) const;
    bool wings_stale_(std::uint64_t built_at) const;

    // The version each kernel was built at; 0 for never.
    std::uint64_t diffraction_built_at_ = 0;
    std::uint64_t defocus_built_at_ = 0;
    std::uint64_t convolution_kernel_built_at_ = 0;
    std::uint64_t convolution_spectrum_built_at_ = 0;
    std::uint64_t wings_kernel_built_at_ = 0;
    std::uint64_t wings_spectrum_built_at_ = 0;

    // The geometry compute_intrinsics_() last saw, to tell which inputs a change touched.
    float optics_focal_length_ = 0.f;
    Vec2<float> optics_pixel_pitch_{0.f, 0.f};
    int optics_width_ = 0;
    int optics_height_ = 0;

    bool auto_precompute_ = true;
    bool explicitly_precomputed_ = false;

    /// Held while building optics kernels or applying a convolution spectrum, so that renders
    /// sharing this camera on different threads do not race.
    mutable std::mutex optics_mutex_;

    // Each builds its kernel if it is out of date. optics_mutex_ must be held.
    void ensure_diffraction_();
    void ensure_polyphase_();
    void ensure_defocus_();
    void ensure_convolution_kernel_();
    void ensure_convolution_spectrum_();
    void ensure_wings_kernel_();
    void ensure_wings_spectrum_();
    int convolution_radius_(const char* caller) const;

    // optics_mutex_ must be held.
    void precompute_locked_();
    bool is_precomputed_locked_() const;

    /// Kernels up to this many pixels are applied directly, as Image::convolve() does, and
    /// larger ones through their spectrum.
    static constexpr int DIRECT_CONVOLUTION_MAX_AREA_ = 25;

    // For the renderer:
    void precompute_for_render_();
    void apply_psf_convolution_(Image<TSpectral>& image) const;
    void apply_wings_convolution_(Image<TSpectral>& image) const;

    enum class FocusReference { Distance, Diopters, SensorOffset };
    FocusReference focus_reference_ = FocusReference::Distance;
    double focus_setting_ = std::numeric_limits<double>::infinity();

    /// Resolved focus distance used by ray generation
    float d_ = std::numeric_limits<float>::infinity();

    /// Closest focus distance accepted (meters), in either direction.
    static constexpr double MIN_FOCUS_DISTANCE_ = 1e-12;

    void check_focal_length_(float focal_length, const std::string& caller) const;

    static double
    resolve_focus_diopters_(FocusReference reference, double setting, double focal_length);
    void set_focus_(FocusReference reference, double setting);
    void update_focus_();

    float veiling_alpha_ = 0.f;
    bool veiling_glare_enabled_ = false;

    float scatter_fraction_ = 0.f;
    float scatter_falloff_exponent_ = 2.f;
    float r0_ = 0.5f;
    float scatter_radius_ = 0.f;
    bool scatter_enabled_ = false;

    float fx_;
    float fy_;
    /// Principal point in sensor coordinates; resolved by compute_intrinsics_().
    float cx_;
    float cy_;

    /// Skew in sensor coordinates (pixels): x_pixel = fx * x + skew * y + cx. Resolved by
    /// compute_intrinsics_() from shear_, the skew as given divided by fx, which is the
    /// cotangent of the angle between the pixel axes and so does not change with focal length.
    float skew_ = 0.f;
    float shear_ = 0.f;

    /// The principal point as it was given, in pixel_convention_; unset means the center of
    /// the sensor, whatever its resolution. Kept as given so that the result does not depend on
    /// whether the convention is set before or after it.
    std::optional<float> principal_x_;
    std::optional<float> principal_y_;
    PixelConvention pixel_convention_{};
    static void check_principal_point_(std::optional<float> cx,
                                       std::optional<float> cy,
                                       const Resolution& resolution,
                                       const std::string& caller);

    // Sensor-coordinate versions of the public projection and ray functions, for internal use
    // (the renderer works in sensor coordinates throughout). See PixelConvention.
    Pixel project_to_sensor_(const Vec3<float>& point_camera_coords) const;
    Ray<TSpectral> sensor_ray_(const Pixel& sensor_position, Sampler<float>& sampler) const;
    Ray<TSpectral> sensor_ray_(const Pixel& sensor_position) const;
    float rx_;
    float ry_;

    bool is_explicit_matrix_ = false;

    /// Where the aperture's size came from, which decides what a focal length change does to
    /// it: the default aperture stays f/2.8, and one that was set keeps its diameter.
    enum class ApertureSource { Default, FStop, Diameter };
    ApertureSource aperture_source_ = ApertureSource::Default;
    static constexpr float DEFAULT_FSTOP_ = 2.8f;

    /// The f-stop given to set_fstop(), and whether a focal length change since has been
    /// warned about.
    float fstop_setting_ = DEFAULT_FSTOP_;
    bool fstop_change_warned_ = false;

    void set_aperture_area_(units::Meter diameter);
    void focal_length_changed_(float previous_focal_length);

    /// Recompute everything derived from the camera's geometry settings. Every setter that
    /// changes focal length, sensor, principal point, distortion or axis convention calls this,
    /// so the result does not depend on the order the settings were made in.
    void compute_intrinsics_();

    bool depth_of_field_ = false;

    template <IsFloatingPoint TFloat>
    Vec3<TFloat> pixel_to_direction_(const Pixel& pixel) const;

    /// Unit ray directions at every pixel corner, (width + 1) x (height + 1) of them, so that
    /// bilinear lookup covers the whole sensor, [0, width] x [0, height]. Empty without
    /// distortion, when directions are computed directly.
    Image<Vec3<float>> distortion_field_;
    void compute_distortion_field_();
    Vec3<float> ray_direction_(const Pixel& pixel) const;

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
