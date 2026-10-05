
#include <limits>
#include <memory>

#include "huira/cameras/apertures/circular_aperture.hpp"
#include "huira/cameras/psfs/harvey_shack_scatter.hpp"
#include "huira/cameras/psfs/measured_psf.hpp"
#include "huira/cameras/sensors/simple_sensor.hpp"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

namespace huira {
/**
 * @brief Construct a new CameraModel with default sensor and aperture.
 *
 * Initializes the camera with a default focal length, a SimpleSensor, and a CircularAperture.
 * The aperture diameter is set based on the focal length and a default f-stop of 2.8.
 */
template <IsSpectral TSpectral>
CameraModel<TSpectral>::CameraModel()
{
    HUIRA_TRACE_SCOPE("CameraModel::CameraModel()");
    units::Meter diameter(this->focal_length_ / 2.8f);
    this->sensor_ = std::make_unique<SimpleSensor<TSpectral>>();
    this->aperture_ = std::make_unique<CircularAperture<TSpectral>>(diameter);
    compute_intrinsics_(); // principal point unset: the center of the sensor
}

/**
 * @brief Set the focal length of the camera (in millimeters).
 *
 * Updates the camera intrinsics and, if using aperture PSF, updates the PSF as well.
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
    if (use_aperture_psf_) {
        units::Meter f(focal_length_);
        units::Meter px(sensor_->pixel_pitch().x);
        units::Meter py(sensor_->pixel_pitch().y);
        psf_ = aperture_->make_psf(f, px, py, psf_->get_radius(), psf_->get_banks());
        invalidate_psf_kernels_();
    }
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
    compute_intrinsics_();
}

/**
 * @brief Delete the current distortion model.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::delete_distortion()
{
    distortion_ = nullptr;
    compute_intrinsics_();
}

/**
 * @brief Enable or disable the Blender convention for the camera model.
 *
 * In the Blender convention the camera looks along -Z with +Y up, rather than along +Z with
 * +Y down. The image itself is the same either way.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::use_blender_convention(bool value)
{
    blender_convention_ = value;
    compute_intrinsics_();
}

/**
 * @brief Set the pixel coordinate convention the camera's users work in.
 *
 * Applies to the principal point passed to configure_sensor_from_pitch(),
 * configure_sensor_from_size(), set_intrinsics() and set_intrinsic_matrix(), and to the
 * positions project_point() returns and cast_ray() takes. The default,
 * PixelConvention::opencv(), puts the center of the top-left pixel at (0, 0) with y down. See
 * PixelConvention for the others.
 *
 * It can be set before or after the principal point: a principal point is kept as given and
 * read in whichever convention is current. Nothing rendered changes unless a principal point
 * was given, since the default principal point is the center of the sensor in every
 * convention.
 *
 * @param convention The convention, e.g. PixelConvention::fits().
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_pixel_convention(PixelConvention convention)
{
    pixel_convention_ = convention;
    compute_intrinsics_();
}

/**
 * @brief Validate and store a principal point as given (unset means the sensor's center).
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_principal_point_(std::optional<float> cx,
                                                  std::optional<float> cy,
                                                  const Resolution& resolution)
{
    if ((cx && !std::isfinite(*cx)) || (cy && !std::isfinite(*cy))) {
        HUIRA_THROW_ERROR("CameraModel - Principal point (cx, cy) must be finite numeric values.");
    }

    const float width = static_cast<float>(resolution.x);
    const float height = static_cast<float>(resolution.y);
    if ((cx && (*cx < -width || *cx > 2.f * width)) ||
        (cy && (*cy < -height || *cy > 2.f * height))) {
        HUIRA_LOG_WARNING("Principal point is significantly outside the sensor resolution. Ensure "
                          "this intended for an off-axis projection.");
    }

    principal_x_ = cx;
    principal_y_ = cy;
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
}

/**
 * @brief Configure the sensor using pixel pitch and resolution.
 *
 * This method sets the sensor resolution, pixel pitch, and principal point. It also computes the
 * intrinsics based on the new configuration.
 * @param resolution Sensor resolution
 * @param pitch_x Pixel pitch in x direction (micrometers)
 * @param pitch_y Pixel pitch in y direction (micrometers)
 * @param cx Principal point x coordinate, in the camera's pixel convention (see
 *           set_pixel_convention()). Defaults to the center of the sensor.
 * @param cy Principal point y coordinate, likewise.
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

    set_principal_point_(cx, cy, resolution);

    compute_intrinsics_();
}

/**
 * @brief Configure the sensor using physical size and resolution.
 *
 * This method sets the sensor resolution, physical size, and principal point. It also computes the
 * intrinsics based on the new configuration.
 * @param resolution Sensor resolution
 * @param width Sensor width in millimeters
 * @param height Sensor height in millimeters
 * @param cx Principal point x coordinate, in the camera's pixel convention (see
 *           set_pixel_convention()). Defaults to the center of the sensor.
 * @param cy Principal point y coordinate, likewise.
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

    set_principal_point_(cx, cy, resolution);

    compute_intrinsics_();
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
 *
 * The principal point is read in the camera's pixel convention (see set_pixel_convention()),
 * so a calibration can be passed in the convention of the tool that produced it. fx and fy are
 * scales, and mean the same in every convention.
 *
 * @param fx Focal length in x direction
 * @param fy Focal length in y direction
 * @param cx Principal point x coordinate, in the camera's pixel convention
 * @param cy Principal point y coordinate, in the camera's pixel convention
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
    set_principal_point_(cx, cy, resolution);
    sensor_->set_resolution(resolution);
    focal_length_ = anchor_focal_length.to_si_f();

    // Use the anchor to compute the pixel_pitch/size
    units::Meter px(focal_length_ / fx_);
    units::Meter py(focal_length_ / fy_);
    sensor_->set_pixel_pitch(px, py);
    sensor_->set_sensor_size(px * resolution.x, py * resolution.y);

    compute_intrinsics_();
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
    update_focus_(); // the defocus kernel lives on the aperture
}

/**
 * @brief Set the point spread function (PSF) model for the camera.
 *
 * @tparam TPSF PSF model type
 * @tparam Args Constructor arguments for the PSF
 * @param args Arguments to construct the PSF
 */
template <IsSpectral TSpectral>
template <IsPSF TPSF, typename... Args>
void CameraModel<TSpectral>::set_psf(Args&&... args)
{
    psf_ = std::make_unique<TPSF>(std::forward<Args>(args)...);
    use_aperture_psf_ = false;
    invalidate_psf_kernels_();
}

/**
 * @brief Sets a measured (user-supplied) PSF as the core PSF of the camera.
 *
 * Convenience wrapper around set_psf<MeasuredPSF>() that is also exposed through the Python
 * bindings. See MeasuredPSF for the data conventions (centered measurement, sampling
 * density, extent limits).
 *
 * @param data Measured PSF samples, centered on the image.
 * @param samples_per_pixel Measurement samples per sensor pixel per axis.
 * @param radius Polyphase stamping kernel radius in sensor pixels (0 = auto).
 * @param banks Number of polyphase banks per axis for subpixel stamping.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_measured_psf(const Image<TSpectral>& data,
                                              float samples_per_pixel,
                                              int radius,
                                              int banks)
{
    this->set_psf<MeasuredPSF<TSpectral>>(data, samples_per_pixel, radius, banks);
}

/**
 * @brief Use the aperture to generate a PSF (point spread function).
 * @param radius PSF kernel radius
 * @param banks Number of PSF banks
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::use_aperture_psf(int radius, int banks)
{
    use_aperture_psf_ = true;
    units::Meter f(focal_length_);
    units::Meter px(sensor_->pixel_pitch().x);
    units::Meter py(sensor_->pixel_pitch().y);
    psf_ = aperture_->make_psf(f, px, py, radius, banks);
    invalidate_psf_kernels_();
}

/**
 * @brief Sets the radius of the whole-image PSF convolution kernel.
 *
 * The convolution kernel is a single centered kernel built via
 * PSF::generate_convolution_kernel() and is independent of the polyphase stamping cache, so
 * it may be much larger (e.g. spanning the full frame to model scattered-light wings). A
 * radius of 0 matches the polyphase radius.
 *
 * @param radius Convolution kernel radius in pixels (kernel dimension is 2 * radius + 1)
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_psf_convolution_radius(int radius)
{
    if (radius < 0) {
        HUIRA_THROW_ERROR(
            "CameraModel::set_psf_convolution_radius - Radius must be non-negative: " +
            std::to_string(radius));
    }
    if (radius != psf_convolution_radius_) {
        psf_convolution_radius_ = radius;
        invalidate_psf_kernels_();
    }
}

/**
 * @brief Returns the total-system PSF kernel for whole-image convolution.
 *
 * The total point spread function of the optical system is the energy-weighted sum of its
 * components: the diffraction-limited core (from the aperture or a user-provided PSF) and the
 * Harvey-Shack scattered-light wings. Veiling glare, the third component, is uniform across
 * the image and is applied separately by the renderer for efficiency. The kernel is built
 * lazily and cached; any change to the core PSF, convolution radius, or scatter parameters
 * invalidates it.
 *
 * @return Reference to the cached total-system convolution kernel (unit energy per channel).
 */
template <IsSpectral TSpectral>
const Image<TSpectral>& CameraModel<TSpectral>::get_psf_convolution_kernel()
{
    if (psf_ == nullptr && !scatter_enabled_) {
        HUIRA_THROW_ERROR("CameraModel::get_psf_convolution_kernel - No PSF or scatter model "
                          "has been set");
    }
    if (psf_ == nullptr && psf_convolution_radius_ <= 0) {
        HUIRA_THROW_ERROR("CameraModel::get_psf_convolution_kernel - "
                          "set_psf_convolution_radius() is required when scattering is enabled "
                          "without a core PSF");
    }

    if (!psf_convolution_kernel_valid_) {
        const int radius =
            (psf_convolution_radius_ > 0) ? psf_convolution_radius_ : psf_->get_radius();
        const int dim = 2 * radius + 1;
        HUIRA_LOG_INFO("CameraModel - Generating " + std::to_string(dim) + "x" +
                       std::to_string(dim) + " PSF convolution kernel");

        // Core component: the diffraction-limited (or user-provided) PSF. With no core PSF
        // set, the core is an ideal delta (perfect optics plus scatter):
        if (psf_ != nullptr) {
            psf_convolution_kernel_ = psf_->generate_convolution_kernel(radius);
        } else {
            psf_convolution_kernel_ = Image<TSpectral>(dim, dim, TSpectral{0.f});
            psf_convolution_kernel_(radius, radius) = TSpectral{1.f};
        }

        // Scattered-light wings: mixed with the core by energy fraction, so that the total
        // system PSF remains normalized to unit energy:
        //     psf_total = (1 - f_s) * core + f_s * wings
        if (scatter_enabled_ && scatter_fraction_ > 0.f) {
            HarveyShackScatter<TSpectral> scatter(scatter_falloff_exponent_, r0_, scatter_radius_);
            Image<TSpectral> wings = scatter.generate_convolution_kernel(radius);

            const float f_s = scatter_fraction_;
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
 * @brief Returns the scattered-light wings kernel alone, building it lazily if needed.
 *
 * This is the Harvey-Shack component of the total system PSF, normalized to unit energy and
 * NOT scaled by the scatter fraction. The renderer uses it to apply wings to unresolved
 * sources: their compact core is stamped via the polyphase cache, their raw energy is
 * splatted into a separate buffer, and that buffer is convolved with this kernel before the
 * two are blended by (1 - f_s) and f_s. Requires scattering to be enabled.
 *
 * @return Reference to the cached wings kernel (unit energy per channel).
 */
template <IsSpectral TSpectral>
const Image<TSpectral>& CameraModel<TSpectral>::get_psf_wings_kernel()
{
    if (!scatter_enabled_) {
        HUIRA_THROW_ERROR("CameraModel::get_psf_wings_kernel - Scattering is not enabled");
    }
    if (psf_ == nullptr && psf_convolution_radius_ <= 0) {
        HUIRA_THROW_ERROR("CameraModel::get_psf_wings_kernel - set_psf_convolution_radius() is "
                          "required when no core PSF is set");
    }

    if (!psf_wings_kernel_valid_) {
        const int radius =
            (psf_convolution_radius_ > 0) ? psf_convolution_radius_ : psf_->get_radius();
        HarveyShackScatter<TSpectral> scatter(scatter_falloff_exponent_, r0_, scatter_radius_);
        psf_wings_kernel_ = scatter.generate_convolution_kernel(radius);
        psf_wings_kernel_valid_ = true;
    }
    return psf_wings_kernel_;
}

/**
 * @brief Delete the PSF and disable aperture PSF usage.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::delete_psf()
{
    invalidate_psf_kernels_();
    psf_ = nullptr;
    use_aperture_psf_ = false;
}

/**
 * @brief Set the veiling glare alpha value.
 * @param alpha Veiling glare alpha (0 to 1)
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_veiling_glare(float alpha)
{
    if (alpha < 0.f || alpha > 1.f || std::isnan(alpha)) {
        HUIRA_THROW_ERROR("CameraModel::set_veiling_glare - Alpha must be in the range [0, 1]: " +
                          std::to_string(alpha));
    }
    veiling_alpha_ = alpha;
    veiling_glare_enabled_ = (alpha > 0.f);
}

/**
 * @brief Disable veiling glare effects.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::disable_veiling_glare()
{
    veiling_alpha_ = 0.f;
    veiling_glare_enabled_ = false;
}

/**
 * @brief Set Harvey-Shack scatter parameters.
 * @param scatter_fraction Fraction of light scattered (0 to 1)
 * @param falloff_exponent Exponent for scatter falloff (typically > 1)
 * @param r0 Radius at which scatter fraction is measured (default 0.5)
 * @param radius Maximum scatter radius in pixels (default 0, meaning infinite)
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_harvey_shack_scatter(float scatter_fraction,
                                                      float falloff_exponent,
                                                      float r0,
                                                      float radius)
{
    if (scatter_fraction < 0.f || scatter_fraction >= 1.f || std::isnan(scatter_fraction)) {
        HUIRA_THROW_ERROR("CameraModel::set_harvey_shack_scatter - Scatter fraction must be in "
                          "the range [0, 1): " +
                          std::to_string(scatter_fraction));
    }
    if (!(falloff_exponent > 0.f) || std::isnan(falloff_exponent)) {
        HUIRA_THROW_ERROR("CameraModel::set_harvey_shack_scatter - Falloff exponent must be a "
                          "positive finite value: " +
                          std::to_string(falloff_exponent));
    }
    if (!(r0 > 0.f) || std::isnan(r0)) {
        HUIRA_THROW_ERROR("CameraModel::set_harvey_shack_scatter - r0 must be a positive finite "
                          "value: " +
                          std::to_string(r0));
    }
    if (radius < 0.f || std::isnan(radius)) {
        HUIRA_THROW_ERROR("CameraModel::set_harvey_shack_scatter - Radius must be non-negative: " +
                          std::to_string(radius));
    }
    scatter_fraction_ = scatter_fraction;
    scatter_falloff_exponent_ = falloff_exponent;
    r0_ = r0;
    scatter_radius_ = radius;
    scatter_enabled_ = (scatter_fraction > 0.f);
    invalidate_psf_kernels_();
}

/**
 * @brief Disable Harvey-Shack scatter effects.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::disable_harvey_shack_scatter()
{
    scatter_fraction_ = 0.f;
    scatter_falloff_exponent_ = 2.f;
    r0_ = 0.5f;
    scatter_radius_ = 0.f;
    scatter_enabled_ = false;
    invalidate_psf_kernels_();
}

/**
 * @brief Focus the camera at a distance.
 *
 * Any value other than zero or NaN is accepted. Positive values focus in front of the camera,
 * and infinity of either sign focuses at infinity. Negative values focus past infinity: the
 * rays then diverge from a virtual point that distance behind the camera.
 *
 * Equivalent to set_focus_diopters() with the reciprocal. Either way the camera stays focused
 * at this distance if the focal length later changes, and focus_distance() returns exactly
 * this value (with infinity of either sign returned as +inf).
 *
 * @param focus_distance Focus distance (any length unit).
 * @throws std::runtime_error if the distance is NaN or closer to zero than 1e-12 m.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_focus_distance(units::Meter focus_distance)
{
    const double distance = focus_distance.to_si();
    if (!(std::abs(distance) >= MIN_FOCUS_DISTANCE_)) {
        HUIRA_THROW_ERROR("CameraModel::set_focus_distance - Focus distance must not be NaN or "
                          "closer to zero than 1e-12 m: " +
                          std::to_string(distance) + " m");
    }
    set_focus_(FocusReference::Distance,
               std::isinf(distance) ? std::numeric_limits<double>::infinity() : distance);
}

/**
 * @brief Focus the camera by the vergence of the light it brings to focus.
 *
 * The reciprocal of the focus distance: 0 focuses at infinity, positive values focus in front
 * of the camera, and negative values focus past infinity.
 *
 * @param diopters Focus vergence (diopters, i.e. reciprocal meters).
 * @throws std::runtime_error if the vergence is not finite, or corresponds to a focus distance
 *         closer to zero than 1e-12 m.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_focus_diopters(units::Diopter diopters)
{
    const double vergence = diopters.to_si();
    if (!(std::abs(vergence) <= 1.0 / MIN_FOCUS_DISTANCE_)) {
        HUIRA_THROW_ERROR("CameraModel::set_focus_diopters - Diopters must be finite and at most "
                          "1e12 in magnitude: " +
                          std::to_string(vergence));
    }
    set_focus_(FocusReference::Diopters, vergence);
}

/**
 * @brief Focus the camera by moving the sensor away from the infinity-focus position.
 *
 * The offset is measured along the optical axis, from the plane where the lens focuses an
 * object at infinity. 0 focuses at infinity, positive values (sensor farther from the lens)
 * focus in front of the camera, and negative values focus past infinity. The focus distance
 * follows from the thin lens equation, 1/f = 1/distance + 1/(f + offset).
 *
 * Unlike the other focus setters this one is image-side, so the offset is what is held fixed
 * if the focal length later changes, and the focus distance moves with it.
 *
 * A point at infinity blurs to a disk of diameter close to |offset| / N, for f-number N. The
 * camera's projection does not change with focus, so for offsets that are a sizeable fraction
 * of the focal length this differs from a physical lens by roughly a factor f / (f + offset).
 *
 * @param offset Sensor offset (any length unit).
 * @throws std::runtime_error if the offset is not finite, or puts the sensor at the lens.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_focus_sensor_offset(units::Micrometer offset)
{
    const double offset_m = offset.to_si();
    const double vergence =
        resolve_focus_diopters_(FocusReference::SensorOffset, offset_m, focal_length_);
    if (!std::isfinite(offset_m) || !(std::abs(vergence) <= 1.0 / MIN_FOCUS_DISTANCE_)) {
        HUIRA_THROW_ERROR("CameraModel::set_focus_sensor_offset - Offset must be finite and must "
                          "not put the sensor at the lens: " +
                          std::to_string(offset_m * 1e6) + " um, with a focal length of " +
                          std::to_string(focal_length_ * 1e3) + " mm");
    }
    set_focus_(FocusReference::SensorOffset, offset_m);
}

/// Get the distance the camera is focused at: negative past infinity, +inf at infinity.
template <IsSpectral TSpectral>
units::Meter CameraModel<TSpectral>::focus_distance() const
{
    if (focus_reference_ == FocusReference::Distance) {
        return units::Meter(focus_setting_);
    }
    const double vergence = focus_diopters().to_si();
    return units::Meter(vergence == 0.0 ? std::numeric_limits<double>::infinity() : 1.0 / vergence);
}

/// Get the focus as a vergence: the reciprocal of focus_distance(), 0 at infinity.
template <IsSpectral TSpectral>
units::Diopter CameraModel<TSpectral>::focus_diopters() const
{
    return units::Diopter(resolve_focus_diopters_(focus_reference_, focus_setting_, focal_length_));
}

/**
 * @brief Get the focus as a sensor offset from the infinity-focus position.
 *
 * See set_focus_sensor_offset(). For a focus distance of exactly one focal length the offset
 * is infinite, and for shorter distances, which a physical lens cannot focus on, it is below
 * -f (the thin lens image is virtual). Those values still round-trip through
 * set_focus_sensor_offset().
 */
template <IsSpectral TSpectral>
units::Micrometer CameraModel<TSpectral>::focus_sensor_offset() const
{
    if (focus_reference_ == FocusReference::SensorOffset) {
        return units::Meter(focus_setting_);
    }
    // Thin lens, with the image at f + offset: offset = f^2 / (distance - f).
    const double f = focal_length_;
    if (focus_reference_ == FocusReference::Distance) {
        return units::Meter(std::isinf(focus_setting_) ? 0.0 : f * f / (focus_setting_ - f));
    }
    const double vergence = focus_setting_;
    return units::Meter(f * f * vergence / (1.0 - f * vergence));
}

/**
 * @brief Get the radius, in pixels, of the defocus blur applied to unresolved sources.
 *
 * This is the blur of a point at infinity for the current focus, aperture, focal length and
 * pixel pitch. It is 0 when the blur is under half a pixel, which is treated as in focus.
 */
template <IsSpectral TSpectral>
float CameraModel<TSpectral>::defocus_blur_radius() const
{
    return aperture_->get_defocus_radius();
}

/**
 * @brief Convert a focus setting to a vergence (diopters) for the given focal length.
 *
 * A distance is inverted (infinity giving 0). A sensor offset is converted with the thin lens
 * equation, with the sensor at f + offset: 1/distance = 1/f - 1/(f + offset).
 */
template <IsSpectral TSpectral>
double CameraModel<TSpectral>::resolve_focus_diopters_(FocusReference reference,
                                                       double setting,
                                                       double focal_length)
{
    switch (reference) {
    case FocusReference::Distance:
        return std::isinf(setting) ? 0.0 : 1.0 / setting;
    case FocusReference::Diopters:
        return setting;
    case FocusReference::SensorOffset:
        return setting / (focal_length * (focal_length + setting));
    default:
        return setting; // unreachable: every reference is handled above
    }
}

/**
 * @brief Store an already validated focus setting and apply it.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_focus_(FocusReference reference, double setting)
{
    focus_reference_ = reference;
    focus_setting_ = (setting == 0.0) ? 0.0 : setting; // no negative zero
    update_focus_();
}

/**
 * @brief Resolve the focus setting and rebuild the defocus kernel for unresolved sources.
 *
 * Must run whenever the focus setting or anything the defocus blur depends on changes: the
 * focal length and pixel pitch (compute_intrinsics_() calls this), and the aperture.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::update_focus_()
{
    const double vergence = focus_diopters().to_si();

    // Setters validate their input, so this only fails for a sensor offset whose focal length
    // has since changed to put the sensor at the lens:
    if (!(std::abs(vergence) <= 1.0 / MIN_FOCUS_DISTANCE_)) {
        HUIRA_THROW_ERROR("CameraModel - The focus sensor offset of " +
                          std::to_string(focus_setting_ * 1e6) +
                          " um puts the sensor at the lens for a focal length of " +
                          std::to_string(focal_length_ * 1e3) + " mm");
    }

    if (focus_reference_ == FocusReference::Distance) {
        d_ = static_cast<float>(focus_setting_); // exactly as given
    } else {
        d_ = (vergence == 0.0) ? std::numeric_limits<float>::infinity()
                               : static_cast<float>(1.0 / vergence);
    }

    units::Meter pitch_x(sensor_->pixel_pitch().x);
    units::Meter pitch_y(sensor_->pixel_pitch().y);
    aperture_->build_defocus_kernel(
        units::Diopter(vergence), units::Meter(focal_length_), pitch_x, pitch_y, 16);
}

/**
 * @brief Project a 3D point in camera coordinates onto the image plane.
 *
 * Uses the pinhole camera model and applies distortion if present.
 * @param point_camera_coords 3D point in camera coordinates (meters)
 * @return Pixel 2D point on the image plane, in the camera's pixel convention (see
 * set_pixel_convention()).
 */
template <IsSpectral TSpectral>
Pixel CameraModel<TSpectral>::project_point(const Vec3<float>& point_camera_coords) const
{
    return pixel_convention_.from_sensor(project_to_sensor_(point_camera_coords),
                                         sensor_->resolution());
}

/**
 * @brief Project a point in camera coordinates to sensor coordinates (pixel i covers
 * [i, i + 1), y down), whatever the pixel convention.
 */
template <IsSpectral TSpectral>
Pixel CameraModel<TSpectral>::project_to_sensor_(const Vec3<float>& point_camera_coords) const
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

/**
 * @brief Cast a camera ray through a position on the image, sampling the aperture when depth
 * of field is enabled.
 *
 * @param pixel Position in the camera's pixel convention (see set_pixel_convention()).
 * @param sampler Sampler for the aperture position.
 */
template <IsSpectral TSpectral>
Ray<TSpectral> CameraModel<TSpectral>::cast_ray(const Pixel& pixel, Sampler<float>& sampler) const
{
    return sensor_ray_(pixel_convention_.to_sensor(pixel, sensor_->resolution()), sampler);
}

/**
 * @brief Cast a pinhole camera ray through a position on the image.
 *
 * @param pixel Position in the camera's pixel convention (see set_pixel_convention()).
 */
template <IsSpectral TSpectral>
Ray<TSpectral> CameraModel<TSpectral>::cast_ray(const Pixel& pixel) const
{
    return sensor_ray_(pixel_convention_.to_sensor(pixel, sensor_->resolution()));
}

/**
 * @brief Cast a pinhole camera ray through integer coordinates (x, y) in the camera's pixel
 * convention: with the default, PixelConvention::opencv(), the center of pixel (x, y).
 */
template <IsSpectral TSpectral>
Ray<TSpectral> CameraModel<TSpectral>::cast_ray(int x, int y) const
{
    return cast_ray(Pixel{static_cast<float>(x), static_cast<float>(y)});
}

/**
 * @brief cast_ray() in sensor coordinates (pixel i covers [i, i + 1), y down), for internal
 * use: the renderer samples pixels in sensor coordinates.
 */
template <IsSpectral TSpectral>
Ray<TSpectral> CameraModel<TSpectral>::sensor_ray_(const Pixel& pixel,
                                                   Sampler<float>& sampler) const
{
    assert(pixel[0] >= 0 && pixel[0] < rx_ && pixel[1] >= 0 && pixel[1] < ry_);

    Vec3<float> origin{0, 0, 0};
    Vec3<float> direction = ray_direction_(pixel);

    if (depth_of_field_) {
        Vec2<float> aperture_sample = aperture_->sample(sampler);
        Vec3<float> aperture_point{aperture_sample.x, aperture_sample.y, 0.f};

        if (!std::isinf(d_)) {
            // The thin-lens focal surface is the plane at axial distance d_, so the pixel's
            // focal point is found by scaling its direction to unit axial length first. The
            // distortion field stores unit vectors, which would otherwise place it on a
            // sphere of radius d_ instead.
            Vec3<float> focal_point = direction * (d_ / std::abs(direction.z));
            direction = focal_point - aperture_point;

            // Focus past infinity: the focal point is a virtual point behind the camera, and
            // the rays diverge from it rather than converging on it, so the direction is the
            // reverse of the vector towards it.
            if (d_ < 0.f) {
                direction = -direction;
            }
        }
        origin = aperture_point;
    }

    return Ray<TSpectral>{origin, glm::normalize(direction)};
}

/**
 * @brief Pinhole cast_ray() in sensor coordinates, for internal use.
 */
template <IsSpectral TSpectral>
Ray<TSpectral> CameraModel<TSpectral>::sensor_ray_(const Pixel& pixel) const
{
    // Pixel coordinates are continuous and the sensor spans [0, rx_] x [0, ry_], so the far
    // edge is a valid position. Region culling relies on this: it samples tile corners, and
    // the last tile's far corner lies exactly on that edge.
    assert(pixel[0] >= 0 && pixel[0] <= rx_ && pixel[1] >= 0 && pixel[1] <= ry_);

    return Ray<TSpectral>{Vec3<float>{0, 0, 0}, glm::normalize(ray_direction_(pixel))};
}

/**
 * @brief Direction (not normalized) of the pinhole ray through a sensor position.
 *
 * Without distortion this is computed exactly. With distortion it is interpolated from the
 * precomputed directions at every pixel corner, which span the whole sensor, so positions
 * anywhere in [0, width] x [0, height] are interpolated rather than clamped.
 */
template <IsSpectral TSpectral>
Vec3<float> CameraModel<TSpectral>::ray_direction_(const Pixel& pixel) const
{
    if (!distortion_) {
        return pixel_to_direction_<float>(pixel);
    }
    return distortion_field_.sample_bilinear<WrapMode::Clamp>(pixel[0] / rx_, pixel[1] / ry_);
}

template <IsSpectral TSpectral>
float CameraModel<TSpectral>::pixel_radiance_to_power(int x, int y) const
{
    Ray<TSpectral> ray = sensor_ray_(Pixel{static_cast<float>(x), static_cast<float>(y)});
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
    update_focus_();
    if (use_aperture_psf_) {
        units::Meter f(focal_length_);
        units::Meter px(sensor_->pixel_pitch().x);
        units::Meter py(sensor_->pixel_pitch().y);
        psf_ = aperture_->make_psf(f, px, py, psf_->get_radius(), psf_->get_banks());
        invalidate_psf_kernels_();
    }
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

    // The principal point as given is in the pixel convention; everything below works in sensor
    // coordinates. Unset, it is the center of the sensor, which is the same in every convention.
    const Pixel given{principal_x_.value_or(0.f), principal_y_.value_or(0.f)};
    const Pixel sensor = pixel_convention_.to_sensor(given, sensor_->resolution());
    cx_ = principal_x_ ? sensor.x : rx_ * 0.5f;
    cy_ = principal_y_ ? sensor.y : ry_ * 0.5f;

    // The frustum reads ray directions from the distortion field, so the field has to be
    // current first:
    compute_distortion_field_();
    compute_frustum_();
    compute_pixel_solid_angles_();

    // Every focal length and pixel pitch change comes through here, and focus depends on both:
    update_focus_();
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
    if (!distortion_) {
        distortion_field_ = Image<Vec3<float>>(0, 0);
        return;
    }

    // One entry per pixel corner, so the field reaches the far edges of the last column and
    // row; with one per pixel, lookups beyond the last pixel's left/top edge were clamped.
    const Resolution res = sensor_->resolution();
    distortion_field_ = Image<Vec3<float>>(res.x + 1, res.y + 1, Vec3<float>{0, 0, 0});

    tbb::parallel_for(
        tbb::blocked_range<int>(0, res.y + 1), [&](const tbb::blocked_range<int>& rows) {
            for (int y = rows.begin(); y < rows.end(); ++y) {
                for (int x = 0; x <= res.x; ++x) {
                    Pixel pixel{static_cast<float>(x), static_cast<float>(y)};
                    distortion_field_(x, y) = glm::normalize(pixel_to_direction_<float>(pixel));
                }
            }
        });
}

template <IsSpectral TSpectral>
void CameraModel<TSpectral>::compute_pixel_solid_angles_()
{
    Resolution res = sensor_->resolution();

    pixel_solid_angles_ = Image<float>(res, 0.f);
    tbb::parallel_for(tbb::blocked_range<int>(0, res.y), [&](const tbb::blocked_range<int>& rows) {
        for (int y = rows.begin(); y < rows.end(); ++y) {
            for (int x = 0; x < res.x; ++x) {
                // Calculate normalized directions to pixel corners
                Vec3<double> c0 = glm::normalize(pixel_to_direction_<double>(
                    Pixel{static_cast<float>(x), static_cast<float>(y)}));
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
    });
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

    // The side planes pass through the optical center and contain the image's y axis (left and
    // right) or x axis (top and bottom). For the whole sensor to be inside, each has to pass
    // through the boundary point that reaches furthest out on its side, measured as the
    // tangent of its angle from the forward axis. Every pixel corner on the boundary is a
    // candidate, out to the far edges at x = width and y = height. (The most oblique point
    // along each edge is not enough: with pincushion distortion the middle of an edge can reach
    // further out than its corners.)
    constexpr float INF_ = std::numeric_limits<float>::infinity();
    float min_tan_x = INF_;
    float max_tan_x = -INF_;
    float min_tan_y = INF_;
    float max_tan_y = -INF_;
    Vec3<float> left_extrema{0, 0, 0};
    Vec3<float> right_extrema{0, 0, 0};
    Vec3<float> top_extrema{0, 0, 0};
    Vec3<float> bottom_extrema{0, 0, 0};

    auto visit = [&](int x, int y) {
        const Vec3<float> direction =
            glm::normalize(ray_direction_(Pixel{static_cast<float>(x), static_cast<float>(y)}));
        const float forward = glm::dot(direction, zdir);
        const float tan_x = glm::dot(direction, xdir) / forward;
        const float tan_y = glm::dot(direction, ydir) / forward;
        if (tan_x < min_tan_x) {
            min_tan_x = tan_x;
            left_extrema = direction;
        }
        if (tan_x > max_tan_x) {
            max_tan_x = tan_x;
            right_extrema = direction;
        }
        if (tan_y < min_tan_y) {
            min_tan_y = tan_y;
            top_extrema = direction;
        }
        if (tan_y > max_tan_y) {
            max_tan_y = tan_y;
            bottom_extrema = direction;
        }
    };

    for (int x = 0; x <= res.x; ++x) {
        visit(x, 0);
        visit(x, res.y);
    }
    for (int y = 1; y < res.y; ++y) {
        visit(0, y);
        visit(res.x, y);
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
