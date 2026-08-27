
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <variant>

#include "huira/cameras/apertures/circular_aperture.hpp"
#include "huira/cameras/psfs/harvey_shack_scatter.hpp"
#include "huira/cameras/psfs/measured_psf.hpp"
#include "huira/cameras/sensors/simple_sensor.hpp"

namespace huira {
/**
 * @brief Construct a new CameraModel with default sensor and aperture.
 *
 * Initializes the camera with a default focal length, a SimpleSensor, and a CircularAperture.
 * The aperture diameter is set based on the focal length and a default f-stop of 2.8. The
 * optics default to a diffraction-limited core with no stray light.
 */
template <IsSpectral TSpectral>
CameraModel<TSpectral>::CameraModel()
{
    HUIRA_TRACE_SCOPE("CameraModel::CameraModel()");
    units::Meter diameter(this->focal_length_ / 2.8f);
    this->sensor_ = std::make_unique<SimpleSensor<TSpectral>>();
    this->aperture_ = std::make_unique<CircularAperture<TSpectral>>(diameter);
    cx_ = static_cast<float>(sensor_->resolution().x) * 0.5f;
    cy_ = static_cast<float>(sensor_->resolution().y) * 0.5f;
    compute_intrinsics_();
}

/**
 * @brief Set the focal length of the camera (in millimeters).
 *
 * @param focal_length Focal length in millimeters
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_focal_length(units::Millimeter focal_length)
{
    is_explicit_matrix_ = false;
    focal_length_ = focal_length.to_si_f();

    if (focal_length_ <= 0 || std::isinf(focal_length_) || std::isnan(focal_length_)) {
        HUIRA_THROW_ERROR(
            "CameraModel::set_focal_length - Focal length must be a positive finite value: " +
            std::to_string(focal_length_));
    }

    compute_intrinsics_();
    invalidate_optics_();
}

/**
 * @brief Set the distortion model for the camera.
 *
 * @tparam TDistortion Distortion model type
 * @tparam Args Constructor arguments for the distortion model
 * @param args Arguments to construct the distortion model
 */
template <IsSpectral TSpectral>
template <IsDistortion<TSpectral> TDistortion, typename... Args>
void CameraModel<TSpectral>::set_distortion(Args&&... args)
{
    distortion_ = std::make_unique<TDistortion>(std::forward<Args>(args)...);
    compute_distortion_field_();
    compute_frustum_();
}

/**
 * @brief Set Brown-Conrady distortion coefficients.
 * @param coeffs Brown distortion coefficients
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_brown_conrady_distortion(BrownCoefficients coeffs)
{
    this->set_distortion<BrownDistortion<TSpectral>>(coeffs);
}

/**
 * @brief Set OpenCV distortion coefficients.
 * @param coeffs OpenCV distortion coefficients
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_opencv_distortion(OpenCVCoefficients coeffs)
{
    this->set_distortion<OpenCVDistortion<TSpectral>>(coeffs);
}

/**
 * @brief Set Owen distortion coefficients.
 * @param coeffs Owen distortion coefficients
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_owen_distortion(OwenCoefficients coeffs)
{
    this->set_distortion<OwenDistortion<TSpectral>>(coeffs);
}

/**
 * @brief Set the sensor model for the camera.
 *
 * @tparam TSensor Sensor model type
 * @tparam Args Constructor arguments for the sensor
 * @param args Arguments to construct the sensor
 */
template <IsSpectral TSpectral>
template <IsSensor<TSpectral> TSensor, typename... Args>
void CameraModel<TSpectral>::set_sensor(Args&&... args)
{
    sensor_ = std::make_unique<TSensor>(std::forward<Args>(args)...);
    compute_intrinsics_();
    invalidate_optics_();
}

/**
 * @brief Configure the sensor using pixel pitch and resolution.
 *
 * This method sets the sensor resolution, pixel pitch, and principal point. It also computes the
 * intrinsics based on the new configuration.
 * @param resolution Sensor resolution
 * @param pitch_x Pixel pitch in x direction (micrometers)
 * @param pitch_y Pixel pitch in y direction (micrometers)
 * @param cx Principal point x coordinate (must be within resolution bounds)
 * @param cy Principal point y coordinate (must be within resolution bounds)
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::configure_sensor_from_pitch(const Resolution& resolution,
                                                         units::Micrometer pitch_x,
                                                         std::optional<units::Micrometer> pitch_y,
                                                         std::optional<float> cx,
                                                         std::optional<float> cy)
{
    is_explicit_matrix_ = false;

    sensor_->set_resolution(resolution);

    if (!pitch_y.has_value()) {
        pitch_y = pitch_x;
    }
    sensor_->set_pixel_pitch(pitch_x, pitch_y.value());

    float final_cx = cx.value_or(static_cast<float>(resolution.x) * 0.5f);
    float final_cy = cy.value_or(static_cast<float>(resolution.y) * 0.5f);

    if (std::isnan(final_cx) || std::isnan(final_cy) || std::isinf(final_cx) ||
        std::isinf(final_cy)) {
        HUIRA_THROW_ERROR("CameraModel - Principal point (cx, cy) must be finite numeric values.");
    }

    if (final_cx < -static_cast<float>(resolution.x) ||
        final_cx > static_cast<float>(resolution.x) * 2.0f ||
        final_cy < -static_cast<float>(resolution.y) ||
        final_cy > static_cast<float>(resolution.y) * 2.0f) {
        HUIRA_LOG_WARNING("Principal point is significantly outside the sensor resolution. Ensure "
                          "this intended for an off-axis projection.");
    }

    cx_ = final_cx;
    cy_ = final_cy;

    compute_intrinsics_();
    invalidate_optics_();
}

/**
 * @brief Configure the sensor using physical size and resolution.
 *
 * This method sets the sensor resolution, physical size, and principal point. It also computes the
 * intrinsics based on the new configuration.
 * @param resolution Sensor resolution
 * @param width Sensor width in millimeters
 * @param height Sensor height in millimeters
 * @param cx Principal point x coordinate (must be within resolution bounds)
 * @param cy Principal point y coordinate (must be within resolution bounds)
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::configure_sensor_from_size(const Resolution& resolution,
                                                        units::Millimeter width,
                                                        std::optional<units::Millimeter> height,
                                                        std::optional<float> cx,
                                                        std::optional<float> cy)
{
    is_explicit_matrix_ = false;

    sensor_->set_resolution(resolution);

    if (height.has_value()) {
        sensor_->set_sensor_size(width, height.value());
    } else {
        // If height is not provided, assume square pixels and compute height from width and
        // resolution
        float pixel_size_x = width.to_si_f() / static_cast<float>(resolution.x);
        float pixel_size_y = pixel_size_x; // Square pixels
        sensor_->set_pixel_pitch(units::Meter(pixel_size_x), units::Meter(pixel_size_y));
    }

    float final_cx = cx.value_or(static_cast<float>(resolution.x) * 0.5f);
    float final_cy = cy.value_or(static_cast<float>(resolution.y) * 0.5f);

    if (std::isnan(final_cx) || std::isnan(final_cy) || std::isinf(final_cx) ||
        std::isinf(final_cy)) {
        HUIRA_THROW_ERROR("CameraModel - Principal point (cx, cy) must be finite numeric values.");
    }

    if (final_cx < -static_cast<float>(resolution.x) ||
        final_cx > static_cast<float>(resolution.x) * 2.0f ||
        final_cy < -static_cast<float>(resolution.y) ||
        final_cy > static_cast<float>(resolution.y) * 2.0f) {
        HUIRA_LOG_WARNING("Principal point is significantly outside the sensor resolution. Ensure "
                          "this intended for an off-axis projection.");
    }

    cx_ = final_cx;
    cy_ = final_cy;

    compute_intrinsics_();
    invalidate_optics_();
}

/**
 * @brief Set the intrinsic matrix for the camera.
 * @param intrinsic_matrix 3x3 intrinsic matrix
 * @param resolution Sensor resolution
 * @param anchor_focal_length Anchor focal length in millimeters
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_intrinsic_matrix(const Mat3<float>& intrinsic_matrix,
                                                  const Resolution& resolution,
                                                  units::Millimeter anchor_focal_length)
{
    this->set_intrinsics(intrinsic_matrix[0][0],
                         intrinsic_matrix[1][1],
                         intrinsic_matrix[0][2],
                         intrinsic_matrix[1][2],
                         resolution,
                         anchor_focal_length);
}

/**
 * @brief Set the intrinsic parameters for the camera.
 * @param fx Focal length in x direction
 * @param fy Focal length in y direction
 * @param cx Principal point x coordinate
 * @param cy Principal point y coordinate
 * @param resolution Sensor resolution
 * @param anchor_focal_length Anchor focal length in millimeters
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_intrinsics(float fx,
                                            float fy,
                                            float cx,
                                            float cy,
                                            const Resolution& resolution,
                                            units::Millimeter anchor_focal_length)
{
    is_explicit_matrix_ = true;

    fx_ = fx;
    fy_ = fy;
    cx_ = cx;
    cy_ = cy;
    sensor_->set_resolution(resolution);
    focal_length_ = anchor_focal_length.to_si_f();

    // Use the anchor to compute the pixel_pitch/size
    units::Meter px(focal_length_ / fx_);
    units::Meter py(focal_length_ / fy_);
    sensor_->set_pixel_pitch(px, py);
    sensor_->set_sensor_size(px * resolution.x, py * resolution.y);

    compute_intrinsics_();
    invalidate_optics_();
}

/**
 * @brief Get the sensor rotation as a Rotation object.
 * @return Rotation<double> Sensor rotation
 */
template <IsSpectral TSpectral>
Rotation<double> CameraModel<TSpectral>::sensor_rotation() const
{
    Mat3<double> rot_matrix = Rotation<double>::local_to_parent_z(sensor_->config_.rotation);
    return Rotation<double>::from_local_to_parent(rot_matrix);
}

/**
 * @brief Set the aperture model for the camera.
 *
 * @tparam TAperture Aperture model type
 * @tparam Args Constructor arguments for the aperture
 * @param args Arguments to construct the aperture
 */
template <IsSpectral TSpectral>
template <IsAperture TAperture, typename... Args>
void CameraModel<TSpectral>::set_aperture(Args&&... args)
{
    aperture_ = std::make_unique<TAperture>(std::forward<Args>(args)...);
    invalidate_optics_();
}

/**
 * @brief Set the complete optical description of the camera.
 *
 * Replaces the PSF core, scattered-light wings, and veiling glare in one step. Kernels are
 * rebuilt on next use, so this may be called before or after the focal length, f-stop,
 * aperture, and sensor are configured.
 *
 * @param optics The optical description to apply.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_optics(Optics<TSpectral> optics)
{
    optics.validate();
    optics_ = std::move(optics);
    invalidate_optics_();
}

/**
 * @brief Replace the PSF core, leaving the stray-light components unchanged.
 * @param core The core to apply.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_core(typename Optics<TSpectral>::Core core)
{
    Optics<TSpectral> optics = optics_;
    optics.core = std::move(core);
    set_optics(std::move(optics));
}

/**
 * @brief Add scattered-light wings to the optics, leaving the core unchanged.
 * @param scatter The scatter parameters to apply.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_scatter(HarveyShack scatter)
{
    Optics<TSpectral> optics = optics_;
    optics.stray_light.scatter = std::move(scatter);
    set_optics(std::move(optics));
}

/**
 * @brief Set the fraction of energy redistributed uniformly across the frame.
 * @param fraction Veiling glare fraction in [0, 1].
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_veiling_glare(float fraction)
{
    Optics<TSpectral> optics = optics_;
    optics.stray_light.veiling_glare = fraction;
    set_optics(std::move(optics));
}

/**
 * @brief Remove the scattered-light wings, leaving the core and glare unchanged.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::clear_scatter()
{
    Optics<TSpectral> optics = optics_;
    optics.stray_light.scatter.reset();
    set_optics(std::move(optics));
}

/**
 * @brief Render with perfect optics: no PSF core, no scatter, and no glare.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_ideal_optics()
{
    set_optics(Optics<TSpectral>::ideal());
}

/**
 * @brief Discard every lazily built kernel and reset the automatic sampling choice.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::invalidate_optics_()
{
    optics_built_ = false;
    psf_convolution_kernel_valid_ = false;
    psf_wings_kernel_valid_ = false;
    psf_ = nullptr;
    scatter_kernel_radius_ = 0;
    ++psf_kernel_version_;

    if (aperture_) {
        aperture_->clear_defocus_kernel();
    }
    aperture_sampling_active_ = auto_aperture_sampling_();
}

/**
 * @brief Build the core PSF and defocus kernel if they are not already current.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_optics_built_() const
{
    if (optics_built_) {
        return;
    }
    build_core_psf_();
    build_defocus_();
    optics_built_ = true;
}

/**
 * @brief Build the PSF core described by the current optics.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::build_core_psf_() const
{
    const units::Meter f(focal_length_);
    const units::Meter px(sensor_->pixel_pitch().x);
    const units::Meter py(sensor_->pixel_pitch().y);

    if (const auto* diffraction = std::get_if<DiffractionCore>(&optics_.core)) {
        int radius = diffraction->radius.value_or(0);
        if (radius <= 0) {
            double max_wavelength = 0.0;
            for (std::size_t i = 0; i < TSpectral::size(); ++i) {
                max_wavelength = std::max(max_wavelength, TSpectral::get_bin(i).center_wavelength);
            }
            const float min_pitch = std::min(px.to_si_f(), py.to_si_f());
            radius = DiffractionCore::derive_radius(
                max_wavelength, static_cast<double>(fstop()), min_pitch);
        }
        radius = std::min(radius, max_kernel_radius_);
        psf_ = aperture_->make_psf(f, px, py, radius, diffraction->banks);
    } else if (const auto* measured = std::get_if<MeasuredCore<TSpectral>>(&optics_.core)) {
        const int radius = std::min(measured->radius.value_or(0), max_kernel_radius_);
        psf_ = std::make_shared<MeasuredPSF<TSpectral>>(
            measured->data, measured->samples_per_pixel, radius, measured->banks);
    } else if (const auto* custom = std::get_if<CustomCore<TSpectral>>(&optics_.core)) {
        psf_ = custom->psf;
    } else {
        psf_ = nullptr;
    }
}

/**
 * @brief Build the defocus kernel for the current focus setting.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::build_defocus_() const
{
    aperture_->build_defocus_kernel(defocus_blur_pixels(), DEFOCUS_BANKS);
}

/**
 * @brief Radius of the defocus blur spot for a source at infinity, in pixels.
 *
 * Zero means the camera is in focus at infinity. The kernel is only built once this
 * exceeds half a pixel.
 *
 * @return The blur radius in pixels.
 */
template <IsSpectral TSpectral>
float CameraModel<TSpectral>::defocus_blur_pixels() const
{
    if (!aperture_ || !sensor_) {
        return 0.f;
    }
    const float vergence = std::abs(focus_vergence_);
    if (!(vergence > 0.f) || std::isinf(vergence)) {
        return 0.f;
    }

    const float bounding_radius = aperture_->get_bounding_radius().to_si_f();
    const float blur_meters = vergence * focal_length_ * bounding_radius;
    const float pitch = std::min(sensor_->pixel_pitch().x, sensor_->pixel_pitch().y);
    if (!(pitch > 0.f)) {
        return 0.f;
    }
    return blur_meters / pitch;
}

/**
 * @brief Whether camera rays should be sampled across the aperture by default.
 *
 * Aperture sampling is only worth its noise cost once the camera is defocused enough for
 * the blur to be visible, which is the same threshold the stamped defocus kernel uses.
 */
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::auto_aperture_sampling_() const
{
    return defocus_blur_pixels() >= 0.5f;
}

/**
 * @brief Resolve the convolution kernel radius for the scattered-light wings.
 *
 * The radius spans the requested fraction of the scattered energy, bounded by the render
 * budget and by any explicit cutoff. An explicit kernel_radius overrides the derivation.
 *
 * @return The kernel radius in pixels.
 */
template <IsSpectral TSpectral>
int CameraModel<TSpectral>::resolve_scatter_radius_() const
{
    const HarveyShack& scatter = optics_.stray_light.scatter.value();

    float radius = 0.f;
    if (scatter.kernel_radius.has_value()) {
        radius = static_cast<float>(scatter.kernel_radius.value());
    } else {
        radius =
            HarveyShack::radius_for_energy(scatter.captured_energy, scatter.exponent, scatter.r0);
        if (scatter.cutoff_radius.has_value()) {
            radius = std::min(radius, scatter.cutoff_radius.value());
        }
    }

    int resolved = std::max(1, static_cast<int>(std::ceil(radius)));
    if (resolved > max_kernel_radius_) {
        resolved = max_kernel_radius_;
    }

    const float captured =
        HarveyShack::energy_within(static_cast<float>(resolved), scatter.exponent, scatter.r0);
    HUIRA_LOG_INFO("CameraModel - Scatter kernel radius " + std::to_string(resolved) +
                   " px spans " + std::to_string(100.f * captured) + "% of the scattered energy");

    return resolved;
}

/**
 * @brief Build every optical kernel this render needs, before any parallel work starts.
 *
 * The kernel accessors build on demand and are safe to call from a single thread, but the
 * polyphase caches are read concurrently while stamping. This forces the construction to
 * happen up front and applies the renderer's budget and sampling override.
 *
 * @param budget Render-time constraints supplied by the renderer.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::prepare_optics_(const OpticsBudget& budget)
{
    if (budget.max_kernel_radius != max_kernel_radius_) {
        max_kernel_radius_ = std::max(1, budget.max_kernel_radius);
        invalidate_optics_();
    }

    ensure_optics_built_();
    if (budget.build_convolution_kernel && (psf_ != nullptr || has_scatter())) {
        psf_convolution_kernel();
    }
    if (has_scatter()) {
        psf_wings_kernel();
    }

    aperture_sampling_active_ = budget.aperture_sampling.value_or(auto_aperture_sampling_());
    if (!aperture_sampling_active_ && auto_aperture_sampling_()) {
        HUIRA_LOG_WARNING("Aperture sampling is disabled while the camera is defocused. "
                          "Unresolved sources will be blurred but path-traced geometry will "
                          "render as though through a pinhole.");
    }
}

/**
 * @brief Build every optical kernel the camera currently describes.
 *
 * Rendering does this automatically. Call it directly before reading get_psf_kernel() or
 * get_psf_radius() outside a render, or to move the build cost off the first frame.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::build_optics() const
{
    ensure_optics_built_();
    if (psf_ != nullptr || has_scatter()) {
        psf_convolution_kernel();
    }
    if (has_scatter()) {
        psf_wings_kernel();
    }
}

/**
 * @brief Returns the total-system PSF kernel for whole-image convolution.
 *
 * The total point spread function is the energy-weighted sum of the core and the
 * Harvey-Shack wings. Veiling glare is uniform across the image and is applied separately
 * by the renderer. The kernel is normalized to unit energy per channel and cached until
 * any quantity it depends on changes.
 *
 * @return Reference to the cached total-system convolution kernel.
 */
template <IsSpectral TSpectral>
const Image<TSpectral>& CameraModel<TSpectral>::psf_convolution_kernel() const
{
    ensure_optics_built_();

    if (psf_ == nullptr && !has_scatter()) {
        HUIRA_THROW_ERROR("CameraModel::psf_convolution_kernel - The optics are ideal, so there "
                          "is no kernel to build");
    }

    if (!psf_convolution_kernel_valid_) {
        int radius = 0;
        if (has_scatter()) {
            scatter_kernel_radius_ = resolve_scatter_radius_();
            radius = scatter_kernel_radius_;
        }
        if (psf_ != nullptr) {
            radius = std::max(radius, psf_->get_radius());
        }
        radius = std::clamp(radius, 1, max_kernel_radius_);

        const int dim = 2 * radius + 1;
        HUIRA_LOG_INFO("CameraModel - Generating " + std::to_string(dim) + "x" +
                       std::to_string(dim) + " PSF convolution kernel");

        // With no core PSF the core is an ideal delta, leaving perfect optics plus scatter.
        if (psf_ != nullptr) {
            psf_convolution_kernel_ = psf_->generate_convolution_kernel(radius);
        } else {
            psf_convolution_kernel_ = Image<TSpectral>(dim, dim, TSpectral{0.f});
            psf_convolution_kernel_(radius, radius) = TSpectral{1.f};
        }

        // The wings are mixed with the core by energy fraction, so that the total system
        // PSF stays normalized to unit energy:
        //     psf_total = (1 - f_s) * core + f_s * wings
        const float f_s = scatter_fraction();
        if (f_s > 0.f) {
            const Image<TSpectral>& wings = psf_wings_kernel();
            const float core_weight = 1.f - f_s;
            for (int y = 0; y < dim; ++y) {
                for (int x = 0; x < dim; ++x) {
                    psf_convolution_kernel_(x, y) =
                        psf_convolution_kernel_(x, y) * core_weight + wings(x, y) * f_s;
                }
            }
        }

        psf_convolution_kernel_valid_ = true;
    }
    return psf_convolution_kernel_;
}

/**
 * @brief Returns the scattered-light wings kernel alone.
 *
 * This is the Harvey-Shack component of the total system PSF, normalized to unit energy
 * and not scaled by the scatter fraction. The renderer uses it to apply wings to
 * unresolved sources whose compact core is stamped rather than convolved.
 *
 * @return Reference to the cached wings kernel.
 */
template <IsSpectral TSpectral>
const Image<TSpectral>& CameraModel<TSpectral>::psf_wings_kernel() const
{
    if (!has_scatter()) {
        HUIRA_THROW_ERROR("CameraModel::psf_wings_kernel - The optics describe no scatter "
                          "component");
    }

    ensure_optics_built_();

    if (!psf_wings_kernel_valid_) {
        if (scatter_kernel_radius_ <= 0) {
            scatter_kernel_radius_ = resolve_scatter_radius_();
        }
        int radius = scatter_kernel_radius_;
        if (psf_ != nullptr) {
            radius = std::max(radius, psf_->get_radius());
        }
        radius = std::clamp(radius, 1, max_kernel_radius_);

        const HarveyShack& params = optics_.stray_light.scatter.value();
        HarveyShackScatter<TSpectral> scatter(
            params.exponent, params.r0, params.cutoff_radius.value_or(0.f));
        psf_wings_kernel_ = scatter.generate_convolution_kernel(radius);
        psf_wings_kernel_valid_ = true;
    }
    return psf_wings_kernel_;
}

/**
 * @brief Set the focus distance.
 *
 * A source at this distance is imaged in focus. Sources at other distances, including
 * everything at infinity, are blurred by the defocus this implies.
 *
 * @param distance Focus distance. Infinite focuses at infinity.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_focus(units::Meter distance)
{
    const float d = distance.to_si_f();
    if (std::isnan(d)) {
        HUIRA_THROW_ERROR("CameraModel::set_focus - Focus distance cannot be NaN");
    }
    if (std::isinf(d)) {
        set_focus(units::Diopter(0.f));
        return;
    }
    if (std::abs(d) < 1e-12f) {
        HUIRA_THROW_ERROR("CameraModel::set_focus - Focus distance is too small: " +
                          std::to_string(d));
    }
    set_focus(units::Diopter(1.f / d));
}

/**
 * @brief Set the focus as a vergence, the reciprocal of the focus distance.
 *
 * This parameterization is continuous through infinity: zero focuses at infinity, positive
 * values focus nearer, and negative values focus beyond infinity. It is also the natural
 * scale for describing a small focus error on an instrument that is nominally focused at
 * infinity, since the value is then the defocus applied to every distant source.
 *
 * @param vergence Focus vergence in diopters.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_focus(units::Diopter vergence)
{
    const float v = vergence.to_si_f();
    if (std::isnan(v) || std::isinf(v)) {
        HUIRA_THROW_ERROR("CameraModel::set_focus - Focus vergence must be finite");
    }
    focus_vergence_ = v;
    invalidate_optics_();
}

/**
 * @brief Get the focus distance.
 * @return The focus distance, infinite when focused at infinity.
 */
template <IsSpectral TSpectral>
units::Meter CameraModel<TSpectral>::focus_distance() const
{
    if (std::abs(focus_vergence_) < 1e-12f) {
        return units::Meter(std::numeric_limits<float>::infinity());
    }
    return units::Meter(1.f / focus_vergence_);
}

/**
 * @brief Project a 3D point in camera coordinates onto the image plane.
 *
 * Uses the pinhole camera model and applies distortion if present.
 * @param point_camera_coords 3D point in camera coordinates (meters)
 * @return Pixel 2D point on the image plane (pixels)
 */
template <IsSpectral TSpectral>
Pixel CameraModel<TSpectral>::project_point(const Vec3<float>& point_camera_coords) const
{
    float sign_y = 1.0f;
    float depth = point_camera_coords.z;
    if (blender_convention_) {
        depth = -depth;
        sign_y = -1.0f;
    }

    Pixel normalized{point_camera_coords.x / depth, sign_y * point_camera_coords.y / depth};
    if (distortion_) {
        normalized = distortion_->distort(normalized);
    }
    return Pixel{fx_ * normalized[0] + cx_, fy_ * normalized[1] + cy_};
}

template <IsSpectral TSpectral>
Pixel CameraModel<TSpectral>::try_project_point(const Vec3<float>& point_camera_coords) const
{
    auto NaN = std::numeric_limits<float>::quiet_NaN();
    if (!in_fov(point_camera_coords)) {
        return Pixel{NaN, NaN};
    }

    return project_point(point_camera_coords);
}

template <IsSpectral TSpectral>
Ray<TSpectral> CameraModel<TSpectral>::cast_ray(const Pixel& pixel, Sampler<float>& sampler) const
{
    assert(pixel[0] >= 0 && pixel[0] < rx_ && pixel[1] >= 0 && pixel[1] < ry_);

    Vec3<float> origin{0, 0, 0};
    Vec3<float> direction{0, 0, 1};
    if (distortion_) {
        float u = pixel[0] / static_cast<float>(rx_ - 1);
        float v = pixel[1] / static_cast<float>(ry_ - 1);
        direction = distortion_field_.sample_bilinear<WrapMode::Clamp>(u, v);
    } else {
        direction = pixel_to_direction_<float>(pixel);
    }

    if (aperture_sampling_active_) {
        Vec2<float> aperture_sample = aperture_->sample(sampler);
        Vec3<float> aperture_point{aperture_sample.x, aperture_sample.y, 0.f};

        // A negative vergence focuses beyond infinity, placing the focal point behind the
        // camera. Mirroring the offset keeps the ray fan converging in front of it.
        if (std::abs(focus_vergence_) > 1e-12f) {
            const float focus_distance = 1.f / focus_vergence_;
            Vec3<float> focal_point = direction * std::abs(focus_distance);
            if (focus_distance < 0.f) {
                aperture_point = -aperture_point;
            }
            direction = focal_point - aperture_point;
        }
        origin = aperture_point;
    }

    return Ray<TSpectral>{origin, glm::normalize(direction)};
}

template <IsSpectral TSpectral>
Ray<TSpectral> CameraModel<TSpectral>::cast_ray(const Pixel& pixel) const
{
    assert(pixel[0] >= 0 && pixel[0] < rx_ && pixel[1] >= 0 && pixel[1] < ry_);

    Vec3<float> origin{0, 0, 0};
    Vec3<float> direction{0, 0, 1};
    if (distortion_) {
        float u = pixel[0] / static_cast<float>(rx_ - 1);
        float v = pixel[1] / static_cast<float>(ry_ - 1);
        direction = distortion_field_.sample_bilinear<WrapMode::Clamp>(u, v);
    } else {
        direction = pixel_to_direction_<float>(pixel);
    }

    return Ray<TSpectral>{origin, glm::normalize(direction)};
}

template <IsSpectral TSpectral>
Ray<TSpectral> CameraModel<TSpectral>::cast_ray(int x, int y) const
{
    Pixel pixel{static_cast<float>(x), static_cast<float>(y)};
    return cast_ray(pixel);
}

template <IsSpectral TSpectral>
float CameraModel<TSpectral>::pixel_radiance_to_power(int x, int y) const
{
    Ray<TSpectral> ray = cast_ray(x, y);
    return pixel_solid_angles_(x, y) * this->get_projected_aperture_area(ray.direction());
}

template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::in_fov(const Vec3<float>& point) const
{
    float len2 = glm::dot(point, point);
    if (len2 < 1e-12f) {
        return false;
    }
    return view_frustum_.contains(point);
}

/**
 * @brief Get the projected aperture area for a given direction.
 * @param direction Direction vector
 * @return float Projected aperture area
 */
template <IsSpectral TSpectral>
float CameraModel<TSpectral>::get_projected_aperture_area(const Vec3<float>& direction) const
{
    float cosTheta = glm::dot(glm::normalize(direction), Vec3<float>{0, 0, 1});
    return this->aperture_->get_area().to_si_f() * std::abs(cosTheta);
}

/**
 * @brief Set the f-stop (aperture ratio) of the camera.
 * @param fstop F-stop value
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_fstop(float fstop)
{
    units::Meter aperture_diameter(focal_length_ / fstop);
    units::SquareMeter aperture_area = PI<float>() * (aperture_diameter * aperture_diameter) / 4.f;
    this->aperture_->set_area(aperture_area);
    invalidate_optics_();
}

/**
 * @brief Get the f-stop (aperture ratio) of the camera.
 * @return float F-stop value
 */
template <IsSpectral TSpectral>
float CameraModel<TSpectral>::fstop() const
{
    float area = this->aperture_->get_area().to_si_f();
    float aperture_diameter = 2.f * std::sqrt(area / PI<float>());
    return focal_length_ / aperture_diameter;
}

/**
 * @brief Compute the camera intrinsic parameters (focal lengths, principal point, resolution).
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::compute_intrinsics_()
{
    rx_ = static_cast<float>(sensor_->resolution().x);
    ry_ = static_cast<float>(sensor_->resolution().y);

    if (!is_explicit_matrix_) {
        fx_ = focal_length_ / sensor_->pixel_pitch().x;
        fy_ = focal_length_ / sensor_->pixel_pitch().y;
    }

    compute_frustum_();
    compute_pixel_solid_angles_();
}

template <IsSpectral TSpectral>
template <IsFloatingPoint TFloat>
Vec3<TFloat> CameraModel<TSpectral>::pixel_to_direction_(const Pixel& pixel) const
{
    // Invert the intrinsic matrix
    float nx = (pixel[0] - cx_) / fx_;
    float ny = (pixel[1] - cy_) / fy_;
    Pixel normalized{nx, ny};

    // Undistort
    if (distortion_) {
        normalized = distortion_->undistort(normalized);
    }

    // Build direction in camera coordinates
    TFloat dir_x = static_cast<TFloat>(normalized[0]);
    TFloat dir_y = static_cast<TFloat>(normalized[1]);

    Vec3<TFloat> direction;
    if (blender_convention_) {
        direction = Vec3<TFloat>{dir_x, -dir_y, static_cast<TFloat>(-1)};
    } else {
        direction = Vec3<TFloat>{dir_x, dir_y, static_cast<TFloat>(1)};
    }

    return direction;
}

template <IsSpectral TSpectral>
void CameraModel<TSpectral>::compute_distortion_field_()
{
    Resolution res = sensor_->resolution();

    distortion_field_ = Image<Vec3<float>>(res, Vec3<float>{0, 0, 0});

    for (int x = 0; x < res.x; ++x) {
        for (int y = 0; y < res.y; ++y) {
            Pixel pixel{static_cast<float>(x), static_cast<float>(y)};
            distortion_field_(x, y) = glm::normalize(pixel_to_direction_<float>(pixel));
        }
    }
}

template <IsSpectral TSpectral>
void CameraModel<TSpectral>::compute_pixel_solid_angles_()
{
    Resolution res = sensor_->resolution();

    pixel_solid_angles_ = Image<float>(res, 0.f);
    for (int x = 0; x < res.x; ++x) {
        for (int y = 0; y < res.y; ++y) {
            // Calculate normalized directions to pixel corners
            Vec3<double> c0 = glm::normalize(
                pixel_to_direction_<double>(Pixel{static_cast<float>(x), static_cast<float>(y)}));
            Vec3<double> c1 = glm::normalize(pixel_to_direction_<double>(
                Pixel{static_cast<float>(x + 1), static_cast<float>(y)}));
            Vec3<double> c2 = glm::normalize(pixel_to_direction_<double>(
                Pixel{static_cast<float>(x + 1), static_cast<float>(y + 1)}));
            Vec3<double> c3 = glm::normalize(pixel_to_direction_<double>(
                Pixel{static_cast<float>(x), static_cast<float>(y + 1)}));

            // Compute solid angle as sum of two triangular areas
            double omega1 = triangle_solid_angle_(c0, c1, c2);
            double omega2 = triangle_solid_angle_(c0, c2, c3);

            pixel_solid_angles_(x, y) = static_cast<float>(omega1 + omega2);
        }
    }
}

template <IsSpectral TSpectral>
Vec3<double> CameraModel<TSpectral>::tangent_(const Vec3<double>& p0, const Vec3<double>& p1) const
{
    Vec3<double> p = p1 - p0;
    Vec3<double> r = glm::cross(p0, p);
    Vec3<double> t = glm::cross(r, p0);
    return glm::normalize(t);
}

template <IsSpectral TSpectral>
double CameraModel<TSpectral>::triangle_solid_angle_(const Vec3<double>& c0,
                                                     const Vec3<double>& c1,
                                                     const Vec3<double>& c2) const
{
    // Compute interior angles using tangent vectors
    Vec3<double> t01 = tangent_(c0, c1);
    Vec3<double> t02 = tangent_(c0, c2);
    double angle0 = std::acos(glm::dot(t01, t02));

    Vec3<double> t10 = tangent_(c1, c0);
    Vec3<double> t12 = tangent_(c1, c2);
    double angle1 = std::acos(glm::dot(t10, t12));

    Vec3<double> t20 = tangent_(c2, c0);
    Vec3<double> t21 = tangent_(c2, c1);
    double angle2 = std::acos(glm::dot(t20, t21));

    // Apply Girard's theorem
    return angle0 + angle1 + angle2 - PI<double>();
}

template <IsSpectral TSpectral>
void CameraModel<TSpectral>::compute_frustum_()
{
    Resolution res = sensor_->resolution();

    Vec3<float> xdir{1, 0, 0};
    Vec3<float> ydir{0, 1, 0};
    Vec3<float> zdir{0, 0, 1};
    if (blender_convention_) {
        zdir = Vec3<float>{0, 0, -1};
        ydir = Vec3<float>{0, -1, 0};
    }

    // Initialize the frustum side planes:
    Vec3<float> left_extrema{0, 0, 0};
    float left_min_dot = 100.f;

    Vec3<float> right_extrema{0, 0, 0};
    float right_min_dot = 100.f;

    Vec3<float> top_extrema{0, 0, 0};
    float top_min_dot = 100.f;

    Vec3<float> bottom_extrema{0, 0, 0};
    float bottom_min_dot = 100.f;

    auto update = [&](int i, int j, Vec3<float>& extrema, float& min_dot) {
        Ray<TSpectral> ray = cast_ray(i, j);
        Vec3<float> direction = ray.direction();
        float d = glm::dot(direction, zdir);
        if (d < min_dot) {
            extrema = direction;
            min_dot = d;
        }
    };

    // Loop over all edge-pixels
    for (int i = 0; i < res.x; ++i) {
        update(i, 0, top_extrema, top_min_dot);
        update(i, res.y - 1, bottom_extrema, bottom_min_dot);
    }
    for (int j = 1; j < res.y - 1; ++j) {
        update(0, j, left_extrema, left_min_dot);
        update(res.x - 1, j, right_extrema, right_min_dot);
    }

    // Form the frustum planes:
    Vec3<float> left_normal = glm::normalize(glm::cross(ydir, left_extrema));
    Vec3<float> right_normal = glm::normalize(glm::cross(right_extrema, ydir));
    Vec3<float> top_normal = glm::normalize(glm::cross(top_extrema, xdir));
    Vec3<float> bottom_normal = glm::normalize(glm::cross(xdir, bottom_extrema));

    view_frustum_ =
        Frustum<TSpectral>({zdir, left_normal, right_normal, top_normal, bottom_normal});
}
} // namespace huira
