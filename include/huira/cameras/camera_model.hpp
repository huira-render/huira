#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "huira/cameras/apertures/aperture.hpp"
#include "huira/cameras/defocus_kernel.hpp"
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"
#include "huira/cameras/pixel_convention.hpp"
#include "huira/cameras/psfs/measured_psf.hpp"
#include "huira/cameras/psfs/psf.hpp"
#include "huira/cameras/psfs/psf_tables.hpp"
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
#include "huira/util/logger.hpp"
#include "huira/util/removed_api.hpp"

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
 * kernels and their spectra), and the tables of its geometry (each pixel's ray direction and
 * solid angle, and the view frustum), are derived from its settings and built when first
 * needed, not by the setters, which are therefore cheap. precompute() builds them ahead of
 * time; see there for what a render does when they are out of date. Getters give the same
 * results either way.
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
    /// Stamp size for a PSF's stamps for unresolved sources, unless given: radius in pixels,
    /// and subpixel positions per axis. See use_aperture_psf() and set_psf(). The radius is
    /// also the largest the aperture's automatic stamp radius gets.
    static constexpr int DEFAULT_PSF_RADIUS = 64;
    static constexpr int DEFAULT_PSF_BANKS = 16;

    /// The smallest automatic stamp radius for the aperture's diffraction pattern, in pixels.
    /// See use_aperture_psf().
    static constexpr int MIN_AUTO_PSF_RADIUS = 16;

    /// The largest radius psf_image() takes, in pixels.
    static constexpr int MAX_PSF_IMAGE_RADIUS = 16384;

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

    void set_sensor_roll(units::Radian angle);
    units::Radian sensor_roll() const;
    Rotation<double> sensor_orientation() const;

    std::pair<units::Micrometer, units::Micrometer> pixel_pitch() const;
    Mat3<float> intrinsic_matrix() const;

    template <IsAperture TAperture, typename... Args>
    void set_aperture(Args&&... args);

    template <IsPSF TPSF, typename... Args>
    void set_psf(Args&&... args);

    void set_measured_psf(const Image<TSpectral>& data,
                          float samples_per_pixel,
                          PSFSampling sampling,
                          int radius = 0,
                          int banks = DEFAULT_PSF_BANKS);

    void use_aperture_psf(int radius = 0, int banks = DEFAULT_PSF_BANKS);

    /// Choose whether a PSF or scattered light that is set blurs resolved bodies (the path-traced
    /// image). On by default. Off, bodies are sharp, and rendering is faster without the
    /// whole-image convolution; unresolved sources, such as stars, get the PSF and scattered light
    /// either way.
    void enable_psf_convolution(bool convolve_psf = true) { convolve_psf_ = convolve_psf; }

    /// Whether a PSF or scattered light that is set blurs resolved bodies (the default). See
    /// enable_psf_convolution().
    bool psf_convolution_enabled() const { return convolve_psf_; }

    void set_psf_convolution_radius(int radius);
    void delete_psf();

    void set_veiling_glare(float alpha);
    void disable_veiling_glare();
    void set_harvey_shack_scatter(float scatter_fraction,
                                  float falloff_exponent,
                                  float r0 = 0.5f,
                                  float radius = 0.f);
    void disable_harvey_shack_scatter();
    void set_scatter(float fraction,
                     float slope,
                     units::Radian shoulder_angle,
                     std::optional<units::Radian> outer_angle = std::nullopt);

    /// Check if the camera model has a PSF: the aperture's diffraction pattern (the default; see
    /// use_aperture_psf()), or one that was set. False after delete_psf().
    bool has_psf() const { return use_aperture_psf_ || psf_ != nullptr; }

    /// Whether the PSF is the aperture's diffraction pattern (the default). See
    /// use_aperture_psf().
    bool uses_aperture_psf() const { return use_aperture_psf_; }

    const Image<TSpectral>& get_psf_kernel(float u, float v);
    int get_psf_radius() const;

    const Image<TSpectral>& get_psf_convolution_kernel();
    const Image<TSpectral>& get_psf_wings_kernel();

    Image<TSpectral> psf_image(int radius,
                               float x_offset = 0.f,
                               float y_offset = 0.f,
                               std::optional<units::Meter> range = std::nullopt);

    void precompute();
    [[nodiscard]] bool is_precomputed() const;
    void enable_auto_precompute(bool auto_precompute = true);

    /// Whether a render builds an out-of-date camera itself (the default). See
    /// enable_auto_precompute().
    [[nodiscard]] bool auto_precompute_enabled() const { return auto_precompute_; }

    void enable_depth_of_field(bool depth_of_field = true);

    /// Whether depth of field is on (the default). See enable_depth_of_field().
    bool depth_of_field_enabled() const { return depth_of_field_; }

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
    std::optional<Ray<TSpectral>> try_cast_ray(const Pixel& pixel) const;

    /// Get the frustum representing the camera's field of view. Found from the image's boundary
    /// when first needed after a change (see precompute()).
    const Frustum<TSpectral>& view_frustum() const
    {
        ensure_frustum_();
        return view_frustum_;
    }

    float pixel_radiance_to_power(int x, int y) const;

    bool in_fov(const Vec3<float>& point) const;

    /// Read out the sensor into the given frame buffer with the specified exposure time.
    ///
    /// Does nothing when the frame buffer's sensor response is disabled: there is nowhere to
    /// put the result, and sensor models write into it unconditionally.
    ///
    /// @throws std::runtime_error if the sensor response is enabled but the received power it
    ///         reads out is not (FrameBuffer::enable_received_power(false) after
    ///         enable_sensor_response()).
    void readout(FrameBuffer<TSpectral>& fb, units::Second exposure_time) const
    {
        if (!fb.has_sensor_response()) {
            return;
        }
        if (!fb.has_received_power()) {
            HUIRA_THROW_ERROR("CameraModel::readout - The frame buffer's sensor response is "
                              "enabled, but not the received power it reads out. Enable it "
                              "(enable_received_power()), or disable the sensor response.");
        }
        sensor_->readout(fb, exposure_time);
    }

    float projected_aperture_area(const Vec3<float>& direction) const;

    /// Get the sensor resolution.
    Resolution resolution() const { return sensor_->resolution(); }

    /// Get the type of the camera model.
    std::string type() const override { return "CameraModel"; }

    /// Create a frame buffer matching the sensor resolution.
    FrameBuffer<TSpectral> make_frame_buffer() const
    {
        return FrameBuffer<TSpectral>(resolution());
    }

    void use_blender_convention(bool value = true);

    /// Whether the camera uses Blender's axes (-z forward, y up) rather than OpenCV's (z forward,
    /// y down, the default). See use_blender_convention().
    bool uses_blender_convention() const { return blender_convention_; }

    std::string describe() const;

    // Removed: calling these fails to compile, with what to use instead.
    template <typename... Args>
    Rotation<double> sensor_rotation(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "CameraModel::sensor_rotation() was removed in v0.9.10. Use "
                      "sensor_orientation() for the rotation, or sensor_roll() for the angle.");
        return {};
    }

    template <typename... Args>
    bool is_blender_convention(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "is_blender_convention() was removed in v0.9.10. Use "
                      "uses_blender_convention() instead.");
        return {};
    }

    template <typename... Args>
    float get_projected_aperture_area(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "get_projected_aperture_area() was removed in v0.9.10. Use "
                      "projected_aperture_area() instead.");
        return {};
    }

    template <typename... Args>
    Resolution res(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "CameraModel::res() was removed in v0.9.10. Use resolution() instead.");
        return resolution();
    }

  protected:
    float focal_length_ = .05f;

    std::unique_ptr<SensorModel<TSpectral>> sensor_;
    std::unique_ptr<Aperture<TSpectral>> aperture_;
    std::unique_ptr<Distortion<TSpectral>> distortion_ = nullptr;
    /// The core PSF. With use_aperture_psf_ (the default) it is the aperture's diffraction PSF,
    /// made from the current optics when needed (see ensure_diffraction_()); otherwise it is the
    /// one set.
    std::unique_ptr<PSF<TSpectral>> psf_ = nullptr;
    bool use_aperture_psf_ = true;
    int aperture_psf_radius_ = 0; ///< 0 for automatic: see aperture_psf_stamp_radius_().
    int aperture_psf_banks_ = DEFAULT_PSF_BANKS;
    int aperture_psf_stamp_radius_() const;
    int table_stamp_radius_() const;
    bool convolve_psf_ = true;

    /// Whether there is anything to blur with: a PSF, or scattered light.
    bool has_psf_or_scatter_() const { return has_psf() || scatter_enabled_; }

    /// Whether the path-traced image of resolved bodies is convolved with the PSF and scattered
    /// light: see enable_psf_convolution().
    bool convolves_bodies_() const { return convolve_psf_ && has_psf_or_scatter_(); }

    // Whole-image convolution kernel: a single centered kernel, made from psf_ and the
    // scattered-light wings, and its spectrum at the sensor's resolution.
    int psf_convolution_radius_ = 0;
    Image<TSpectral> psf_convolution_kernel_;
    FftConvolver<TSpectral> psf_convolver_;

    // Scattered-light wings kernel alone (unit energy), used by the renderer to apply wings
    // to unresolved sources whose compact core is stamped rather than convolved.
    Image<TSpectral> psf_wings_kernel_;
    FftConvolver<TSpectral> wings_convolver_;

    /// The tables that give an unresolved source's light in each pixel: see psf_image(). Shared,
    /// so that an image can be read from them while they are rebuilt for new optics.
    std::shared_ptr<const PsfTables<TSpectral>> psf_tables_;

    /// The tables the renderer draws still unresolved sources from with the aperture's PSF: its
    /// diffraction with the scattered light, blurred as a star is by the focus, and the reach
    /// they were built for. psf_tables_ shares them.
    std::shared_ptr<const PsfTables<TSpectral>> render_tables_;
    double render_tables_reach_ = 0.0;
    double render_tables_blur_ = 0.0;

    /// The same without the scattered light, which the wings add to moving sources, for the
    /// stamps, and the blur they were built for: render_tables_ without scattered light.
    std::shared_ptr<const PsfTables<TSpectral>> stamp_tables_;
    double stamp_tables_blur_ = 0.0;

    /// Stamps from stamp_tables_, for sources that move during the exposure: one per subpixel
    /// position, banks x banks of them, each normalized per channel as PSF's are, with their
    /// radius.
    std::vector<Image<TSpectral>> table_stamps_;
    int table_stamp_banks_ = 0;
    int table_stamps_radius_ = 0;

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
    std::uint64_t render_tables_built_at_ = 0;
    std::uint64_t stamp_tables_built_at_ = 0;
    std::uint64_t table_stamps_built_at_ = 0;

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
    void ensure_psf_tables_();
    void ensure_render_tables_();
    void ensure_stamp_tables_();
    void ensure_table_stamps_();
    [[nodiscard]] double tables_reach_() const;
    [[nodiscard]] bool render_tables_stale_() const;
    [[nodiscard]] bool stamp_tables_stale_() const;
    [[nodiscard]] double tables_blur_() const;
    [[nodiscard]] bool table_stamps_stale_() const;
    std::shared_ptr<const PsfTables<TSpectral>>
    build_psf_tables_(double blur,
                      const std::optional<typename PsfTables<TSpectral>::Scatter>& scatter,
                      double reach) const;
    std::optional<typename PsfTables<TSpectral>::Scatter> scatter_for_tables_() const;
    double blur_pixels_(double inverse_range) const;
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

    /// Whether the scattered light was set in angles, with set_scatter(), rather than in pixels.
    /// Its shoulder and outer angles, in radians (0 for no outer angle), then replace r0_ and
    /// scatter_radius_, which follow from them.
    bool scatter_in_angles_ = false;
    double scatter_shoulder_angle_ = 0.0;
    double scatter_outer_angle_ = 0.0;
    std::array<float, 2> scatter_pixels_() const;

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
    // use_field: interpolate the direction from the distortion field when it is up to date, as
    // the renderer does; cast_ray() computes it directly. See ray_direction_().
    Ray<TSpectral>
    sensor_ray_(const Pixel& sensor_position, Sampler<float>& sampler, bool use_field = true) const;
    Ray<TSpectral> sensor_ray_(const Pixel& sensor_position, bool use_field = true) const;
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

    /// Resolve the intrinsics (fx, fy, cx, cy, skew) from the geometry settings, and mark the
    /// geometry tables out of date. Every setter that changes focal length, sensor, principal
    /// point, distortion or axis convention calls this, so the result does not depend on the
    /// order the settings were made in. It is cheap: the tables are built when needed (see
    /// ensure_pixel_geometry_() and ensure_frustum_()).
    void compute_intrinsics_();

    bool depth_of_field_ = true;

    template <IsFloatingPoint TFloat>
    Vec3<TFloat> pixel_to_direction_(const Pixel& pixel) const;

    // ---- Geometry tables ----
    //
    // Derived from the intrinsics and distortion, per pixel or along the image's boundary, and
    // costly to build for a large sensor with distortion (seconds). Setters only mark them out
    // of date (geometry_version_); they are built when first needed: by precompute() or a
    // render, or, for the frustum, by a getter that needs it. Getters that need a single value
    // (cast_ray(), pixel_radiance_to_power()) compute it directly while the tables are out of
    // date, the same way the tables do, so every getter gives the same result whether or not
    // they have been built. Built under geometry_mutex_; readers check their stamp, which is
    // atomic, so renders on several threads can share a camera.

    /// What the tables depend on. A change of distortion model, which is an object, counts as
    /// a change of distortion_version_.
    struct GeometryKey {
        float fx = 0.f;
        float fy = 0.f;
        float cx = 0.f;
        float cy = 0.f;
        float skew = 0.f;
        float rx = 0.f;
        float ry = 0.f;
        bool blender = false;
        std::uint64_t distortion_version = 0;
        bool operator==(const GeometryKey&) const = default;
    };
    std::uint64_t distortion_version_ = 0;

    /// The version of the geometry settings, set by compute_intrinsics_(): a new one when they
    /// change, or, when they change back to what a table was built with, that table's. So
    /// setting a value to what it already is, or back to what it was, rebuilds nothing.
    GeometryKey geometry_key_{};
    std::uint64_t geometry_version_ = 0;
    std::uint64_t next_geometry_version_ = 1;
    static constexpr std::uint64_t NEVER_BUILT_ = std::numeric_limits<std::uint64_t>::max();
    mutable std::mutex geometry_mutex_;

    /// The per-pixel tables: built together, from one pass over the pixel corners.
    mutable std::atomic<std::uint64_t> pixel_geometry_built_at_{NEVER_BUILT_};
    mutable GeometryKey pixel_geometry_built_key_{};
    [[nodiscard]] bool pixel_geometry_current_() const
    {
        return pixel_geometry_built_at_.load(std::memory_order_acquire) == geometry_version_;
    }
    void ensure_pixel_geometry_() const;
    void build_pixel_geometry_() const;

    /// Unit ray directions at every pixel corner, (width + 1) x (height + 1) of them, so that
    /// bilinear lookup covers the whole sensor, [0, width] x [0, height]. Empty without
    /// distortion, when directions are computed directly.
    mutable Image<Vec3<float>> distortion_field_;

    /// Each pixel's power per unit radiance and aperture area: its solid angle times the
    /// cosine of the angle between the optical axis and the pixel's center. See
    /// pixel_radiance_to_power().
    mutable Image<float> pixel_geometry_factors_;
    double pixel_geometry_factor_(int x, int y) const;
    double pixel_geometry_factor_(const Vec3<double>& c00,
                                  const Vec3<double>& c10,
                                  const Vec3<double>& c11,
                                  const Vec3<double>& c01) const;
    Vec3<double> corner_direction_(int x, int y) const;

    /// Direction (not normalized) of the pinhole ray through a sensor position: interpolated
    /// from the distortion field when use_field and it is up to date, computed directly
    /// otherwise.
    Vec3<float> ray_direction_(const Pixel& pixel, bool use_field = true) const;

    /// The first pixel corner (sensor coordinates) where the distortion has no inverse, which
    /// makes the camera unusable until a setting changes; empty when there is none. Found by
    /// build_pixel_geometry_(), and reported by check_distortion_().
    mutable std::optional<Pixel> distortion_failure_;
    void check_distortion_(const std::string& caller) const;
    [[noreturn]] void throw_no_inverse_(const std::string& caller,
                                        const Pixel& sensor_position) const;

    Vec3<double> tangent_(const Vec3<double>& p0, const Vec3<double>& p1) const;
    double triangle_solid_angle_(const Vec3<double>& c0,
                                 const Vec3<double>& c1,
                                 const Vec3<double>& c2) const;

    /// The view frustum, and the bounds it is made from.
    mutable std::atomic<std::uint64_t> frustum_built_at_{NEVER_BUILT_};
    mutable GeometryKey frustum_built_key_{};
    mutable Frustum<TSpectral> view_frustum_;
    void ensure_frustum_() const;
    void build_frustum_() const;

    /// Where the view frustum's side planes are, as the tangent of their angle from the forward
    /// axis along the image's x and y axes, and the largest change in that tangent one pixel
    /// makes at the image's boundary. Found by build_frustum_().
    struct FrustumBounds {
        Vec3<float> xdir{1, 0, 0};
        Vec3<float> ydir{0, 1, 0};
        Vec3<float> zdir{0, 0, 1};
        float min_tan_x = 0.f;
        float max_tan_x = 0.f;
        float min_tan_y = 0.f;
        float max_tan_y = 0.f;
        float tan_per_pixel = 0.f;
    };
    mutable FrustumBounds frustum_bounds_;
    Frustum<TSpectral> frustum_from_bounds_(float widen_tan) const;

    /// Both tables up to date.
    [[nodiscard]] bool geometry_current_() const;
    void ensure_geometry_() const;

    /// The view frustum widened by at least the given number of pixels on every side, to cull
    /// sources whose light reaches into the image from just outside it.
    Frustum<TSpectral> view_frustum_with_margin_(float pixels) const;

    bool blender_convention_ = false;

    friend class CameraModelHandle<TSpectral>;
    friend class Renderer<TSpectral>;
};
} // namespace huira

#include "huira_impl/cameras/camera_model.ipp"
