#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "huira/cameras/apertures/aperture.hpp"
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"
#include "huira/cameras/pixel_convention.hpp"
#include "huira/cameras/psfs/grid_psf.hpp"
#include "huira/cameras/psfs/measured_psf.hpp"
#include "huira/cameras/psfs/psf.hpp"
#include "huira/cameras/psfs/psf_convolver.hpp"
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

template <IsSpectral TSpectral>
class SceneView;

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
 * The kernels the camera's optics need for rendering (the PSF's tables or grid, and the
 * convolution of resolved bodies with it), and the tables of its geometry (each pixel's ray
 * direction and solid angle, and the view frustum), are derived from its settings and built when
 * first needed, not by the setters, which are therefore cheap. precompute() builds them ahead of
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
    /// How far from the source, in pixels, a PSF that was set holds light, unless it says
    /// (see PSF::radius() and set_psf()).
    static constexpr int DEFAULT_PSF_RADIUS = 64;

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

    void enable_diffraction(bool diffraction = true);

    /// Whether the aperture's diffraction pattern is on (the default). A PSF that was set takes
    /// its place either way. See enable_diffraction().
    [[nodiscard]] bool diffraction_enabled() const { return diffraction_enabled_; }

    template <IsPSF TPSF, typename... Args>
    void set_psf(Args&&... args);

    void set_measured_psf(const Image<TSpectral>& data,
                          float samples_per_pixel,
                          PSFSampling sampling,
                          PSFFocus focus = PSFFocus::InFocus);

    void clear_psf();

    /// Whether a PSF was set with set_psf() or set_measured_psf(), in place of the aperture's
    /// diffraction pattern. See clear_psf().
    [[nodiscard]] bool has_set_psf() const { return psf_ != nullptr; }

    void set_scatter(float fraction,
                     float slope,
                     units::Radian shoulder_angle,
                     std::optional<units::Radian> outer_angle = std::nullopt);
    void disable_scatter();

    /// Whether scattered light is on: see set_scatter(). Off by default.
    [[nodiscard]] bool scatter_enabled() const { return scatter_enabled_; }

    void set_veiling_glare(float alpha);
    void disable_veiling_glare();

    /// Choose whether the PSF and scattered light blur resolved bodies (the path-traced image).
    /// On by default. Off, bodies are sharp, and rendering is faster without the whole-image
    /// convolution; unresolved sources, such as stars, get the PSF and scattered light either
    /// way. Bodies get the whole PSF and scattered light, across the frame, in focus: path
    /// tracing gives them their defocus.
    void enable_psf_convolution(bool convolve_psf = true) { convolve_psf_ = convolve_psf; }

    /// Whether the PSF and scattered light blur resolved bodies (the default). See
    /// enable_psf_convolution().
    bool psf_convolution_enabled() const { return convolve_psf_; }

    Image<TSpectral> psf_image(int radius,
                               float x_offset = 0.f,
                               float y_offset = 0.f,
                               std::optional<units::Meter> range = std::nullopt);
    Image<TSpectral>
    diffraction_image(int radius, float x_offset = 0.f, float y_offset = 0.f) const;
    Image<TSpectral> scatter_image(int radius, float x_offset = 0.f, float y_offset = 0.f) const;
    Image<TSpectral> defocus_image(int radius,
                                   float x_offset = 0.f,
                                   float y_offset = 0.f,
                                   std::optional<units::Meter> range = std::nullopt) const;

    void precompute();
    [[nodiscard]] bool is_precomputed() const;
    void enable_auto_precompute(bool auto_precompute = true);

    /// Whether a render builds an out-of-date camera itself (the default). See
    /// enable_auto_precompute().
    [[nodiscard]] bool auto_precompute_enabled() const { return auto_precompute_; }

    void enable_depth_of_field(bool depth_of_field = true);

    /// Whether depth of field is on (the default). See enable_depth_of_field().
    bool depth_of_field_enabled() const { return depth_of_field_; }

    void enable_motion_blur(bool motion_blur = true);

    /// Whether motion blur is on (the default). See enable_motion_blur().
    [[nodiscard]] bool motion_blur_enabled() const { return motion_blur_; }

    // Focus. The three setters are alternative ways of specifying the same state, and all
    // three getters are always available and mutually consistent, whichever was set.
    void set_focus_distance(units::Meter focus_distance);
    void set_focus_diopters(units::Diopter diopters);
    void set_focus_sensor_offset(units::Micrometer offset);

    units::Meter focus_distance() const;
    units::Diopter focus_diopters() const;
    units::Micrometer focus_sensor_offset() const;

    float defocus_blur_radius(std::optional<units::Meter> range = std::nullopt) const;

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
    /// @throws std::runtime_error if the exposure time is negative or not finite, or the sensor
    ///         response is enabled but the received power it reads out is not
    ///         (FrameBuffer::enable_received_power(false) after enable_sensor_response()).
    void readout(FrameBuffer<TSpectral>& fb, units::Second exposure_time) const
    {
        if (!(exposure_time.to_si() >= 0.0) || !std::isfinite(exposure_time.to_si())) {
            HUIRA_THROW_ERROR("CameraModel::readout - The exposure time must be non-negative and "
                              "finite: " +
                              std::to_string(exposure_time.to_si()) + " s");
        }
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

    // The old set_measured_psf(data, samples_per_pixel[, radius, banks]), with no sampling:
    template <typename... Args>
        requires(sizeof...(Args) <= 2 && (std::is_integral_v<std::remove_cvref_t<Args>> && ...))
    void set_measured_psf(const Image<TSpectral>&, float, Args&&...)
    {
        static_assert(
            detail::removed_api<Args...>,
            "set_measured_psf(data, samples_per_pixel, radius, banks) changed in v0.9.10. Say "
            "what the samples are: set_measured_psf(data, samples_per_pixel, "
            "PSFSampling::PointSampled or PSFSampling::PixelIntegrated[, PSFFocus]). "
            "The measurement is taken whole, out to its farthest sample: crop it to "
            "use less.");
    }

    template <typename... Args>
    void set_diopters(Args&&...)
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
    void use_aperture_psf(Args&&...)
    {
        static_assert(detail::removed_api<Args...>,
                      "use_aperture_psf() was removed in v0.9.10. The aperture's diffraction "
                      "pattern is on by default: use enable_diffraction() to turn it on or off, "
                      "and clear_psf() to go back to it from a PSF that was set. Its extent is "
                      "automatic.");
    }

    template <typename... Args>
    void delete_psf(Args&&...)
    {
        static_assert(detail::removed_api<Args...>,
                      "delete_psf() was removed in v0.9.10. Use clear_psf() to remove a PSF "
                      "that was set, and enable_diffraction(false) to turn the aperture's "
                      "diffraction pattern off.");
    }

    template <typename... Args>
    bool has_psf(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "has_psf() was removed in v0.9.10. Use has_set_psf() for a PSF that was "
                      "set, or diffraction_enabled() for the aperture's diffraction pattern.");
        return {};
    }

    template <typename... Args>
    void set_psf_convolution_radius(Args&&...)
    {
        static_assert(detail::removed_api<Args...>,
                      "set_psf_convolution_radius() was removed in v0.9.10. Resolved bodies are "
                      "convolved with the whole PSF across the frame; use psf_image() to see it.");
    }

    template <typename... Args>
    void set_harvey_shack_scatter(Args&&...)
    {
        static_assert(detail::removed_api<Args...>,
                      "set_harvey_shack_scatter() was removed in v0.9.10. Use set_scatter(), "
                      "which takes the shoulder and outer radius as angles.");
    }

    template <typename... Args>
    void disable_harvey_shack_scatter(Args&&...)
    {
        static_assert(detail::removed_api<Args...>,
                      "disable_harvey_shack_scatter() was removed in v0.9.10. Use "
                      "disable_scatter() instead.");
    }

    template <typename... Args>
    const Image<TSpectral>& get_psf_kernel(Args&&...)
    {
        static_assert(detail::removed_api<Args...>,
                      "get_psf_kernel() was removed in v0.9.10. Use psf_image(radius, x_offset, "
                      "y_offset), which gives the PSF as the renderer draws it, at any offset.");
        static const Image<TSpectral> none;
        return none;
    }

    template <typename... Args>
    int get_psf_radius(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "get_psf_radius() was removed in v0.9.10. Sources are drawn as far as "
                      "their light reaches (see Renderer::set_unresolved_taper()); psf_image() "
                      "takes the radius to show.");
        return {};
    }

    template <typename... Args>
    const Image<TSpectral>& get_psf_convolution_kernel(Args&&...)
    {
        static_assert(detail::removed_api<Args...>,
                      "get_psf_convolution_kernel() was removed in v0.9.10. Use psf_image(), the "
                      "whole PSF resolved bodies are convolved with.");
        static const Image<TSpectral> none;
        return none;
    }

    template <typename... Args>
    const Image<TSpectral>& get_psf_wings_kernel(Args&&...)
    {
        static_assert(detail::removed_api<Args...>,
                      "get_psf_wings_kernel() was removed in v0.9.10. Use scatter_image() "
                      "instead.");
        static const Image<TSpectral> none;
        return none;
    }

    template <typename... Args>
    std::uint64_t psf_kernel_version(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "psf_kernel_version() was removed in v0.9.10. The camera rebuilds what its "
                      "settings change; is_precomputed() says whether that is still to do.");
        return {};
    }

  protected:
    float focal_length_ = .05f;

    std::unique_ptr<SensorModel<TSpectral>> sensor_;
    std::unique_ptr<Aperture<TSpectral>> aperture_;
    std::unique_ptr<Distortion<TSpectral>> distortion_ = nullptr;
    /// A PSF set with set_psf() or set_measured_psf(), which takes the place of the aperture's
    /// diffraction pattern; none if not set.
    std::unique_ptr<PSF<TSpectral>> psf_ = nullptr;
    bool diffraction_enabled_ = true;
    bool convolve_psf_ = true;

    /// Whether unresolved sources are drawn with the aperture's diffraction pattern: it is on,
    /// and no PSF was set in its place. Its tables then hold the scattered light too.
    bool uses_diffraction_() const { return diffraction_enabled_ && psf_ == nullptr; }

    /// Whether there is anything to blur with: diffraction, a PSF that was set, or scattered
    /// light.
    bool has_psf_or_scatter_() const
    {
        return uses_diffraction_() || psf_ != nullptr || scatter_enabled_;
    }

    /// Whether the path-traced image of resolved bodies is convolved with the PSF and scattered
    /// light: see enable_psf_convolution().
    bool convolves_bodies_() const { return convolve_psf_ && has_psf_or_scatter_(); }

    /// The tables the renderer draws unresolved sources from, and psf_image() reads: with the
    /// aperture's diffraction pattern, it with the scattered light, and otherwise the scattered
    /// light alone; blurred as a star is by the focus, with the reach they were built for.
    /// Shared, so that an image can be read from them while they are rebuilt for new optics.
    std::shared_ptr<const PsfTables<TSpectral>> render_tables_;
    double render_tables_reach_ = 0.0;
    double render_tables_blur_ = 0.0;
    /// Whether the tables hold the scattered light alone, as they do for a PSF that was set or
    /// none: see tables_scatter_(). The same for object_tables_ and body_tables_.
    bool render_tables_scatter_only_ = false;

    /// What is built for unresolved objects by their blur, where it differs from a star's: tables
    /// with the diffraction pattern, or a PSF that was set on its grid. Most recently used last.
    /// Those the last render used are kept, and others up to OBJECT_TABLES_KEPT_ in all.
    template <class T>
    struct BlurCache {
        struct Entry {
            double blur = 0.0;
            std::shared_ptr<const T> value;
        };
        std::vector<Entry> entries;

        /// The kept one nearest blur, if within PsfTables::BLUR_TOLERANCE of it, or one made with
        /// build(); used counts those this render has used, which are kept last.
        template <class Build>
        std::shared_ptr<const T> find(double blur, std::size_t& used, const Build& build)
        {
            auto nearest = entries.end();
            for (auto it = entries.begin(); it != entries.end(); ++it) {
                if (nearest == entries.end() ||
                    std::abs(it->blur - blur) < std::abs(nearest->blur - blur)) {
                    nearest = it;
                }
            }
            if (nearest != entries.end() &&
                std::abs(nearest->blur - blur) <= PsfTables<TSpectral>::BLUR_TOLERANCE) {
                std::shared_ptr<const T> value = nearest->value;
                // Moved to the end as the most recently used, unless this render has used it:
                const auto position = static_cast<std::size_t>(nearest - entries.begin());
                if (position < entries.size() - used) {
                    Entry found = std::move(*nearest);
                    entries.erase(nearest);
                    entries.push_back(std::move(found));
                    ++used;
                }
                return value;
            }
            entries.push_back(Entry{blur, build()});
            ++used;
            return entries.back().value;
        }

        /// Drops those this render did not use, oldest first, beyond the number kept.
        void trim(std::size_t used)
        {
            const std::size_t kept = std::max(used, OBJECT_TABLES_KEPT_);
            if (entries.size() > kept) {
                entries.erase(entries.begin(),
                              entries.begin() + static_cast<std::ptrdiff_t>(entries.size() - kept));
            }
        }
    };
    static constexpr std::size_t OBJECT_TABLES_KEPT_ = 8;

    /// A PSF set with set_psf() or set_measured_psf(), on its grid (see GridPsf), or without one
    /// a point, drawn exactly (see GridPsf::disc()): in focus, and blurred as a star is by the
    /// focus, with the blur, which the renderer draws unresolved sources from; and for objects
    /// whose blur differs from a star's, by their blur.
    std::shared_ptr<const GridPsf<TSpectral>> focused_grid_;
    std::shared_ptr<const GridPsf<TSpectral>> grid_psf_;
    double grid_psf_blur_ = 0.0;
    BlurCache<GridPsf<TSpectral>> object_grids_;

    /// Tables for unresolved objects whose blur differs from a star's, with the reach they were
    /// built for.
    BlurCache<PsfTables<TSpectral>> object_tables_;
    double object_tables_reach_ = 0.0;
    bool object_tables_scatter_only_ = false;

    /// What resolved bodies are convolved with: the whole of the PSF, across the frame, from
    /// render_tables_ in focus, or body_tables_ otherwise, with a PSF that was set on its grid in
    /// focus (see body_parts_()).
    PsfConvolver<TSpectral> body_convolver_;

    /// The tables for resolved bodies when a star is out of focus: in focus, as render_tables_
    /// are otherwise, since path tracing gives bodies their own defocus. Empty in focus, when the
    /// renderer's serve. With the reach they were built for.
    std::shared_ptr<const PsfTables<TSpectral>> body_tables_;
    double body_tables_reach_ = 0.0;
    bool body_tables_scatter_only_ = false;

    /// The settings the optics kernels are derived from. Each kernel depends on some of them,
    /// and is rebuilt only when one of those has changed since it was built.
    enum class OpticsInput : std::size_t {
        FocalLength,
        PixelPitch,
        Resolution,
        Aperture,
        Focus,
        CorePSF,
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

    // The version each kernel was built at; 0 for never.
    std::uint64_t render_tables_built_at_ = 0;
    std::uint64_t body_tables_built_at_ = 0;
    std::uint64_t object_tables_built_at_ = 0;
    std::uint64_t grid_source_built_at_ = 0;
    std::uint64_t object_grids_built_at_ = 0;

    // The geometry compute_intrinsics_() last saw, to tell which inputs a change touched.
    float optics_focal_length_ = 0.f;
    Vec2<float> optics_pixel_pitch_{0.f, 0.f};
    int optics_width_ = 0;
    int optics_height_ = 0;

    bool auto_precompute_ = true;
    bool explicitly_precomputed_ = false;
    /// Whether note_object_build_() has warned since precompute() was last called.
    bool warned_object_build_ = false;
    void note_object_build_();

    /// Held while building optics kernels or applying a convolution spectrum, so that renders
    /// sharing this camera on different threads do not race.
    mutable std::mutex optics_mutex_;

    // Each builds its kernel if it is out of date. optics_mutex_ must be held.
    void ensure_render_tables_();
    void ensure_body_convolver_();
    void ensure_grid_psf_();
    std::vector<std::shared_ptr<const PsfTables<TSpectral>>>
    tables_at_locked_(const std::vector<double>& inverse_depths);
    std::vector<std::shared_ptr<const GridPsf<TSpectral>>>
    grids_at_locked_(const std::vector<double>& inverse_depths);
    [[nodiscard]] std::shared_ptr<const GridPsf<TSpectral>> blurred_grid_(double blur,
                                                                          double aspect) const;
    [[nodiscard]] double source_blur_(double inverse_depth) const;
    [[nodiscard]] double tables_reach_() const;
    [[nodiscard]] bool render_tables_stale_() const;
    [[nodiscard]] double tables_blur_() const;
    [[nodiscard]] bool body_tables_stale_() const;
    [[nodiscard]] bool body_convolver_stale_() const;
    [[nodiscard]] bool grid_psf_stale_() const;
    std::shared_ptr<const PsfTables<TSpectral>>
    build_psf_tables_(double blur,
                      const std::optional<typename PsfTables<TSpectral>::Scatter>& scatter,
                      double reach) const;
    std::optional<typename PsfTables<TSpectral>::Scatter> scatter_for_tables_() const;
    std::optional<typename PsfTables<TSpectral>::Scatter> tables_scatter_() const;
    [[nodiscard]] bool uses_tables_() const { return uses_diffraction_() || scatter_enabled_; }
    [[nodiscard]] typename PsfConvolver<TSpectral>::Parts body_parts_() const;
    double blur_pixels_(double inverse_range) const;
    void check_image_arguments_(const char* caller,
                                int radius,
                                float x_offset,
                                float y_offset,
                                std::optional<units::Meter> range) const;
    [[nodiscard]] double image_reach_(int radius) const;

    // optics_mutex_ must be held.
    void precompute_locked_();
    bool is_precomputed_locked_() const;

    // For the renderer:
    void precompute_for_render_();
    std::vector<std::shared_ptr<const PsfTables<TSpectral>>>
    tables_at_(const std::vector<double>& inverse_depths);
    std::vector<std::shared_ptr<const GridPsf<TSpectral>>>
    grids_at_(const std::vector<double>& inverse_depths);
    [[nodiscard]] std::array<double, TSpectral::size()>
    psf_convolution_error_(const Image<TSpectral>& image, FftPrecision precision) const;
    void apply_psf_convolution_(Image<TSpectral>& image,
                                FftPrecision precision,
                                FftPlanEffort effort) const;

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

    /// The scattered light (see set_scatter()): its fraction, slope, and shoulder and outer
    /// angles in radians (0 for no outer angle).
    float scatter_fraction_ = 0.f;
    float scatter_slope_ = 2.5f;
    bool scatter_enabled_ = false;
    double scatter_shoulder_angle_ = 0.0;
    double scatter_outer_angle_ = 0.0;

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
    bool motion_blur_ = true;

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
    friend class SceneView<TSpectral>;
};
} // namespace huira

#include "huira_impl/cameras/camera_model.ipp"
