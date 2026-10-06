
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "huira/cameras/apertures/circular_aperture.hpp"
#include "huira/cameras/psfs/harvey_shack_scatter.hpp"
#include "huira/cameras/psfs/measured_psf.hpp"
#include "huira/cameras/sensors/simple_sensor.hpp"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/task_arena.h"

namespace huira {
namespace detail {
/// A number as people write it: 50, 6.25, 2.8 (not 50.000000).
inline std::string short_number(double value)
{
    std::ostringstream out;
    out.precision(4);
    out << value;
    return out.str();
}

/// A position on the image, in full: 4095.5, not 4096 (as short_number() would round it).
inline std::string position_number(double value)
{
    std::ostringstream out;
    out.precision(10);
    out << value;
    return out.str();
}
} // namespace detail

/**
 * @brief Construct a new CameraModel with default sensor and aperture.
 *
 * Initializes the camera with a 50 mm focal length, a SimpleSensor, and a CircularAperture at
 * f/2.8, which it stays at until the aperture is set (see set_fstop()).
 */
template <IsSpectral TSpectral>
CameraModel<TSpectral>::CameraModel()
{
    HUIRA_TRACE_SCOPE("CameraModel::CameraModel()");
    units::Meter diameter(this->focal_length_ / DEFAULT_FSTOP_);
    this->sensor_ = std::make_unique<SimpleSensor<TSpectral>>();
    this->aperture_ = std::make_unique<CircularAperture<TSpectral>>(diameter);
    compute_intrinsics_(); // principal point unset: the center of the sensor
}

/**
 * @brief Set the focal length of the camera (in millimeters).
 *
 * Updates the camera intrinsics. What happens to the aperture depends on how it was set:
 * - Not set: the camera stays f/2.8, and the aperture's diameter follows the focal length.
 * - set_fstop(): the diameter is kept, so the f-number changes, and a warning is logged the
 *   first time. Call set_fstop() after set_focal_length() to get the f-number asked for.
 * - set_aperture_diameter() or set_aperture(): the diameter is kept, as asked.
 *
 * @param focal_length Focal length in millimeters
 * @throws std::runtime_error if the focal length is not positive and finite, or if the focus is
 *         a sensor offset that would put the sensor at the lens at this focal length.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_focal_length(units::Millimeter focal_length)
{
    const float f = focal_length.to_si_f();
    check_focal_length_(f, "CameraModel::set_focal_length");

    is_explicit_matrix_ = false;
    focal_length_ = f;
    compute_intrinsics_();
}

/**
 * @brief Checks a focal length before it is stored.
 *
 * It must be positive and finite, and must leave the focus valid: a focus given as a sensor
 * offset is re-resolved at the new focal length, and must not put the sensor at the lens.
 *
 * @param focal_length Focal length in meters.
 * @param caller Name for the error message.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::check_focal_length_(float focal_length,
                                                 const std::string& caller) const
{
    if (!(focal_length > 0.f) || !std::isfinite(focal_length)) {
        HUIRA_THROW_ERROR(caller + " - Focal length must be a positive finite value: " +
                          std::to_string(focal_length) + " m");
    }
    const double vergence = resolve_focus_diopters_(
        focus_reference_, focus_setting_, static_cast<double>(focal_length));
    if (!(std::abs(vergence) <= 1.0 / MIN_FOCUS_DISTANCE_)) {
        HUIRA_THROW_ERROR(caller + " - The focus sensor offset of " +
                          detail::short_number(focus_setting_ * 1e6) +
                          " um would put the sensor at the lens with a focal length of " +
                          detail::short_number(static_cast<double>(focal_length) * 1e3) +
                          " mm. Change the focus first.");
    }
}

/**
 * @brief Set the distortion model for the camera.
 *
 * The distortion has to have an inverse over the whole image: every position on the sensor has
 * to be where some ray lands. A model can fail that at the edges of a wide field, where it folds
 * over or stops short of the image's corners, which depends on the focal length and sensor as
 * well as the coefficients. Since those can be set in any order, a failure is not reported
 * here but by precompute(), a render, or cast_ray() and pixel_radiance_to_power() at the
 * position concerned, until a setting changes that fixes it.
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
    ++distortion_version_;
    compute_intrinsics_();
}

/**
 * @brief Delete the current distortion model.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::delete_distortion()
{
    if (distortion_) {
        distortion_ = nullptr;
        ++distortion_version_;
    }
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
 * @brief Check a principal point before it is stored (unset means the sensor's center).
 *
 * Throws if it is not finite, and warns if it is far outside the sensor.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::check_principal_point_(std::optional<float> cx,
                                                    std::optional<float> cy,
                                                    const Resolution& resolution,
                                                    const std::string& caller)
{
    if ((cx && !std::isfinite(*cx)) || (cy && !std::isfinite(*cy))) {
        HUIRA_THROW_ERROR(caller + " - Principal point (cx, cy) must be finite numeric values.");
    }

    const float width = static_cast<float>(resolution.x);
    const float height = static_cast<float>(resolution.y);
    if ((cx && (*cx < -width || *cx > 2.f * width)) ||
        (cy && (*cy < -height || *cy > 2.f * height))) {
        HUIRA_LOG_WARNING("Principal point is significantly outside the sensor resolution. Ensure "
                          "this is intended, e.g. for an off-axis projection.");
    }
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
 * The new sensor brings its own resolution and pixel pitch. If they are the old sensor's (a new
 * noise model for the same sensor, say), the intrinsics are kept: the principal point, skew and
 * any intrinsic matrix. If not, those were set for a different sensor and no longer apply, so
 * they are reset, with a warning: the principal point to the center of the new sensor, the skew
 * to 0, and fx and fy to the focal length over the new pitch. The focal length, aperture,
 * distortion and focus belong to the lens and are kept either way.
 *
 * @tparam TSensor Sensor model type
 * @tparam Args Constructor arguments for the sensor
 * @param args Arguments to construct the sensor
 */
template <IsSpectral TSpectral>
template <IsSensor<TSpectral> TSensor, typename... Args>
void CameraModel<TSpectral>::set_sensor(Args&&... args)
{
    auto sensor = std::make_unique<TSensor>(std::forward<Args>(args)...);

    const Resolution old_resolution = sensor_->resolution();
    const Resolution new_resolution = sensor->resolution();
    const Vec2<float> old_pitch = sensor_->pixel_pitch();
    const Vec2<float> new_pitch = sensor->pixel_pitch();
    auto same = [](float a, float b) { return std::abs(a - b) <= 1e-6f * std::max(a, b); };
    const bool same_geometry = new_resolution.x == old_resolution.x &&
                               new_resolution.y == old_resolution.y &&
                               same(new_pitch.x, old_pitch.x) && same(new_pitch.y, old_pitch.y);
    const bool intrinsics_set =
        principal_x_ || principal_y_ || shear_ != 0.f || is_explicit_matrix_;

    if (!same_geometry && intrinsics_set) {
        auto describe = [](const Resolution& resolution, const Vec2<float>& pitch) {
            return std::to_string(resolution.x) + "x" + std::to_string(resolution.y) +
                   " pixels of " + detail::short_number(static_cast<double>(pitch.x) * 1e6) +
                   " x " + detail::short_number(static_cast<double>(pitch.y) * 1e6) + " um";
        };
        HUIRA_LOG_WARNING("CameraModel::set_sensor - The new sensor has " +
                          describe(new_resolution, new_pitch) + ", not " +
                          describe(old_resolution, old_pitch) +
                          ", so the principal point, skew and intrinsic matrix set for the old "
                          "one have been reset: the principal point to the center of the new "
                          "sensor, the skew to 0, and fx and fy to the focal length over the "
                          "pitch. Set them again for the new sensor if needed.");
        principal_x_.reset();
        principal_y_.reset();
        shear_ = 0.f;
        is_explicit_matrix_ = false;
    }

    sensor_ = std::move(sensor);
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
    // Everything is checked before anything is changed:
    const std::string caller = "CameraModel::configure_sensor_from_pitch";
    const units::Micrometer pitch_y_value = pitch_y.value_or(pitch_x);
    SensorModel<TSpectral>::check_resolution(resolution, caller);
    SensorModel<TSpectral>::check_pixel_pitch(pitch_x, pitch_y_value, caller);
    check_principal_point_(cx, cy, resolution, caller);

    is_explicit_matrix_ = false;
    sensor_->set_resolution(resolution);
    sensor_->set_pixel_pitch(pitch_x, pitch_y_value);
    principal_x_ = cx;
    principal_y_ = cy;
    shear_ = 0.f; // rectangular pixels

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
    // Everything is checked before anything is changed. Without a height the pixels are
    // square, so the height follows from the width and the resolution:
    const std::string caller = "CameraModel::configure_sensor_from_size";
    SensorModel<TSpectral>::check_resolution(resolution, caller);
    SensorModel<TSpectral>::check_sensor_size(width, height.value_or(width), caller);
    const units::Meter pitch_x(width.to_si() / static_cast<double>(resolution.x));
    const units::Meter pitch_y(height ? height->to_si() / static_cast<double>(resolution.y)
                                      : pitch_x.to_si());
    SensorModel<TSpectral>::check_pixel_pitch(pitch_x, pitch_y, caller);
    check_principal_point_(cx, cy, resolution, caller);

    is_explicit_matrix_ = false;
    sensor_->set_resolution(resolution);
    sensor_->set_pixel_pitch(pitch_x, pitch_y);
    principal_x_ = cx;
    principal_y_ = cy;
    shear_ = 0.f; // rectangular pixels

    compute_intrinsics_();
}

/**
 * @brief Set the intrinsics from a camera matrix.
 *
 * The matrix maps normalized image coordinates to pixel coordinates, and is upper triangular:
 *
 *     | fx  s  cx |
 *     |  0  fy cy |
 *     |  0  0  1  |
 *
 * Like any homogeneous matrix it is defined up to scale, so a matrix whose bottom-right entry
 * is not 1 is divided through by it.
 *
 * Mat3 is GLM's column-major matrix, indexed K[column][row], so cx is K[2][0], cy is K[2][1]
 * and s is K[1][0]. GLM's constructor also takes the values a column at a time:
 * Mat3<float>{fx, 0, 0, s, fy, 0, cx, cy, 1}. (From Python, pass the matrix as written above,
 * as a row-major 3x3 array.) See set_intrinsics() for what the values mean.
 *
 * @param intrinsic_matrix The camera matrix.
 * @param resolution Sensor resolution
 * @param anchor_focal_length Anchor focal length in millimeters
 * @throws std::runtime_error if the matrix is not upper triangular (most likely, it was filled
 *         one row at a time and is transposed), its bottom-right entry is 0, or as
 *         set_intrinsics().
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_intrinsic_matrix(const Mat3<float>& intrinsic_matrix,
                                                  const Resolution& resolution,
                                                  units::Millimeter anchor_focal_length)
{
    const float w = intrinsic_matrix[2][2];
    if (!std::isfinite(w) || w == 0.f) {
        HUIRA_THROW_ERROR("CameraModel::set_intrinsic_matrix - The bottom-right entry of the "
                          "matrix must be finite and non-zero: " +
                          std::to_string(w));
    }
    const Mat3<float> k = intrinsic_matrix / w;

    // The entries below the diagonal are 0 in any camera matrix. With them, the matrix would be
    // a general homography, which no calibration produces. The bottom row multiplies normalized
    // coordinates, and the entry under fx multiplies pixels, so each has its own scale:
    const float pixel_scale = std::max({std::abs(k[0][0]), std::abs(k[1][1]), 1.f});
    if (!(std::abs(k[0][2]) <= 1e-6f) || !(std::abs(k[1][2]) <= 1e-6f) ||
        !(std::abs(k[0][1]) <= 1e-6f * pixel_scale)) {
        HUIRA_THROW_ERROR("CameraModel::set_intrinsic_matrix - The matrix must be upper "
                          "triangular, [[fx, s, cx], [0, fy, cy], [0, 0, 1]] up to scale. If "
                          "cx and cy are in its bottom row, it is transposed: MATLAB's "
                          "IntrinsicMatrix property is (its K property is not), and so is a "
                          "Mat3 filled one row at a time in C++, since Mat3 is column-major "
                          "(GLM), indexed K[column][row].");
    }

    this->set_intrinsics(
        k[0][0], k[1][1], k[2][0], k[2][1], resolution, anchor_focal_length, k[1][0]);
}

/**
 * @brief Set the intrinsic parameters for the camera.
 *
 * A point at normalized image coordinates (x, y) projects to pixel
 * (fx * x + skew * y + cx, fy * y + cy).
 *
 * The principal point is read in the camera's pixel convention (see set_pixel_convention()),
 * so a calibration can be passed in the convention of the tool that produced it. fx and fy are
 * scales, and mean the same in every convention. The skew does too, except that a convention
 * counting y up from the bottom row flips its sign.
 *
 * The skew is 0 for pixels with perpendicular axes, which is what most calibration tools
 * assume (OpenCV, for one, never estimates it). It is kept if the focal length changes later,
 * as a fraction of fx; configure_sensor_from_pitch() and configure_sensor_from_size() describe
 * rectangular pixels and set it back to 0.
 *
 * The anchor focal length sets the physical scale: the pixel pitch is the anchor focal length
 * divided by fx and fy. Like set_focal_length(), it changes the focal length, with the same
 * effect on the aperture.
 *
 * @param fx Focal length in x direction, in pixels
 * @param fy Focal length in y direction, in pixels
 * @param cx Principal point x coordinate, in the camera's pixel convention
 * @param cy Principal point y coordinate, in the camera's pixel convention
 * @param resolution Sensor resolution
 * @param anchor_focal_length Anchor focal length in millimeters
 * @param skew Skew, in pixels; 0 for perpendicular pixel axes.
 * @throws std::runtime_error if fx or fy is not positive and finite, the principal point or
 *         skew is not finite, the resolution is not at least 1x1, or the anchor focal length is
 *         not valid (see set_focal_length()).
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_intrinsics(float fx,
                                            float fy,
                                            float cx,
                                            float cy,
                                            const Resolution& resolution,
                                            units::Millimeter anchor_focal_length,
                                            float skew)
{
    // Everything is checked before anything is changed:
    const std::string caller = "CameraModel::set_intrinsics";
    if (!(fx > 0.f) || !std::isfinite(fx) || !(fy > 0.f) || !std::isfinite(fy)) {
        HUIRA_THROW_ERROR(caller + " - fx and fy must be positive finite values: " +
                          std::to_string(fx) + ", " + std::to_string(fy) +
                          ". (For an image whose y axis points up, keep fy positive and set a "
                          "bottom-left pixel convention; see set_pixel_convention().)");
    }
    if (!std::isfinite(skew)) {
        HUIRA_THROW_ERROR(caller + " - Skew must be finite: " + std::to_string(skew));
    }
    SensorModel<TSpectral>::check_resolution(resolution, caller);
    check_principal_point_(cx, cy, resolution, caller);
    const float f = anchor_focal_length.to_si_f();
    check_focal_length_(f, caller);

    // The anchor gives the pixel pitch:
    const units::Meter pitch_x(static_cast<double>(f) / static_cast<double>(fx));
    const units::Meter pitch_y(static_cast<double>(f) / static_cast<double>(fy));
    SensorModel<TSpectral>::check_pixel_pitch(pitch_x, pitch_y, caller);

    is_explicit_matrix_ = true;
    fx_ = fx;
    fy_ = fy;
    shear_ = skew / fx;
    principal_x_ = cx;
    principal_y_ = cy;
    sensor_->set_resolution(resolution);
    sensor_->set_pixel_pitch(pitch_x, pitch_y);
    focal_length_ = f;

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
 * @brief Get the pixel pitch.
 * @return The pitch along x and along y.
 */
template <IsSpectral TSpectral>
std::pair<units::Micrometer, units::Micrometer> CameraModel<TSpectral>::pixel_pitch() const
{
    const Vec2<float> pitch = sensor_->pixel_pitch();
    return {units::Meter(static_cast<double>(pitch.x)), units::Meter(static_cast<double>(pitch.y))};
}

/**
 * @brief Get the camera matrix K = [[fx, s, cx], [0, fy, cy], [0, 0, 1]], as set_intrinsics() and
 * set_intrinsic_matrix() take it: in pixels, with the principal point (cx, cy) in the camera's
 * pixel convention (see set_pixel_convention()).
 *
 * Mat3 is column-major (GLM), so it is indexed K[column][row]: cx is K[2][0] and cy K[2][1].
 */
template <IsSpectral TSpectral>
Mat3<float> CameraModel<TSpectral>::intrinsic_matrix() const
{
    const Pixel principal = pixel_convention_.from_sensor(Pixel{cx_, cy_}, sensor_->resolution());
    Mat3<float> k{1.f}; // identity
    k[0][0] = fx_;
    k[1][1] = fy_;
    k[1][0] = shear_ * fx_; // the skew as given, in the pixel convention
    k[2][0] = principal.x;
    k[2][1] = principal.y;
    return k;
}

/**
 * @brief Set the aperture model for the camera.
 *
 * The aperture's size is kept if the focal length later changes, so the f-number follows the
 * focal length. See set_fstop().
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
    aperture_source_ = ApertureSource::Diameter;
    optics_changed_(OpticsInput::Aperture);
}

/**
 * @brief Set the point spread function (PSF) model for the camera.
 *
 * Replaces the aperture's PSF, if it was in use (see use_aperture_psf()). A PSF that does not
 * size its own stamps for unresolved sources (with PSF::set_polyphase_size(), as AiryDisk and
 * MeasuredPSF do; HarveyShackScatter does not) gets the default size, DEFAULT_PSF_RADIUS and
 * DEFAULT_PSF_BANKS.
 *
 * @tparam TPSF PSF model type
 * @tparam Args Constructor arguments for the PSF
 * @param args Arguments to construct the PSF
 */
template <IsSpectral TSpectral>
template <IsPSF TPSF, typename... Args>
void CameraModel<TSpectral>::set_psf(Args&&... args)
{
    auto psf = std::make_unique<TPSF>(std::forward<Args>(args)...);
    if (psf->get_banks() < 1) {
        psf->set_polyphase_size(DEFAULT_PSF_RADIUS, DEFAULT_PSF_BANKS);
    }
    psf_ = std::move(psf);
    use_aperture_psf_ = false;
    optics_changed_(OpticsInput::CorePSF);
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
 * @param sampling What the samples are: the PSF's intensity at points, or the light a pixel
 *                 centered there receives, such as a star image taken with the sensor itself
 *                 (see PSFSampling).
 * @param radius Polyphase stamping kernel radius in sensor pixels (0 = auto).
 * @param banks Number of polyphase banks per axis for subpixel stamping.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_measured_psf(const Image<TSpectral>& data,
                                              float samples_per_pixel,
                                              PSFSampling sampling,
                                              int radius,
                                              int banks)
{
    this->set_psf<MeasuredPSF<TSpectral>>(data, samples_per_pixel, sampling, radius, banks);
}

/**
 * @brief Use the aperture's diffraction pattern as the PSF (point spread function), with the
 * given stamp size.
 *
 * This is the default, with an automatic stamp radius: so this sets the stamp size, or brings
 * the aperture's PSF back after set_psf() or delete_psf(). (In the interim: the API rework
 * replaces this, when diffraction and the PSF as a whole are set separately.)
 *
 * The PSF follows the aperture, focal length and pixel pitch: it is made from their values at
 * the time it is needed (see precompute()), so they can be set before or after this.
 *
 * The automatic stamp radius holds about 99% of the light: for a circular aperture's Airy
 * pattern that is 20.4 lambda N, at the longest of the spectral type's wavelengths, plus a
 * pixel. It is at least MIN_AUTO_PSF_RADIUS and at most DEFAULT_PSF_RADIUS pixels; light
 * beyond the stamp is folded into it, since each stamp is normalized.
 *
 * @param radius Radius in pixels of the stamps used for unresolved sources; 0 for automatic.
 * @param banks Subpixel positions per axis the stamps are made for.
 * @throws std::runtime_error if the radius is negative, the banks less than 1, or the stamps
 *         would exceed 4 GiB.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::use_aperture_psf(int radius, int banks)
{
    if (radius < 0) {
        HUIRA_THROW_ERROR("CameraModel::use_aperture_psf - Radius must be non-negative (0 for "
                          "automatic): " +
                          std::to_string(radius));
    }
    PSF<TSpectral>::check_polyphase_size(radius == 0 ? 1 : radius, banks);
    if (use_aperture_psf_ && radius == aperture_psf_radius_ && banks == aperture_psf_banks_) {
        return;
    }
    use_aperture_psf_ = true;
    aperture_psf_radius_ = radius;
    aperture_psf_banks_ = banks;
    psf_ = nullptr; // made when needed
    optics_changed_(OpticsInput::CorePSF);
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
        optics_changed_(OpticsInput::Convolution);
    }
}

/**
 * @brief Returns the total-system PSF kernel for whole-image convolution.
 *
 * The total point spread function of the optical system is the energy-weighted sum of its
 * components: the diffraction-limited core (from the aperture or a user-provided PSF) and the
 * Harvey-Shack scattered-light wings. Veiling glare, the third component, is uniform across
 * the image and is applied separately by the renderer for efficiency. The kernel is built
 * when first needed, and rebuilt when the settings it depends on change (see precompute()).
 *
 * @return Reference to the total-system convolution kernel (unit energy per channel), valid
 *         until the camera's settings next change.
 */
template <IsSpectral TSpectral>
const Image<TSpectral>& CameraModel<TSpectral>::get_psf_convolution_kernel()
{
    std::lock_guard<std::mutex> lock(optics_mutex_);
    tbb::this_task_arena::isolate([&] { ensure_convolution_kernel_(); });
    return psf_convolution_kernel_;
}

/**
 * @brief Returns the scattered-light wings kernel alone, building it if needed.
 *
 * This is the Harvey-Shack component of the total system PSF, normalized to unit energy and
 * NOT scaled by the scatter fraction. The renderer uses it to apply wings to unresolved
 * sources: their compact core is stamped via the polyphase cache, their raw energy is
 * splatted into a separate buffer, and that buffer is convolved with this kernel before the
 * two are blended by (1 - f_s) and f_s. Requires scattering to be enabled.
 *
 * @return Reference to the wings kernel (unit energy per channel), valid until the camera's
 *         settings next change.
 */
template <IsSpectral TSpectral>
const Image<TSpectral>& CameraModel<TSpectral>::get_psf_wings_kernel()
{
    std::lock_guard<std::mutex> lock(optics_mutex_);
    tbb::this_task_arena::isolate([&] { ensure_wings_kernel_(); });
    return psf_wings_kernel_;
}

/**
 * @brief Get the PSF's stamp for an unresolved source at a subpixel position, building the
 * stamps first if needed.
 *
 * @param u Horizontal subpixel position in [0, 1).
 * @param v Vertical subpixel position in [0, 1).
 * @return Reference to the stamp, valid until the camera's settings next change.
 */
template <IsSpectral TSpectral>
const Image<TSpectral>& CameraModel<TSpectral>::get_psf_kernel(float u, float v)
{
    if (!has_psf()) {
        HUIRA_THROW_ERROR("CameraModel::get_psf_kernel - No PSF has been set");
    }
    std::lock_guard<std::mutex> lock(optics_mutex_);
    tbb::this_task_arena::isolate([&] { ensure_polyphase_(); });
    return psf_->get_kernel(u, v);
}

/// Get the radius in pixels of the PSF's stamps for unresolved sources; 0 without a PSF.
template <IsSpectral TSpectral>
int CameraModel<TSpectral>::get_psf_radius() const
{
    if (use_aperture_psf_) {
        return aperture_psf_stamp_radius_();
    }
    return psf_ != nullptr ? psf_->get_radius() : 0;
}

/// The aperture PSF's stamp radius: as set, or automatic (see use_aperture_psf()).
template <IsSpectral TSpectral>
int CameraModel<TSpectral>::aperture_psf_stamp_radius_() const
{
    if (aperture_psf_radius_ > 0) {
        return aperture_psf_radius_;
    }
    double longest_wavelength = 0.0;
    for (std::size_t i = 0; i < TSpectral::size(); ++i) {
        longest_wavelength = std::max(longest_wavelength, TSpectral::get_bin(i).center_wavelength);
    }
    const double fnumber =
        static_cast<double>(focal_length_) / (2.0 * aperture_->get_bounding_radius().to_si());
    const double pitch = std::min(sensor_->pixel_pitch().x, sensor_->pixel_pitch().y);
    constexpr double RADIUS_99_PERCENT = 20.4; // in units of lambda N, for an Airy pattern
    const double radius = std::ceil(RADIUS_99_PERCENT * longest_wavelength * fnumber / pitch) + 1;
    return static_cast<int>(std::clamp(
        radius, static_cast<double>(MIN_AUTO_PSF_RADIUS), static_cast<double>(DEFAULT_PSF_RADIUS)));
}

/**
 * @brief Remove the PSF, the aperture's diffraction pattern (the default) included: unresolved
 * sources then put all their light in the pixel they fall in. use_aperture_psf() brings the
 * aperture's back.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::delete_psf()
{
    if (!has_psf()) {
        return;
    }
    psf_ = nullptr;
    use_aperture_psf_ = false;
    optics_changed_(OpticsInput::CorePSF);
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
 * @param scatter_fraction Fraction of light scattered, in [0, 1)
 * @param falloff_exponent Power-law exponent of the falloff (typically 2 to 3)
 * @param r0 Shoulder radius in pixels: the profile is flat within it and falls off as
 *        r^-falloff_exponent beyond it (default 0.5)
 * @param radius Cutoff radius in pixels (default 0: none, beyond the convolution kernel's own
 *        radius)
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
    if (!(falloff_exponent > 0.f) || !std::isfinite(falloff_exponent)) {
        HUIRA_THROW_ERROR("CameraModel::set_harvey_shack_scatter - Falloff exponent must be a "
                          "positive finite value: " +
                          std::to_string(falloff_exponent));
    }
    if (!(r0 > 0.f) || !std::isfinite(r0)) {
        HUIRA_THROW_ERROR("CameraModel::set_harvey_shack_scatter - r0 must be a positive finite "
                          "value: " +
                          std::to_string(r0));
    }
    if (radius < 0.f || std::isnan(radius)) {
        HUIRA_THROW_ERROR("CameraModel::set_harvey_shack_scatter - Radius must be non-negative: " +
                          std::to_string(radius));
    }
    if (scatter_fraction == scatter_fraction_ && falloff_exponent == scatter_falloff_exponent_ &&
        r0 == r0_ && radius == scatter_radius_) {
        return;
    }
    scatter_fraction_ = scatter_fraction;
    scatter_falloff_exponent_ = falloff_exponent;
    r0_ = r0;
    scatter_radius_ = radius;
    scatter_enabled_ = (scatter_fraction > 0.f);
    optics_changed_(OpticsInput::Scatter);
}

/**
 * @brief Disable Harvey-Shack scatter effects.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::disable_harvey_shack_scatter()
{
    set_harvey_shack_scatter(0.f, 2.f, 0.5f, 0.f);
}

/**
 * @brief Build everything the camera's optics and geometry need for the next render, now.
 *
 * The kernels a render uses are derived from the camera's settings: the PSF's stamps for
 * unresolved sources (see use_aperture_psf() and set_psf()), the defocus blur's stamps (see
 * set_focus_distance()), and the whole-image convolution kernels and their spectra (see
 * enable_psf_convolution() and set_harvey_shack_scatter()). So are the tables of the camera's
 * geometry: each pixel's ray direction with distortion, and its solid angle, and the view
 * frustum, which depend on the focal length, sensor, intrinsics, distortion and axis
 * convention. The setters only record settings, so they are cheap and can be made in any
 * order, and each kernel or table is rebuilt only when a setting it depends on has changed:
 * refocusing, for example, does not rebuild the PSF's stamps.
 *
 * Building can take a while, for a large PSF or convolution kernel, or a large sensor with
 * distortion. Calling this once the
 * camera is configured keeps that time out of the first render, so that every render takes
 * comparable time, as a timed or hardware-in-the-loop run needs.
 *
 * Without it, a render builds whatever is out of date itself and logs the time taken: as a
 * warning the first time after precompute() was called (the settings have then changed
 * since), and as information otherwise. set_auto_precompute(false) makes the render throw
 * instead.
 *
 * Only what the current settings use is built: for example, no PSF stamps while a defocus
 * blur replaces them.
 *
 * @throws std::runtime_error if the settings are inconsistent, e.g. scattering is enabled
 *         without a PSF and without set_psf_convolution_radius(), or the lens distortion has no
 *         inverse somewhere in the image at this focal length and sensor (see set_distortion()).
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::precompute()
{
    std::lock_guard<std::mutex> lock(optics_mutex_);
    explicitly_precomputed_ = true;
    if (geometry_current_() && is_precomputed_locked_()) {
        check_distortion_("CameraModel::precompute");
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    ensure_geometry_();
    check_distortion_("CameraModel::precompute");
    tbb::this_task_arena::isolate([&] { precompute_locked_(); });
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    HUIRA_LOG_INFO("CameraModel - Precomputed the camera's geometry and optics in " +
                   std::to_string(elapsed.count()) + " seconds");
}

/**
 * @brief Whether everything the next render needs from the camera's optics and geometry is
 * built and up to date.
 *
 * True after precompute(), until a setting changes that a kernel or table the render uses
 * depends on.
 * Setting a value to what it already is changes nothing. Also true when the settings need
 * nothing built.
 */
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::is_precomputed() const
{
    std::lock_guard<std::mutex> lock(optics_mutex_);
    return geometry_current_() && is_precomputed_locked_();
}

/**
 * @brief Choose what a render does when the camera's optics kernels are out of date.
 *
 * Enabled by default: the render builds them, as precompute() would, and logs the time taken.
 * Disabled, the render throws instead, which guarantees that no render includes that time: a
 * timed or hardware-in-the-loop run can disable it and call precompute() after every change.
 *
 * @param auto_precompute True for the render to build out-of-date kernels, false to throw.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_auto_precompute(bool auto_precompute)
{
    auto_precompute_ = auto_precompute;
}

/**
 * @brief Called by the renderer before each render: makes sure the optics kernels are up to
 * date, as set_auto_precompute() chooses.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::precompute_for_render_()
{
    std::lock_guard<std::mutex> lock(optics_mutex_);
    if (geometry_current_() && is_precomputed_locked_()) {
        check_distortion_("Renderer::render");
        return;
    }
    if (!auto_precompute_) {
        HUIRA_THROW_ERROR("Renderer::render - The camera's geometry or optics are out of date "
                          "and auto precompute is disabled. Call precompute() on the camera "
                          "after changing it, before rendering.");
    }

    const auto start = std::chrono::steady_clock::now();
    ensure_geometry_();
    check_distortion_("Renderer::render");
    tbb::this_task_arena::isolate([&] { precompute_locked_(); });
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    const std::string seconds = std::to_string(elapsed.count());
    if (explicitly_precomputed_) {
        // Once per precompute(): a camera changed on every frame would otherwise warn on every
        // frame. Later rebuilds are logged as information until precompute() is called again.
        explicitly_precomputed_ = false;
        HUIRA_LOG_WARNING("CameraModel - The camera changed after precompute() was called, so "
                          "this render rebuilt its geometry and optics, taking " +
                          seconds +
                          " seconds. Call precompute() after changing the camera to keep this "
                          "out of the render.");
    } else {
        HUIRA_LOG_INFO("CameraModel - Precomputed the camera's geometry and optics for this "
                       "render in " +
                       seconds +
                       " seconds. Call precompute() after configuring the camera to do this "
                       "before rendering.");
    }
}

/**
 * @brief Build every kernel the current settings use that is out of date.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::precompute_locked_()
{
    ensure_defocus_();

    // Unresolved sources in focus get the PSF's stamps, and the scattered-light wings. Out of
    // focus they get the defocus blur's stamps instead, which the convolution kernel (the PSF
    // and the wings together) then blurs. Resolved bodies get the convolution kernel too,
    // unless enable_psf_convolution(false).
    const bool defocused = !defocus_kernel_.empty();
    if (has_psf() && !defocused) {
        ensure_polyphase_();
    }
    if (scatter_enabled_ && !defocused) {
        ensure_wings_spectrum_();
    }
    if (convolves_bodies_() || (defocused && has_psf_or_scatter_())) {
        ensure_convolution_spectrum_();
    }
}

/**
 * @brief Whether precompute_locked_() has nothing to do. Mirrors it.
 */
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::is_precomputed_locked_() const
{
    const bool defocused = defocus_blur_radius() > 0.f;
    if (defocused) {
        if (defocus_kernel_.empty() || stale_(defocus_built_at_,
                                              {OpticsInput::FocalLength,
                                               OpticsInput::PixelPitch,
                                               OpticsInput::Aperture,
                                               OpticsInput::Focus,
                                               OpticsInput::Resolution})) {
            return false;
        }
    } else if (!defocus_kernel_.empty()) {
        return false; // stamps left from when it was out of focus
    }

    if (has_psf() && !defocused) {
        const bool diffraction_current =
            !use_aperture_psf_ || (psf_ != nullptr && !diffraction_stale_(diffraction_built_at_));
        if (!diffraction_current || !psf_->has_polyphase_cache()) {
            return false;
        }
    }

    // The spectra depend on everything their kernels do, so they are current only if the
    // kernels are too:
    if (scatter_enabled_ && !defocused &&
        (wings_stale_(wings_spectrum_built_at_) ||
         stale_(wings_spectrum_built_at_, {OpticsInput::Resolution}))) {
        return false;
    }
    if ((convolves_bodies_() || (defocused && has_psf_or_scatter_())) &&
        (convolution_stale_(convolution_spectrum_built_at_) ||
         stale_(convolution_spectrum_built_at_, {OpticsInput::Resolution}))) {
        return false;
    }
    return true;
}

/**
 * @brief Whether anything built at built_at is out of date: never built, or one of the inputs
 * has changed since.
 */
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::stale_(std::uint64_t built_at,
                                    std::initializer_list<OpticsInput> inputs) const
{
    if (built_at == 0) {
        return true;
    }
    return std::any_of(inputs.begin(), inputs.end(), [&](OpticsInput input) {
        return optics_changed_at_[static_cast<std::size_t>(input)] > built_at;
    });
}

/// The aperture's PSF depends on the optics; a PSF that was set, only on itself.
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::diffraction_stale_(std::uint64_t built_at) const
{
    if (use_aperture_psf_) {
        return stale_(built_at,
                      {OpticsInput::FocalLength,
                       OpticsInput::PixelPitch,
                       OpticsInput::Aperture,
                       OpticsInput::CorePSF});
    }
    return stale_(built_at, {OpticsInput::CorePSF});
}

/// The convolution kernel is the core PSF mixed with the scattered-light wings.
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::convolution_stale_(std::uint64_t built_at) const
{
    return diffraction_stale_(built_at) ||
           stale_(built_at, {OpticsInput::Convolution, OpticsInput::Scatter});
}

/// The wings kernel is sized from the convolution radius, or the core PSF's.
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::wings_stale_(std::uint64_t built_at) const
{
    // The aperture PSF's automatic stamp radius follows the optics:
    const bool automatic_radius =
        use_aperture_psf_ && aperture_psf_radius_ == 0 && psf_convolution_radius_ == 0;
    return stale_(built_at,
                  {OpticsInput::CorePSF, OpticsInput::Convolution, OpticsInput::Scatter}) ||
           (automatic_radius &&
            stale_(built_at,
                   {OpticsInput::FocalLength, OpticsInput::PixelPitch, OpticsInput::Aperture}));
}

/**
 * @brief Make the aperture's PSF from the current optics, if it is in use and out of date.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_diffraction_()
{
    if (!use_aperture_psf_ || (psf_ != nullptr && !diffraction_stale_(diffraction_built_at_))) {
        return;
    }
    const Vec2<float> pitch = sensor_->pixel_pitch();
    psf_ = aperture_->make_psf(units::Meter(focal_length_),
                               units::Meter(pitch.x),
                               units::Meter(pitch.y),
                               aperture_psf_stamp_radius_(),
                               aperture_psf_banks_);
    diffraction_built_at_ = optics_version_;
}

/**
 * @brief Build the core PSF's stamps for unresolved sources, if out of date.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_polyphase_()
{
    ensure_diffraction_();
    if (psf_ != nullptr) {
        psf_->ensure_polyphase_cache();
    }
}

/**
 * @brief Build the defocus blur's stamps for unresolved sources if out of date, or remove
 * them when in focus.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_defocus_()
{
    const float radius = defocus_blur_radius();
    if (radius <= 0.f) {
        defocus_kernel_.clear();
        return;
    }
    if (!defocus_kernel_.empty() && !stale_(defocus_built_at_,
                                            {OpticsInput::FocalLength,
                                             OpticsInput::PixelPitch,
                                             OpticsInput::Aperture,
                                             OpticsInput::Focus,
                                             OpticsInput::Resolution})) {
        return;
    }
    // Sources are stamped where they project onto the image, so no stamp needs to reach further
    // than the image's larger dimension:
    const int reach = std::max(sensor_->resolution().x, sensor_->resolution().y) + 1;
    defocus_kernel_.build(*aperture_, radius, DEFOCUS_BANKS_, reach);
    defocus_built_at_ = optics_version_;
}

/**
 * @brief The radius of the whole-image convolution kernels: as set, or the core PSF's.
 *
 * @param caller Name for error messages.
 * @throws std::runtime_error if there is no PSF or scattering to convolve with, or no radius.
 */
template <IsSpectral TSpectral>
int CameraModel<TSpectral>::convolution_radius_(const char* caller) const
{
    if (!has_psf() && !scatter_enabled_) {
        HUIRA_THROW_ERROR(std::string(caller) + " - No PSF or scatter model has been set");
    }
    if (psf_convolution_radius_ > 0) {
        return psf_convolution_radius_;
    }
    if (!has_psf()) {
        HUIRA_THROW_ERROR(std::string(caller) +
                          " - set_psf_convolution_radius() is required when scattering is "
                          "enabled without a core PSF");
    }
    return get_psf_radius();
}

/**
 * @brief Build the total-system convolution kernel, if out of date. See
 * get_psf_convolution_kernel().
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_convolution_kernel_()
{
    const int radius = convolution_radius_("CameraModel::get_psf_convolution_kernel");
    if (!convolution_stale_(convolution_kernel_built_at_)) {
        return;
    }

    const int dim = 2 * radius + 1;
    HUIRA_LOG_INFO("CameraModel - Generating " + std::to_string(dim) + "x" + std::to_string(dim) +
                   " PSF convolution kernel");

    // Core component: the diffraction-limited (or user-provided) PSF. With no core PSF set,
    // the core is an ideal delta (perfect optics plus scatter):
    ensure_diffraction_();
    if (psf_ != nullptr) {
        psf_convolution_kernel_ = psf_->generate_convolution_kernel(radius);
    } else {
        psf_convolution_kernel_ = Image<TSpectral>(dim, dim, TSpectral{0.f});
        psf_convolution_kernel_(radius, radius) = TSpectral{1.f};
    }

    // Scattered-light wings: mixed with the core by energy fraction, so that the total system
    // PSF remains normalized to unit energy:
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

    convolution_kernel_built_at_ = optics_version_;
}

/**
 * @brief Transform the total-system convolution kernel at the sensor's resolution, if out of
 * date.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_convolution_spectrum_()
{
    ensure_convolution_kernel_();
    if (!convolution_stale_(convolution_spectrum_built_at_) &&
        !stale_(convolution_spectrum_built_at_, {OpticsInput::Resolution})) {
        return;
    }
    const Image<TSpectral>& kernel = psf_convolution_kernel_;
    if (kernel.width() * kernel.height() > DIRECT_CONVOLUTION_MAX_AREA_) {
        psf_convolver_.set_kernel(kernel, sensor_->resolution());
    }
    convolution_spectrum_built_at_ = optics_version_;
}

/**
 * @brief Build the scattered-light wings kernel, if out of date. See get_psf_wings_kernel().
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_wings_kernel_()
{
    if (!scatter_enabled_) {
        HUIRA_THROW_ERROR("CameraModel::get_psf_wings_kernel - Scattering is not enabled");
    }
    const int radius = convolution_radius_("CameraModel::get_psf_wings_kernel");
    if (!wings_stale_(wings_kernel_built_at_)) {
        return;
    }
    HarveyShackScatter<TSpectral> scatter(scatter_falloff_exponent_, r0_, scatter_radius_);
    psf_wings_kernel_ = scatter.generate_convolution_kernel(radius);
    wings_kernel_built_at_ = optics_version_;
}

/**
 * @brief Transform the scattered-light wings kernel at the sensor's resolution, if out of date.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_wings_spectrum_()
{
    ensure_wings_kernel_();
    if (!wings_stale_(wings_spectrum_built_at_) &&
        !stale_(wings_spectrum_built_at_, {OpticsInput::Resolution})) {
        return;
    }
    const Image<TSpectral>& kernel = psf_wings_kernel_;
    if (kernel.width() * kernel.height() > DIRECT_CONVOLUTION_MAX_AREA_) {
        wings_convolver_.set_kernel(kernel, sensor_->resolution());
    }
    wings_spectrum_built_at_ = optics_version_;
}

/**
 * @brief Convolve an image in place with the total-system convolution kernel, as
 * Image::convolve() would, but through the precomputed spectrum.
 *
 * For the renderer, after precompute_for_render_().
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::apply_psf_convolution_(Image<TSpectral>& image) const
{
    const Image<TSpectral>& kernel = psf_convolution_kernel_;
    if (kernel.width() * kernel.height() <= DIRECT_CONVOLUTION_MAX_AREA_) {
        image.convolve(kernel);
        return;
    }
    // The convolver's working buffers are shared:
    std::lock_guard<std::mutex> lock(optics_mutex_);
    tbb::this_task_arena::isolate([&] { psf_convolver_.apply(image); });
}

/**
 * @brief Convolve an image in place with the scattered-light wings kernel. See
 * apply_psf_convolution_().
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::apply_wings_convolution_(Image<TSpectral>& image) const
{
    const Image<TSpectral>& kernel = psf_wings_kernel_;
    if (kernel.width() * kernel.height() <= DIRECT_CONVOLUTION_MAX_AREA_) {
        image.convolve(kernel);
        return;
    }
    std::lock_guard<std::mutex> lock(optics_mutex_);
    tbb::this_task_arena::isolate([&] { wings_convolver_.apply(image); });
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
 * @brief Focus the camera in diopters: the reciprocal of the focus distance.
 *
 * 0 focuses at infinity, positive values focus in front of the camera, and negative values
 * focus past infinity. This is the sign of a focus scale marked in diopters. (In the sign
 * convention of optics, where light diverging from a point in front of a lens has negative
 * vergence, the light from the focus distance has vergence -diopters.)
 *
 * @param diopters The reciprocal of the focus distance (diopters, i.e. reciprocal meters).
 * @throws std::runtime_error if it is not finite, or corresponds to a focus distance closer to
 *         zero than 1e-12 m.
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

/// Get the focus in diopters: the reciprocal of focus_distance(), 0 at infinity.
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
 * @brief Turn depth of field on (the default) or off.
 *
 * On, the camera has the focus that set_focus_distance(), set_focus_diopters() or
 * set_focus_sensor_offset() gave it (at infinity unless set): resolved bodies are traced with
 * rays from across the aperture, which blurs whatever is out of focus, and unresolved sources
 * are blurred by their defocus (see defocus_blur_radius()). Off, everything is in focus, as
 * through a pinhole, whatever the focus setting.
 *
 * Rays from across the aperture are noisier where bodies are out of focus, and need more
 * samples per pixel to look clean: with focus at infinity, bodies closer than about the
 * aperture's diameter over the angle a pixel spans (for 50 mm at f/2.8 and 8.5 um pixels,
 * about 100 m).
 *
 * @param depth_of_field True to turn depth of field on, false to turn it off.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::enable_depth_of_field(bool depth_of_field)
{
    if (depth_of_field == depth_of_field_) {
        return;
    }
    depth_of_field_ = depth_of_field;
    optics_changed_(OpticsInput::Focus); // unresolved sources' defocus follows it
}

/**
 * @brief Get the radius, in pixels, of the defocus blur applied to unresolved sources.
 *
 * This is the blur of a point at infinity for the current focus, aperture, focal length and
 * pixel pitch. It is 0 when the blur is under half a pixel, which is treated as in focus, and
 * when depth of field is off (see enable_depth_of_field()).
 */
template <IsSpectral TSpectral>
float CameraModel<TSpectral>::defocus_blur_radius() const
{
    if (!depth_of_field_) {
        return 0.f;
    }

    // Depth of field sends light from a point at infinity through aperture point a to the
    // sensor position for direction a / d off the point's own, for focus distance d. Over an
    // aperture of bounding radius R that is a disk of radius R * f * |vergence|: exactly what
    // the ray tracer does, since focus does not change the projection, and to first order what
    // a physical lens does (see set_focus_sensor_offset()). The smaller pitch gives the
    // conservative pixel count.
    const double vergence = focus_diopters().to_si();
    const double aperture_radius = aperture_->get_bounding_radius().to_si();
    const double pitch = std::min(sensor_->pixel_pitch().x, sensor_->pixel_pitch().y);
    const auto radius = static_cast<float>(std::abs(vergence) * static_cast<double>(focal_length_) *
                                           aperture_radius / pitch);

    // Blur under half a pixel is treated as in focus:
    return radius < 0.5f ? 0.f : radius;
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
    const double previous = focus_diopters().to_si();
    focus_reference_ = reference;
    focus_setting_ = (setting == 0.0) ? 0.0 : setting; // no negative zero
    update_focus_();

    // The defocus blur depends on the vergence, however it was given:
    if (focus_diopters().to_si() != previous) {
        optics_changed_(OpticsInput::Focus);
    }
}

/**
 * @brief Resolve the focus setting into the focus distance used by ray generation.
 *
 * Must run whenever the focus setting or the focal length changes (compute_intrinsics_() calls
 * this), since a sensor offset resolves to a different distance at a different focal length.
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
}

/**
 * @brief Project a 3D point in camera coordinates onto the image plane.
 *
 * Uses the pinhole camera model and applies distortion if present.
 * @param point_camera_coords 3D point in camera coordinates (meters)
 * @return Pixel 2D point on the image plane, in the camera's pixel convention (see
 * set_pixel_convention()), or NaN in both coordinates where the lens images nothing: past the
 * pole of an OpenCV distortion model's rational radial factor.
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
    return Pixel{fx_ * normalized[0] + skew_ * normalized[1] + cx_, fy_ * normalized[1] + cy_};
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
 * The position may be anywhere, including outside the image.
 *
 * @param pixel Position in the camera's pixel convention (see set_pixel_convention()).
 * @param sampler Sampler for the aperture position.
 * @throws std::runtime_error if the lens distortion has no inverse at the position (see
 *         set_distortion()).
 */
template <IsSpectral TSpectral>
Ray<TSpectral> CameraModel<TSpectral>::cast_ray(const Pixel& pixel, Sampler<float>& sampler) const
{
    const Pixel sensor_position = pixel_convention_.to_sensor(pixel, sensor_->resolution());
    const Ray<TSpectral> ray = sensor_ray_(sensor_position, sampler, false);
    if (!std::isfinite(ray.direction().x) || !std::isfinite(ray.direction().y) ||
        !std::isfinite(ray.direction().z)) {
        throw_no_inverse_("CameraModel::cast_ray", sensor_position);
    }
    return ray;
}

/**
 * @brief Cast a pinhole camera ray through a position on the image.
 *
 * The position may be anywhere, including outside the image. The direction is computed exactly
 * (with distortion, by inverting it), so it needs nothing built (see precompute()).
 *
 * @param pixel Position in the camera's pixel convention (see set_pixel_convention()).
 * @throws std::runtime_error if the lens distortion has no inverse at the position (see
 *         set_distortion()).
 */
template <IsSpectral TSpectral>
Ray<TSpectral> CameraModel<TSpectral>::cast_ray(const Pixel& pixel) const
{
    const Pixel sensor_position = pixel_convention_.to_sensor(pixel, sensor_->resolution());
    const Ray<TSpectral> ray = sensor_ray_(sensor_position, false);
    if (!std::isfinite(ray.direction().x) || !std::isfinite(ray.direction().y) ||
        !std::isfinite(ray.direction().z)) {
        throw_no_inverse_("CameraModel::cast_ray", sensor_position);
    }
    return ray;
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
                                                   Sampler<float>& sampler,
                                                   bool use_field) const
{
    Vec3<float> origin{0, 0, 0};
    Vec3<float> direction = ray_direction_(pixel, use_field);

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
Ray<TSpectral> CameraModel<TSpectral>::sensor_ray_(const Pixel& pixel, bool use_field) const
{
    return Ray<TSpectral>{Vec3<float>{0, 0, 0}, glm::normalize(ray_direction_(pixel, use_field))};
}

/**
 * @brief Direction (not normalized) of the pinhole ray through a sensor position.
 *
 * Without distortion this is computed exactly. With distortion, for the renderer, it is
 * interpolated from the directions at every pixel corner (the distortion field), which span the
 * whole sensor, [0, width] x [0, height], when use_field and the field is up to date (see
 * precompute()); otherwise, and beyond the sensor, it is computed exactly, by undistorting.
 */
template <IsSpectral TSpectral>
Vec3<float> CameraModel<TSpectral>::ray_direction_(const Pixel& pixel, bool use_field) const
{
    const bool on_sensor = pixel[0] >= 0.f && pixel[0] <= rx_ && pixel[1] >= 0.f && pixel[1] <= ry_;
    if (!distortion_ || !on_sensor || !use_field || !pixel_geometry_current_()) {
        return pixel_to_direction_<float>(pixel);
    }
    return distortion_field_.sample_bilinear<WrapMode::Clamp>(pixel[0] / rx_, pixel[1] / ry_);
}

/**
 * @brief The power a pixel receives per unit of radiance: its solid angle times the aperture
 * area projected along the direction to the pixel's center.
 *
 * Read from a table that precompute() and renders build; until then, computed directly, the
 * same way.
 *
 * @param x Pixel column.
 * @param y Pixel row, from the top.
 * @throws std::runtime_error if the lens distortion has no inverse at the pixel (see
 *         set_distortion()).
 */
template <IsSpectral TSpectral>
float CameraModel<TSpectral>::pixel_radiance_to_power(int x, int y) const
{
    // From the table when it is up to date (always, in a render), else computed the same way:
    const float geometry = pixel_geometry_current_()
                               ? pixel_geometry_factors_(x, y)
                               : static_cast<float>(pixel_geometry_factor_(x, y));
    const float factor = geometry * this->aperture_->get_area().to_si_f();
    if (!std::isfinite(factor)) {
        // Pixel (x, y) covers [x, x + 1) x [y, y + 1) in sensor coordinates:
        throw_no_inverse_("CameraModel::pixel_radiance_to_power",
                          Pixel{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f});
    }
    return factor;
}

template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::in_fov(const Vec3<float>& point) const
{
    float len2 = glm::dot(point, point);
    if (len2 < 1e-12f) {
        return false;
    }
    return view_frustum().contains(point);
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
 * @brief Set the f-stop (focal ratio) of the camera.
 *
 * Sets the aperture's diameter to the current focal length divided by the f-stop. The diameter
 * is what is kept: if the focal length changes later, the f-number changes with it, and a
 * warning is logged the first time, in case that was not intended. Set the f-stop after the
 * focal length to get the f-number asked for, or use set_aperture_diameter() to set the
 * diameter itself.
 *
 * Until the aperture is set, by this or set_aperture_diameter() or set_aperture(), the camera
 * is f/2.8 at any focal length.
 *
 * @param fstop The f-number, e.g. 8 for f/8.
 * @throws std::runtime_error if it is not positive and finite.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_fstop(float fstop)
{
    if (!(fstop > 0.f) || !std::isfinite(fstop)) {
        HUIRA_THROW_ERROR("CameraModel::set_fstop - The f-stop must be a positive finite value: " +
                          std::to_string(fstop));
    }
    set_aperture_area_(units::Meter(focal_length_ / fstop));
    aperture_source_ = ApertureSource::FStop;
    fstop_setting_ = fstop;
    fstop_change_warned_ = false;
}

/**
 * @brief Get the f-stop (focal ratio) of the camera: the focal length divided by the aperture's
 * diameter (for a non-circular aperture, the diameter of a circle of the same area).
 */
template <IsSpectral TSpectral>
float CameraModel<TSpectral>::fstop() const
{
    float area = this->aperture_->get_area().to_si_f();
    float aperture_diameter = 2.f * std::sqrt(area / PI<float>());
    return focal_length_ / aperture_diameter;
}

/**
 * @brief Set the aperture's diameter, which is kept if the focal length changes.
 *
 * The aperture keeps its shape. For a non-circular aperture this is the diameter of a circle of
 * the same area. Unlike set_fstop(), the f-number then follows the focal length without a
 * warning, since the diameter is what was asked for.
 *
 * @param diameter The aperture's diameter.
 * @throws std::runtime_error if it is not positive and finite.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_aperture_diameter(units::Millimeter diameter)
{
    const double d = diameter.to_si();
    if (!(d > 0.0) || !std::isfinite(d)) {
        HUIRA_THROW_ERROR("CameraModel::set_aperture_diameter - The diameter must be a positive "
                          "finite value: " +
                          std::to_string(d) + " m");
    }
    set_aperture_area_(units::Meter(d));
    aperture_source_ = ApertureSource::Diameter;
}

/// Get the aperture's diameter (for a non-circular aperture, that of a circle of the same area).
template <IsSpectral TSpectral>
units::Millimeter CameraModel<TSpectral>::aperture_diameter() const
{
    const double area = aperture_->get_area().to_si();
    return units::Meter(2.0 * std::sqrt(area / PI<double>()));
}

/**
 * @brief Resize the aperture to the area of a circle of the given diameter, keeping its shape.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_aperture_area_(units::Meter diameter)
{
    const units::SquareMeter area = PI<float>() * (diameter * diameter) / 4.f;
    if (!(area.to_si() > 0.0) || !std::isfinite(area.to_si())) {
        HUIRA_THROW_ERROR("CameraModel - The aperture's area must be a positive finite value: " +
                          std::to_string(area.to_si()) + " m^2, from a diameter of " +
                          std::to_string(diameter.to_si()) + " m");
    }
    const double previous_area = aperture_->get_area().to_si();
    aperture_->set_area(area);
    if (aperture_->get_area().to_si() != previous_area) {
        optics_changed_(OpticsInput::Aperture);
    }
}

/**
 * @brief Apply a focal length change to the aperture. See set_focal_length().
 *
 * @param previous_focal_length The focal length before the change, in meters; 0 when the camera
 *        is first constructed.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::focal_length_changed_(float previous_focal_length)
{
    switch (aperture_source_) {
    case ApertureSource::Default:
        set_aperture_area_(units::Meter(focal_length_ / DEFAULT_FSTOP_));
        break;
    case ApertureSource::FStop:
        if (!fstop_change_warned_ && previous_focal_length > 0.f) {
            fstop_change_warned_ = true;
            HUIRA_LOG_WARNING(
                "CameraModel - The focal length changed from " +
                detail::short_number(static_cast<double>(previous_focal_length) * 1e3) + " mm to " +
                detail::short_number(static_cast<double>(focal_length_) * 1e3) +
                " mm after set_fstop(" + detail::short_number(fstop_setting_) +
                "). The aperture keeps its " +
                detail::short_number(aperture_diameter().to_si() * 1e3) +
                " mm diameter, so the camera is now f/" + detail::short_number(fstop()) +
                ". Call set_fstop() after set_focal_length() to keep f/" +
                detail::short_number(fstop_setting_) +
                ", or set_aperture_diameter() to keep the diameter without this warning.");
        }
        break;
    case ApertureSource::Diameter:
    default:
        break;
    }
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

    // The skew as given is in the pixel convention, where y may count up from the bottom row,
    // which flips its sign in sensor coordinates (y down):
    const float skew_sign = (pixel_convention_.origin == PixelOrigin::BottomLeft) ? -1.f : 1.f;
    skew_ = skew_sign * shear_ * fx_;

    // The principal point as given is in the pixel convention; everything below works in sensor
    // coordinates. Unset, it is the center of the sensor, which is the same in every convention.
    const Pixel given{principal_x_.value_or(0.f), principal_y_.value_or(0.f)};
    const Pixel sensor = pixel_convention_.to_sensor(given, sensor_->resolution());
    cx_ = principal_x_ ? sensor.x : rx_ * 0.5f;
    cy_ = principal_y_ ? sensor.y : ry_ * 0.5f;

    // The geometry tables follow from these, and are rebuilt when next needed if they changed.
    // Changed back to what a table was built with, they are that table's version again:
    const GeometryKey key{
        fx_, fy_, cx_, cy_, skew_, rx_, ry_, blender_convention_, distortion_version_};
    if (key != geometry_key_ || geometry_version_ == 0) {
        geometry_key_ = key;
        const std::uint64_t pixel_built = pixel_geometry_built_at_.load(std::memory_order_acquire);
        const std::uint64_t frustum_built = frustum_built_at_.load(std::memory_order_acquire);
        if (pixel_built != NEVER_BUILT_ && key == pixel_geometry_built_key_) {
            geometry_version_ = pixel_built;
        } else if (frustum_built != NEVER_BUILT_ && key == frustum_built_key_) {
            geometry_version_ = frustum_built;
        } else {
            geometry_version_ = next_geometry_version_++;
        }
    }

    // Every focal length change comes through here, and a sensor offset focus depends on it:
    update_focus_();

    // The optics kernels depend on some of these; see precompute().
    const Vec2<float> pitch = sensor_->pixel_pitch();
    if (focal_length_ != optics_focal_length_) {
        const float previous_focal_length = optics_focal_length_;
        optics_focal_length_ = focal_length_;
        optics_changed_(OpticsInput::FocalLength);
        focal_length_changed_(previous_focal_length);
    }
    if (pitch.x != optics_pixel_pitch_.x || pitch.y != optics_pixel_pitch_.y) {
        optics_pixel_pitch_ = pitch;
        optics_changed_(OpticsInput::PixelPitch);
    }
    if (sensor_->resolution().x != optics_width_ || sensor_->resolution().y != optics_height_) {
        optics_width_ = sensor_->resolution().x;
        optics_height_ = sensor_->resolution().y;
        optics_changed_(OpticsInput::Resolution);
    }
}

template <IsSpectral TSpectral>
template <IsFloatingPoint TFloat>
Vec3<TFloat> CameraModel<TSpectral>::pixel_to_direction_(const Pixel& pixel) const
{
    // Invert the intrinsic matrix
    float ny = (pixel[1] - cy_) / fy_;
    float nx = (pixel[0] - cx_ - skew_ * ny) / fx_;
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

/**
 * @brief Build the geometry tables if they are out of date: the per-pixel tables and the view
 * frustum. Thread-safe.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_geometry_() const
{
    ensure_pixel_geometry_();
    ensure_frustum_();
}

template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::geometry_current_() const
{
    return pixel_geometry_current_() &&
           frustum_built_at_.load(std::memory_order_acquire) == geometry_version_;
}

/**
 * @brief Build the per-pixel tables (the distortion field and the pixels' geometry factors) if
 * they are out of date. Thread-safe: concurrent callers wait for a single build.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_pixel_geometry_() const
{
    if (pixel_geometry_current_()) {
        return;
    }
    std::lock_guard<std::mutex> lock(geometry_mutex_);
    if (pixel_geometry_current_()) {
        return;
    }
    // Isolated, so that while this thread waits for the parallel build it cannot pick up
    // another task that needs the tables (it would deadlock on the lock it holds).
    tbb::this_task_arena::isolate([&] { build_pixel_geometry_(); });
    pixel_geometry_built_key_ = geometry_key_;
    pixel_geometry_built_at_.store(geometry_version_, std::memory_order_release);
}

/**
 * @brief The unit direction of the pinhole ray through pixel corner (x, y), in sensor
 * coordinates; NaN where the distortion has no inverse.
 */
template <IsSpectral TSpectral>
Vec3<double> CameraModel<TSpectral>::corner_direction_(int x, int y) const
{
    return glm::normalize(
        pixel_to_direction_<double>(Pixel{static_cast<float>(x), static_cast<float>(y)}));
}

/**
 * @brief A pixel's power per unit radiance and aperture area, from the unit directions of its
 * corners (c00 at (x, y), c10 at (x + 1, y), c11 at (x + 1, y + 1), c01 at (x, y + 1)): its
 * solid angle, as two spherical triangles, times the cosine of the angle between the optical
 * axis and its center, which foreshortens the aperture. The center's direction is the mean of
 * the corners', as interpolating the distortion field gives it.
 */
template <IsSpectral TSpectral>
double CameraModel<TSpectral>::pixel_geometry_factor_(const Vec3<double>& c00,
                                                      const Vec3<double>& c10,
                                                      const Vec3<double>& c11,
                                                      const Vec3<double>& c01) const
{
    const double solid_angle =
        triangle_solid_angle_(c00, c10, c11) + triangle_solid_angle_(c00, c11, c01);
    const Vec3<double> center = glm::normalize(c00 + c10 + c11 + c01);
    return solid_angle * std::abs(center.z);
}

/**
 * @brief Pixel (x, y)'s geometry factor, computed directly: as the table holds it.
 */
template <IsSpectral TSpectral>
double CameraModel<TSpectral>::pixel_geometry_factor_(int x, int y) const
{
    return pixel_geometry_factor_(corner_direction_(x, y),
                                  corner_direction_(x + 1, y),
                                  corner_direction_(x + 1, y + 1),
                                  corner_direction_(x, y + 1));
}

/**
 * @brief Build the per-pixel tables in one pass over the pixel corners: each corner's direction
 * is found once, and serves the distortion field and the four pixels that share it.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::build_pixel_geometry_() const
{
    const Resolution res = sensor_->resolution();
    distortion_failure_.reset();
    distortion_field_ = distortion_ ? Image<Vec3<float>>(res.x + 1, res.y + 1, Vec3<float>{0.f})
                                    : Image<Vec3<float>>(0, 0);
    pixel_geometry_factors_ = Image<float>(res, 0.f);

    // Each block of pixel rows finds the corner rows above and below them; the field's corner
    // rows are written by the block holding them (the last corner row by the last block).
    tbb::parallel_for(tbb::blocked_range<int>(0, res.y), [&](const tbb::blocked_range<int>& rows) {
        const auto width = static_cast<std::size_t>(res.x) + 1;
        std::vector<Vec3<double>> upper(width);
        std::vector<Vec3<double>> lower(width);
        auto corner_row = [&](int y, std::vector<Vec3<double>>& row) {
            for (int x = 0; x <= res.x; ++x) {
                row[static_cast<std::size_t>(x)] = corner_direction_(x, y);
            }
            if (distortion_ && (y < rows.end() || y == res.y)) {
                for (int x = 0; x <= res.x; ++x) {
                    distortion_field_(x, y) = Vec3<float>(row[static_cast<std::size_t>(x)]);
                }
            }
        };

        corner_row(rows.begin(), upper);
        for (int y = rows.begin(); y < rows.end(); ++y) {
            corner_row(y + 1, lower);
            for (int x = 0; x < res.x; ++x) {
                const auto i = static_cast<std::size_t>(x);
                pixel_geometry_factors_(x, y) = static_cast<float>(
                    pixel_geometry_factor_(upper[i], upper[i + 1], lower[i + 1], lower[i]));
            }
            std::swap(upper, lower);
        }
    });

    // Where the distortion has no inverse, the directions are NaN. Keep the first such corner,
    // in reading order, for check_distortion_() to report:
    if (distortion_) {
        for (int y = 0; y <= res.y && !distortion_failure_; ++y) {
            for (int x = 0; x <= res.x; ++x) {
                const Vec3<float>& direction = distortion_field_(x, y);
                if (!std::isfinite(direction.x) || !std::isfinite(direction.y) ||
                    !std::isfinite(direction.z)) {
                    distortion_failure_ = Pixel{static_cast<float>(x), static_cast<float>(y)};
                    break;
                }
            }
        }
    }
}

/**
 * @brief Throw if the lens distortion has no inverse somewhere on the sensor, at the current
 * focal length and sensor. See set_distortion().
 *
 * @param caller Name for the error message.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::check_distortion_(const std::string& caller) const
{
    ensure_pixel_geometry_();
    if (distortion_failure_) {
        throw_no_inverse_(caller, *distortion_failure_);
    }
}

/**
 * @brief Throw for a position where the lens distortion has no inverse.
 *
 * @param caller Name for the error message.
 * @param sensor_position The position, in sensor coordinates; reported in the camera's pixel
 *        convention.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::throw_no_inverse_(const std::string& caller,
                                               const Pixel& sensor_position) const
{
    const Pixel position = pixel_convention_.from_sensor(sensor_position, sensor_->resolution());
    const std::string model = distortion_ ? " (" + distortion_->get_type_name() + ")" : "";
    HUIRA_THROW_ERROR(
        caller + " - The lens distortion" + model + " has no inverse at image position (" +
        detail::position_number(static_cast<double>(position.x)) + ", " +
        detail::position_number(static_cast<double>(position.y)) +
        "): no ray lands there, because the distortion model folds over or stops short of it. "
        "Check the distortion coefficients, and that they were calibrated for this focal length "
        "and sensor.");
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

/**
 * @brief Build the view frustum if it is out of date. Thread-safe.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_frustum_() const
{
    if (frustum_built_at_.load(std::memory_order_acquire) == geometry_version_) {
        return;
    }
    std::lock_guard<std::mutex> lock(geometry_mutex_);
    if (frustum_built_at_.load(std::memory_order_acquire) == geometry_version_) {
        return;
    }
    build_frustum_();
    frustum_built_key_ = geometry_key_;
    frustum_built_at_.store(geometry_version_, std::memory_order_release);
}

/**
 * @brief Find the view frustum from the ray directions around the image's boundary, computed
 * directly (2 (width + height) of them).
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::build_frustum_() const
{
    Resolution res = sensor_->resolution();

    FrustumBounds bounds;
    if (blender_convention_) {
        bounds.zdir = Vec3<float>{0, 0, -1};
        bounds.ydir = Vec3<float>{0, -1, 0};
    }

    // The side planes pass through the optical center and contain the image's y axis (left and
    // right) or x axis (top and bottom). For the whole sensor to be inside, each has to pass
    // through the boundary point that reaches furthest out on its side, measured as the
    // tangent of its angle from the forward axis. Every pixel corner on the boundary is a
    // candidate, out to the far edges at x = width and y = height. (The most oblique point
    // along each edge is not enough: with pincushion distortion the middle of an edge can reach
    // further out than its corners.)
    constexpr float INF_ = std::numeric_limits<float>::infinity();
    bounds.min_tan_x = INF_;
    bounds.max_tan_x = -INF_;
    bounds.min_tan_y = INF_;
    bounds.max_tan_y = -INF_;

    auto tangents = [&](int x, int y) {
        const Vec3<float> direction =
            pixel_to_direction_<float>(Pixel{static_cast<float>(x), static_cast<float>(y)});
        const float forward = glm::dot(direction, bounds.zdir);
        return Vec2<float>{glm::dot(direction, bounds.xdir) / forward,
                           glm::dot(direction, bounds.ydir) / forward};
    };

    // Comparisons skip the NaN directions of a distortion with no inverse there, which
    // precompute() reports.
    auto visit = [&](int x, int y) {
        const Vec2<float> t = tangents(x, y);
        bounds.min_tan_x = std::min(bounds.min_tan_x, t.x);
        bounds.max_tan_x = std::max(bounds.max_tan_x, t.x);
        bounds.min_tan_y = std::min(bounds.min_tan_y, t.y);
        bounds.max_tan_y = std::max(bounds.max_tan_y, t.y);

        // The angle a pixel spans at the boundary, one pixel inward along each axis this
        // corner is at the boundary of:
        auto step_to = [&](int inward_x, int inward_y) {
            const Vec2<float> inward = tangents(inward_x, inward_y);
            const float step = std::max(std::abs(inward.x - t.x), std::abs(inward.y - t.y));
            if (step > bounds.tan_per_pixel) {
                bounds.tan_per_pixel = step;
            }
        };
        if (x == 0 || x == res.x) {
            step_to(x == 0 ? 1 : res.x - 1, y);
        }
        if (y == 0 || y == res.y) {
            step_to(x, y == 0 ? 1 : res.y - 1);
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

    frustum_bounds_ = bounds;
    view_frustum_ = frustum_from_bounds_(0.f);
}

template <IsSpectral TSpectral>
Frustum<TSpectral> CameraModel<TSpectral>::frustum_from_bounds_(float widen_tan) const
{
    const FrustumBounds& b = frustum_bounds_;

    // Each side plane contains the image's y axis (left and right) or x axis (top and bottom),
    // and the direction at its tangent, moved out by widen_tan:
    const Vec3<float> left = b.xdir * (b.min_tan_x - widen_tan) + b.zdir;
    const Vec3<float> right = b.xdir * (b.max_tan_x + widen_tan) + b.zdir;
    const Vec3<float> top = b.ydir * (b.min_tan_y - widen_tan) + b.zdir;
    const Vec3<float> bottom = b.ydir * (b.max_tan_y + widen_tan) + b.zdir;

    return Frustum<TSpectral>({b.zdir,
                               glm::normalize(glm::cross(b.ydir, left)),
                               glm::normalize(glm::cross(right, b.ydir)),
                               glm::normalize(glm::cross(top, b.xdir)),
                               glm::normalize(glm::cross(b.xdir, bottom))});
}

template <IsSpectral TSpectral>
Frustum<TSpectral> CameraModel<TSpectral>::view_frustum_with_margin_(float pixels) const
{
    ensure_frustum_();
    // A pixel can span a larger angle beyond the boundary than at it (barrel distortion
    // compresses the edge of the field), so the margin is doubled to be safe. Sources that the
    // wider frustum lets through but whose light lands nowhere in the image are dropped once
    // projected.
    return frustum_from_bounds_(2.f * pixels * frustum_bounds_.tan_per_pixel);
}
} // namespace huira
