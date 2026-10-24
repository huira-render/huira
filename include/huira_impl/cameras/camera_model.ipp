
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "huira/cameras/apertures/circular_aperture.hpp"
#include "huira/cameras/psfs/airy_disk.hpp"
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

/**
 * @brief Convolve an image in place with convolve(image), leaving out the pixels that are not
 * finite (NaN or infinite in any channel), and warn if there were any.
 *
 * A convolution spreads every pixel over the kernel, and one through FFTs mixes every pixel
 * into every other: a single NaN pixel would make the whole image NaN. The pixels that are not
 * finite are taken as no light for the convolution, and put back as they were afterwards, so
 * that they stay where they are (the sensor's readout reports them), and the rest of the image
 * is convolved as if they were black.
 */
template <IsSpectral TSpectral, typename Convolve>
void convolve_finite_pixels(Image<TSpectral>& image, const char* what, Convolve&& convolve)
{
    std::vector<std::pair<std::size_t, TSpectral>> not_finite;
    for (std::size_t i = 0; i < image.size(); ++i) {
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            if (!std::isfinite(image[i][c])) {
                not_finite.emplace_back(i, image[i]);
                image[i] = TSpectral{0.f};
                break;
            }
        }
    }

    convolve(image);

    if (!not_finite.empty()) {
        for (const auto& [i, value] : not_finite) {
            image[i] = value;
        }
        HUIRA_LOG_WARNING(std::string{"CameraModel - "} + std::to_string(not_finite.size()) +
                          " pixel(s) were not finite (NaN or infinite) before the " + what +
                          " convolution. They were left out of it, so whatever light they had "
                          "is missing from the pixels around them, and they stay as they were.");
    }
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
    resize_default_aperture_(f);

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
    resize_default_aperture_(f); // changes nothing if it throws, and nothing after it does

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
 * @brief Set the sensor's roll: its rotation about the optical axis, relative to the camera.
 *
 * The sensor's x axis is turned by the angle toward its y axis, so the scene turns the other
 * way in the image: with OpenCV's axes (the default; x right, y down), a positive roll turns the
 * scene counterclockwise in the image. The camera's pose, and so the direction it looks, is
 * unchanged. Projections and rays (project_point(), cast_ray()) are in the sensor's axes.
 *
 * @param angle The roll angle (any angle unit).
 * @throws std::runtime_error if the angle is not finite.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_sensor_roll(units::Radian angle)
{
    sensor_->set_roll(angle);
}

/**
 * @brief The sensor's roll about the optical axis. See set_sensor_roll().
 */
template <IsSpectral TSpectral>
units::Radian CameraModel<TSpectral>::sensor_roll() const
{
    return sensor_->roll();
}

/**
 * @brief The sensor's orientation relative to the camera: the rotation from the sensor's axes,
 * in which projections and rays are given, to the camera's.
 *
 * It is the roll about the optical axis (see set_sensor_roll()); a tilted sensor will add to
 * it.
 */
template <IsSpectral TSpectral>
Rotation<double> CameraModel<TSpectral>::sensor_orientation() const
{
    Mat3<double> rot_matrix = Rotation<double>::local_to_parent_z(sensor_->roll());
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
 * @throws std::runtime_error if the aperture's area or bounding radius is not positive and
 *         finite; the camera then keeps the aperture it had.
 */
template <IsSpectral TSpectral>
template <IsAperture TAperture, typename... Args>
void CameraModel<TSpectral>::set_aperture(Args&&... args)
{
    auto aperture = std::make_unique<TAperture>(std::forward<Args>(args)...);
    // An aperture of the user's own may not check itself:
    const double area = aperture->get_area().to_si();
    const double radius = aperture->get_bounding_radius().to_si();
    if (!(area > 0.0) || !std::isfinite(area) || !(radius > 0.0) || !std::isfinite(radius)) {
        HUIRA_THROW_ERROR("CameraModel::set_aperture - The aperture's area and bounding radius "
                          "must be positive and finite: " +
                          std::to_string(area) + " m^2, " + std::to_string(radius) + " m");
    }
    aperture_ = std::move(aperture);
    aperture_source_ = ApertureSource::Diameter;
    optics_changed_(OpticsInput::Aperture);
}

/**
 * @brief Turn the aperture's diffraction pattern on (the default) or off.
 *
 * On, unresolved sources are drawn with the diffraction pattern of the aperture, averaged evenly
 * over each spectral bin's wavelengths, out to where their light fades (see
 * Renderer::set_unresolved_taper()), and resolved bodies are convolved with it across the frame
 * (see enable_psf_convolution()). Off, an unresolved source in focus puts all the light that
 * diffraction would spread into the pixel it falls in. Either way scattered light (see
 * set_scatter()) and the defocus at each source's depth (see set_focus_distance()) are added.
 *
 * A PSF that was set (see set_psf()) takes the diffraction pattern's place, whether it is on or
 * off; clear_psf() goes back to it. See psf_image() for the PSF as the renderer draws it, and
 * diffraction_image() for the pattern alone.
 *
 * @param diffraction True to turn the diffraction pattern on, false to turn it off.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::enable_diffraction(bool diffraction)
{
    if (diffraction == diffraction_enabled_) {
        return;
    }
    diffraction_enabled_ = diffraction;
    // A PSF that was set is drawn either way, until it is cleared:
    if (psf_ == nullptr) {
        optics_changed_(OpticsInput::CorePSF);
    }
}

/**
 * @brief Set a point spread function (PSF) in place of the aperture's diffraction pattern.
 *
 * The PSF is taken as complete within its radius (see PSF::radius()); one that does not set its
 * radius gets DEFAULT_PSF_RADIUS. Unresolved sources are drawn from it on a grid (see GridPsf),
 * tapered as with diffraction (see Renderer::set_unresolved_taper()): a still one at its exact
 * position, and a moving one along its path, as line integrals (see GridPsf::draw_line()). Out
 * of focus, it is blurred by the defocus at the source's depth, as diffraction is (see
 * psf_image()), unless it already holds it (PSF::includes_defocus(), as a MeasuredPSF can with
 * PSFFocus::IncludesDefocus). Scattered light, if set, is added to it.
 *
 * It stays in place until clear_psf(), whether diffraction is on or off (see
 * enable_diffraction()). It is put on its grid here rather than at the next render, which
 * checks every sample, and takes about a tenth of a second at the default radius.
 *
 * @tparam TPSF PSF model type
 * @tparam Args Constructor arguments for the PSF
 * @param args Arguments to construct the PSF
 * @throws std::runtime_error if the PSF's constructor throws, or any of its samples within its
 *         radius is negative or not finite, or they hold no light in a channel (see GridPsf). The
 *         camera then keeps the PSF it had.
 */
template <IsSpectral TSpectral>
template <IsPSF TPSF, typename... Args>
void CameraModel<TSpectral>::set_psf(Args&&... args)
{
    auto psf = std::make_unique<TPSF>(std::forward<Args>(args)...);
    if (psf->radius() < 1) {
        psf->set_radius(DEFAULT_PSF_RADIUS);
    }
    // Put on its grid now, which checks every sample, so that a PSF that is not finite, is
    // negative or holds no light is refused here, and the camera keeps the one it had:
    const Vec2<float> pitch = sensor_->pixel_pitch();
    auto grid = std::make_shared<const GridPsf<TSpectral>>(
        *psf, static_cast<double>(pitch.y) / static_cast<double>(pitch.x));
    if (psf->includes_defocus()) {
        HUIRA_LOG_WARNING("CameraModel::set_psf - The PSF already holds the camera's defocus, so "
                          "none is added to it. Resolved bodies are still traced with depth of "
                          "field and then blurred by this PSF too, so their defocus is "
                          "approximate at best.");
    }
    std::lock_guard<std::mutex> lock(optics_mutex_);
    psf_ = std::move(psf);
    optics_changed_(OpticsInput::CorePSF);
    focused_grid_ = std::move(grid);
    grid_source_built_at_ = optics_version_;
    // Blurred from it as a star is when next needed:
    grid_psf_ = nullptr;
}

/**
 * @brief Set a measured (user-supplied) PSF in place of the aperture's diffraction pattern.
 *
 * Convenience wrapper around set_psf<MeasuredPSF>() that is also exposed through the Python
 * bindings. See MeasuredPSF for the data conventions (centered measurement, sampling density).
 * The measurement is taken whole, as holding all of a source's light out to its farthest
 * sample; crop it to use less.
 *
 * @param data Measured PSF samples, centered on the image.
 * @param samples_per_pixel Measurement samples per sensor pixel per axis.
 * @param sampling What the samples are: the PSF's intensity at points, or the light a pixel
 *                 centered there receives, such as a star image taken with the sensor itself
 *                 (see PSFSampling).
 * @param focus Whether the samples are a point's in focus, which the camera's focus blurs as it
 *              blurs diffraction, or already hold the defocus the camera gives (see PSFFocus).
 * @throws std::runtime_error if the data is not a valid measurement (see MeasuredPSF); the
 *         camera then keeps the PSF it had.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_measured_psf(const Image<TSpectral>& data,
                                              float samples_per_pixel,
                                              PSFSampling sampling,
                                              PSFFocus focus)
{
    this->set_psf<MeasuredPSF<TSpectral>>(data, samples_per_pixel, sampling, focus);
}

/**
 * @brief Remove a PSF set with set_psf() or set_measured_psf(): unresolved sources are drawn
 * with the aperture's diffraction pattern again, or without one if it is off (see
 * enable_diffraction()).
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::clear_psf()
{
    if (psf_ == nullptr) {
        return;
    }
    psf_ = nullptr;
    optics_changed_(OpticsInput::CorePSF);
}

/**
 * @brief Checks the arguments the PSF's images take, throwing with the caller's name.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::check_image_arguments_(const char* caller,
                                                    int radius,
                                                    float x_offset,
                                                    float y_offset,
                                                    std::optional<units::Meter> range) const
{
    const std::string name = std::string("CameraModel::") + caller + " - ";
    if (range.has_value() && !(range->to_si() > 0.0)) {
        HUIRA_THROW_ERROR(name + "The range must be positive: " + std::to_string(range->to_si()) +
                          " m");
    }
    if (radius < 0 || radius > MAX_PSF_IMAGE_RADIUS) {
        HUIRA_THROW_ERROR(name + "The radius must be from 0 to " +
                          std::to_string(MAX_PSF_IMAGE_RADIUS) + ": " + std::to_string(radius));
    }
    if (!std::isfinite(x_offset) || !std::isfinite(y_offset)) {
        HUIRA_THROW_ERROR(name + "The offsets must be finite: " + std::to_string(x_offset) + ", " +
                          std::to_string(y_offset));
    }
}

/**
 * @brief The light an unresolved source puts in each pixel around it, per channel, as the
 * renderer draws it.
 *
 * Each pixel holds the fraction of the source's light, in each channel, that falls on it: the
 * PSF integrated over the pixel, for the source at its exact position. The PSF is the aperture's
 * diffraction pattern (see enable_diffraction()), averaged evenly over each spectral bin's
 * wavelengths, for a circular aperture, with scattered light set with set_scatter(), blurred by
 * defocus as the focus setting blurs a point at the given range: a uniform disc, the aperture's
 * image out of focus (see set_focus_distance()). Without a range the point is a star, at
 * infinity. There is no blur with depth of field off (see enable_depth_of_field()). As the
 * renderer draws unresolved objects, a source whose blur is a star's within
 * PsfTables::BLUR_TOLERANCE is given a star's tables. See PsfTables for how it is computed and
 * how accurate it is.
 *
 * A PSF set with set_psf() or set_measured_psf() is given as the renderer draws a still source
 * with it (see GridPsf), blurred by the same defocus unless the PSF already holds it (see
 * PSFFocus); with diffraction off and no PSF set, the defocus blur alone, a uniform disc (see
 * GridPsf::disc()), or in focus the pixel the source falls in, holds that share of its light.
 * Either way the scattered light's share, if any, is from its own tables.
 *
 * Resolved bodies are convolved with this PSF in focus (see enable_psf_convolution()). The
 * components are given alone by diffraction_image(), scatter_image() and defocus_image().
 *
 * The tables for stars are built the first time they are needed after the optics change: the
 * focal length, aperture, pixel pitch, resolution, scattered light or focus. Tables for another
 * blur are built the first time it is needed, and the last few are kept, shared with the
 * renderer. Either can take a second or two.
 *
 * @param radius The image is 2 radius + 1 pixels square, centered on the pixel the source is
 *        offset from.
 * @param x_offset The source's position right of the center pixel's center, in pixels.
 * @param y_offset Its position below the center pixel's center, in pixels.
 * @param range The source's distance along the camera's axis, which sets its defocus; none for
 *        a star.
 * @return The image. Each channel sums to the part of the source's light that falls within it.
 * @throws std::runtime_error if the radius is negative or over MAX_PSF_IMAGE_RADIUS, an offset
 *         is not finite, or the range is not positive.
 */
template <IsSpectral TSpectral>
Image<TSpectral> CameraModel<TSpectral>::psf_image(int radius,
                                                   float x_offset,
                                                   float y_offset,
                                                   std::optional<units::Meter> range)
{
    check_image_arguments_("psf_image", radius, x_offset, y_offset, range);
    const double inverse_depth = range.has_value() ? 1.0 / range->to_si() : 0.0;
    std::shared_ptr<const PsfTables<TSpectral>> tables;
    std::shared_ptr<const GridPsf<TSpectral>> grid;
    {
        std::lock_guard<std::mutex> lock(optics_mutex_);
        tbb::this_task_arena::isolate([&] {
            if (uses_tables_()) {
                tables = tables_at_locked_({inverse_depth}).front();
            }
            if (!uses_diffraction_()) {
                grid = grids_at_locked_({inverse_depth}).front();
            }
        });
    }
    if (uses_diffraction_()) {
        return tables->image(radius, static_cast<double>(x_offset), static_cast<double>(y_offset));
    }

    // Otherwise the scattered light's share from its tables, and the rest from the PSF that
    // was set, or without one the defocus blur, as the renderer draws a still source:
    const double scattered = scatter_enabled_ ? static_cast<double>(scatter_fraction_) : 0.0;
    const int size = 2 * radius + 1;
    Image<TSpectral> image(size, size, TSpectral{0.f});
    if (tables != nullptr) {
        image = tables->image(radius, static_cast<double>(x_offset), static_cast<double>(y_offset));
        for (std::size_t i = 0; i < image.size(); ++i) {
            image[i] = image[i] * static_cast<float>(scattered);
        }
    }
    const auto core = static_cast<float>(1.0 - scattered);
    // The center pixel covers [0, 1) x [0, 1):
    const auto scale =
        grid->scale(0.5 + static_cast<double>(x_offset), 0.5 + static_cast<double>(y_offset));
    tbb::parallel_for(0, size, [&](int y) {
        for (int x = 0; x < size; ++x) {
            image(x, y) += grid->pixel(static_cast<double>(x - radius) - x_offset,
                                       static_cast<double>(y - radius) - y_offset,
                                       scale) *
                           core;
        }
    });
    return image;
}

/**
 * @brief The aperture's diffraction pattern alone, in focus, as psf_image() gives the light in
 * each pixel: averaged evenly over each spectral bin's wavelengths, for a circular aperture of
 * the aperture's bounding radius, at the current focal length and pixel pitch.
 *
 * It is the aperture's whether or not diffraction is on and whether or not a PSF was set in
 * its place (see enable_diffraction()), without scattered light or defocus. Its tables are built
 * for each call, which can take a second.
 *
 * @param radius The image is 2 radius + 1 pixels square, centered on the pixel the source is
 *        offset from.
 * @param x_offset The source's position right of the center pixel's center, in pixels.
 * @param y_offset Its position below the center pixel's center, in pixels.
 * @return The image. Each channel sums to the part of the pattern's light that falls within it.
 * @throws std::runtime_error if the radius is negative or over MAX_PSF_IMAGE_RADIUS, or an
 *         offset is not finite.
 */
template <IsSpectral TSpectral>
Image<TSpectral>
CameraModel<TSpectral>::diffraction_image(int radius, float x_offset, float y_offset) const
{
    check_image_arguments_("diffraction_image", radius, x_offset, y_offset, std::nullopt);
    const std::optional<typename PsfTables<TSpectral>::Scatter> none;
    const auto tables = build_psf_tables_(0.0, none, image_reach_(radius));
    return tables->image(radius, static_cast<double>(x_offset), static_cast<double>(y_offset));
}

/**
 * @brief The scattered light alone (see set_scatter()), in focus, as psf_image() gives the
 * light in each pixel: its profile, at its size in pixels at the current focal length and pixel
 * pitch, holding all of the light, not just the scattered fraction.
 *
 * @param radius The image is 2 radius + 1 pixels square, centered on the pixel the source is
 *        offset from.
 * @param x_offset The source's position right of the center pixel's center, in pixels.
 * @param y_offset Its position below the center pixel's center, in pixels.
 * @return The image. Each channel sums to the part of the scattered light that falls within it.
 * @throws std::runtime_error if there is no scattered light, the radius is negative or over
 *         MAX_PSF_IMAGE_RADIUS, or an offset is not finite.
 */
template <IsSpectral TSpectral>
Image<TSpectral>
CameraModel<TSpectral>::scatter_image(int radius, float x_offset, float y_offset) const
{
    check_image_arguments_("scatter_image", radius, x_offset, y_offset, std::nullopt);
    auto scatter = scatter_for_tables_();
    if (!scatter.has_value()) {
        HUIRA_THROW_ERROR("CameraModel::scatter_image - There is no scattered light: see "
                          "set_scatter()");
    }
    scatter->fraction = 1.0;
    const auto tables = build_psf_tables_(0.0, scatter, image_reach_(radius));
    return tables->image(radius, static_cast<double>(x_offset), static_cast<double>(y_offset));
}

/**
 * @brief The defocus blur alone that the focus gives a point at a range, as psf_image() gives
 * the light in each pixel: a uniform disc of radius defocus_blur_radius(range), each pixel
 * holding the share of it that falls on the pixel.
 *
 * In focus, or with depth of field off, the pixel the point falls in holds all of it. A PSF that
 * was set and already holds the defocus (see PSFFocus) has none added to it when drawn; this
 * still gives the camera's.
 *
 * @param radius The image is 2 radius + 1 pixels square, centered on the pixel the point is
 *        offset from.
 * @param x_offset The point's position right of the center pixel's center, in pixels.
 * @param y_offset Its position below the center pixel's center, in pixels.
 * @param range The point's distance along the camera's axis; none for a star.
 * @return The image. It sums to the part of the disc that falls within it.
 * @throws std::runtime_error if the radius is negative or over MAX_PSF_IMAGE_RADIUS, an offset
 *         is not finite, or the range is not positive.
 */
template <IsSpectral TSpectral>
Image<TSpectral> CameraModel<TSpectral>::defocus_image(int radius,
                                                       float x_offset,
                                                       float y_offset,
                                                       std::optional<units::Meter> range) const
{
    check_image_arguments_("defocus_image", radius, x_offset, y_offset, range);
    double blur = blur_pixels_(range.has_value() ? 1.0 / range->to_si() : 0.0);
    if (blur < PsfTables<TSpectral>::MIN_BLUR) {
        blur = 0.0;
    }
    const int size = 2 * radius + 1;
    Image<TSpectral> image(size, size, TSpectral{0.f});
    if (blur == 0.0) {
        // In double, which holds any offset, until it is known to be within the image:
        const double x = std::floor(static_cast<double>(x_offset) + 0.5) + radius;
        const double y = std::floor(static_cast<double>(y_offset) + 0.5) + radius;
        if (x >= 0.0 && x < size && y >= 0.0 && y < size) {
            image(static_cast<int>(x), static_cast<int>(y)) = TSpectral{1.f};
        }
        return image;
    }
    // In pixel widths, a pixel being aspect widths high:
    const Vec2<float> pitch = sensor_->pixel_pitch();
    const double aspect = static_cast<double>(pitch.y) / static_cast<double>(pitch.x);
    const double area = PI<double>() * blur * blur;
    tbb::parallel_for(0, size, [&](int y) {
        for (int x = 0; x < size; ++x) {
            const double dx = static_cast<double>(x - radius) - static_cast<double>(x_offset);
            const double dy =
                aspect * (static_cast<double>(y - radius) - static_cast<double>(y_offset));
            const double light = grid_psf_detail::circle_rectangle_area(
                dx - 0.5, dx + 0.5, dy - 0.5 * aspect, dy + 0.5 * aspect, blur);
            image(x, y) = TSpectral{static_cast<float>(light / area)};
        }
    });
    return image;
}

/**
 * @brief How far the tables for an image of the given radius need to reach: past its corners,
 * at least 64 pixels.
 */
template <IsSpectral TSpectral>
double CameraModel<TSpectral>::image_reach_(int radius) const
{
    const Vec2<float> pitch = sensor_->pixel_pitch();
    const double aspect = static_cast<double>(pitch.y) / static_cast<double>(pitch.x);
    const double corner =
        std::hypot(static_cast<double>(radius) + 1.0, aspect * (static_cast<double>(radius) + 1.0));
    return std::max(64.0, corner + 2.0);
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
 * @brief Set scattered light: Harvey-Shack wings around every source, in angles.
 *
 * A fraction of each source's light leaves the diffraction pattern and spreads as
 *
 *     S(theta) = K (1 + (theta / theta_0)^2)^(-slope / 2) (1 + (theta / theta_1)^2)^(-1)
 *
 * with theta the angle from the source, theta_0 the shoulder angle and theta_1 the outer angle:
 * flat within the shoulder, falling as theta^-slope beyond it, and as theta^-(slope + 2) beyond
 * the outer angle, if one is given. K makes the scattered light total the fraction. Measured
 * stray light around stars often falls with a slope of 2 to 3 (King 1971, Racine 1996).
 *
 * Without an outer angle the light totals a finite amount only for a slope over 2, so a slope of
 * 2 or less needs one. The outer angle bends the wings down smoothly rather than cutting them
 * off, so that they leave no edge in an image. An angle theta lands f theta from the source on
 * the sensor, within 0.1% below 3 degrees.
 *
 * The scattered light follows the focal length and pixel pitch. It is included in psf_image(),
 * and given alone by scatter_image(). Unresolved sources, still or moving, are drawn from tables
 * that hold it: the diffraction pattern's, or with a PSF set in its place or none, its own,
 * blurred by each source's defocus; resolved bodies are convolved with it across the frame (see
 * enable_psf_convolution()). disable_scatter() turns it off.
 *
 * @param fraction Fraction of the light scattered, in [0, 1); 0 for none.
 * @param slope How fast the wings fall beyond the shoulder; positive.
 * @param shoulder_angle The angle within which the wings are flat; positive.
 * @param outer_angle The angle beyond which they fall faster, beyond the shoulder; none by
 *        default.
 * @throws std::runtime_error if an argument is out of range, an angle is not below 90 degrees,
 *         or the slope is 2 or less without an outer angle.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::set_scatter(float fraction,
                                         float slope,
                                         units::Radian shoulder_angle,
                                         std::optional<units::Radian> outer_angle)
{
    const std::string caller = "CameraModel::set_scatter - ";
    const double shoulder = shoulder_angle.to_si();
    const double outer = outer_angle.has_value() ? outer_angle->to_si() : 0.0;
    const double right_angle = 0.5 * PI<double>();
    if (!(fraction >= 0.f && fraction < 1.f)) {
        HUIRA_THROW_ERROR(caller + "The fraction must be in [0, 1): " + std::to_string(fraction));
    }
    if (!(slope > 0.f) || !std::isfinite(slope)) {
        HUIRA_THROW_ERROR(caller +
                          "The slope must be positive and finite: " + std::to_string(slope));
    }
    if (!(shoulder > 0.0 && shoulder < right_angle)) {
        HUIRA_THROW_ERROR(caller + "The shoulder angle must be above 0 and below 90 degrees: " +
                          std::to_string(shoulder) + " rad");
    }
    if (outer_angle.has_value() && !(outer > shoulder && outer < right_angle)) {
        HUIRA_THROW_ERROR(caller +
                          "The outer angle must be beyond the shoulder and below 90 "
                          "degrees: " +
                          std::to_string(outer) + " rad, shoulder " + std::to_string(shoulder) +
                          " rad");
    }
    if (!outer_angle.has_value() && !(slope > 2.f)) {
        HUIRA_THROW_ERROR(caller +
                          "A slope of 2 or less needs an outer angle, or the scattered "
                          "light has no finite total: slope " +
                          std::to_string(slope));
    }
    if (fraction == scatter_fraction_ && slope == scatter_slope_ &&
        shoulder == scatter_shoulder_angle_ && outer == scatter_outer_angle_) {
        return;
    }
    scatter_fraction_ = fraction;
    scatter_slope_ = slope;
    scatter_shoulder_angle_ = shoulder;
    scatter_outer_angle_ = outer;
    scatter_enabled_ = fraction > 0.f;
    optics_changed_(OpticsInput::Scatter);
}

/**
 * @brief Turn scattered light off (the default), as set_scatter() with a fraction of 0 does.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::disable_scatter()
{
    if (!scatter_enabled_) {
        return;
    }
    scatter_fraction_ = 0.f;
    scatter_enabled_ = false;
    optics_changed_(OpticsInput::Scatter);
}

/**
 * @brief Build everything the camera's optics and geometry need for the next render, now.
 *
 * The kernels a render uses are derived from the camera's settings: the diffraction pattern's
 * tables for unresolved sources, with the defocus blur and scattered light (see
 * enable_diffraction(), set_focus_distance() and set_scatter()), or a PSF that was set, or the
 * defocus blur without either, on its grid, blurred as a star is by the focus (see set_psf()),
 * with the scattered light's own tables, and the convolution of resolved bodies with the whole
 * PSF (see enable_psf_convolution()). So are the tables of the camera's geometry: each pixel's
 * ray direction with distortion, and its solid angle, and the view
 * frustum, which depend on the focal length, sensor, intrinsics, distortion and axis
 * convention. The setters only record settings, so they are cheap and can be made in any
 * order, and each kernel or table is rebuilt only when a setting it depends on has changed:
 * refocusing while a star's blur stays under PsfTables::MIN_BLUR, for example, rebuilds nothing.
 *
 * Building can take a while, for the PSF's tables or a large set PSF, or a large sensor with
 * distortion. Calling this once the camera is configured keeps that time out of the first
 * render, so that every render takes comparable time, as a timed or hardware-in-the-loop run
 * needs.
 *
 * Without it, a render builds whatever is out of date itself and logs the time taken: as a
 * warning the first time after precompute() was called (the settings have then changed
 * since), and as information otherwise. enable_auto_precompute(false) makes the render throw
 * instead.
 *
 * Only what the current settings use is built: for example, no tables of the diffraction pattern
 * while a PSF is set in its place. Tables and grids for unresolved objects whose blur differs from
 * a star's are built by the render that first needs them (see psf_image()).
 *
 * @throws std::runtime_error if the lens distortion has no inverse somewhere in the image at this
 *         focal length and sensor (see set_distortion()).
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::precompute()
{
    std::lock_guard<std::mutex> lock(optics_mutex_);
    explicitly_precomputed_ = true;
    warned_object_build_ = false;
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
 * @brief Choose what a render does when the camera's geometry or optics are out of date.
 *
 * Enabled by default: the render builds them, as precompute() would, and logs the time taken.
 * Disabled, the render throws instead, which keeps that time out of every render: a timed or
 * hardware-in-the-loop run can disable it and call precompute() after every change.
 *
 * The exception is an unresolved object whose defocus blur is not a star's (see psf_image()):
 * what it is drawn from depends on its depth, which only the render knows, so the render builds
 * it the first time that blur is needed either way, which for the aperture's diffraction can
 * take a second or two. Disabled, the render warns when it does, once until precompute() is
 * called again. Objects far enough away share the stars'.
 *
 * @param auto_precompute True for the render to build what is out of date, false to throw.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::enable_auto_precompute(bool auto_precompute)
{
    auto_precompute_ = auto_precompute;
}

/**
 * @brief Called by the renderer before each render: makes sure the optics kernels are up to
 * date, as enable_auto_precompute() chooses.
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
    // With the aperture's diffraction pattern, unresolved sources are drawn from its tables,
    // which hold the defocus blur and the scattered light: still ones at their position, and
    // moving ones along their path.
    //
    // With a PSF that was set, its share of each source is drawn from it on its grid, blurred by
    // the source's defocus: at its position if still, and along its path if moving. With neither,
    // it is the defocus blur alone, or in focus a point, drawn the same way (see
    // GridPsf::disc()). The scattered light, if any, is drawn from its own tables, as
    // diffraction's are.
    //
    // Resolved bodies are convolved with the whole PSF across the frame, in focus, unless
    // enable_psf_convolution(false).
    if (!uses_diffraction_()) {
        ensure_grid_psf_();
    }
    if (uses_tables_()) {
        ensure_render_tables_();
    }
    if (convolves_bodies_()) {
        ensure_body_convolver_();
    }
}

/**
 * @brief Whether precompute_locked_() has nothing to do. Mirrors it.
 */
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::is_precomputed_locked_() const
{
    if (!uses_diffraction_() && grid_psf_stale_()) {
        return false;
    }
    if (uses_tables_() && render_tables_stale_()) {
        return false;
    }
    return !convolves_bodies_() || !body_convolver_stale_();
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

/// Whether the grids are out of date: the PSF that was set (or that there is none) or the pixels'
/// shape changed, or the blur a star gets.
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::grid_psf_stale_() const
{
    if (uses_diffraction_()) {
        return false;
    }
    const double blur = source_blur_(0.0);
    if (stale_(grid_source_built_at_, {OpticsInput::CorePSF, OpticsInput::PixelPitch}) ||
        grid_psf_blur_ != blur) {
        return true;
    }
    return focused_grid_ == nullptr || grid_psf_ == nullptr;
}

/**
 * @brief Put a PSF set with set_psf() or set_measured_psf() on its grid, in focus and blurred as
 * a star is by the focus, if out of date (see GridPsf). Without a PSF, a star's defocus blur
 * alone, a disc, or in focus a point, is drawn from a GridPsf of its own (see GridPsf::disc()).
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_grid_psf_()
{
    if (uses_diffraction_() || !grid_psf_stale_()) {
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    const Vec2<float> pitch = sensor_->pixel_pitch();
    const double aspect = static_cast<double>(pitch.y) / static_cast<double>(pitch.x);
    if (stale_(grid_source_built_at_, {OpticsInput::CorePSF, OpticsInput::PixelPitch})) {
        focused_grid_ =
            psf_ != nullptr
                ? std::make_shared<const GridPsf<TSpectral>>(*psf_, aspect)
                : std::make_shared<const GridPsf<TSpectral>>(GridPsf<TSpectral>::disc(0.0, aspect));
        grid_source_built_at_ = optics_version_;
    }
    const double blur = source_blur_(0.0);
    grid_psf_ = blur == 0.0 ? focused_grid_ : blurred_grid_(blur, aspect);
    grid_psf_blur_ = blur;
    if (grid_psf_->exact()) {
        return;
    }

    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    std::ostringstream message;
    message << "CameraModel - Put the " << (psf_ != nullptr ? "PSF" : "defocus blur")
            << " on its grid in " << std::fixed << std::setprecision(2) << elapsed.count()
            << " seconds: " << static_cast<double>(grid_psf_->memory_bytes()) / (1024.0 * 1024.0)
            << " MiB, a sample every " << grid_psf_->step() << " pixels";
    if (blur > 0.0) {
        message << ", blurred by " << blur << " pixels";
    }
    HUIRA_LOG_INFO(message.str());
}

/// Warns, with auto precompute off, that what an unresolved object is drawn from for its own
/// blur is being built, as it must be (see enable_auto_precompute()): once until precompute() is
/// called again. optics_mutex_ must be held.
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::note_object_build_()
{
    if (auto_precompute_ || warned_object_build_) {
        return;
    }
    warned_object_build_ = true;
    HUIRA_LOG_WARNING("CameraModel - Building the PSF for an unresolved object's own defocus "
                      "blur, which depends on its depth, so that even with auto precompute "
                      "disabled the render builds it the first time that blur is needed. Objects "
                      "whose blur is a star's share the stars'.");
}

/// The grid a source with the given blur, in pixel widths, is drawn from: the PSF that was set,
/// blurred by it, or without one the disc alone (see GridPsf::disc()).
template <IsSpectral TSpectral>
std::shared_ptr<const GridPsf<TSpectral>> CameraModel<TSpectral>::blurred_grid_(double blur,
                                                                                double aspect) const
{
    if (psf_ != nullptr) {
        return std::make_shared<const GridPsf<TSpectral>>(*focused_grid_, blur);
    }
    return std::make_shared<const GridPsf<TSpectral>>(GridPsf<TSpectral>::disc(blur, aspect));
}

/**
 * @brief How far the tables reach: twice the frame's diagonal, since a source lights pixels up to
 * a diagonal away and can lie as far outside the frame, and at least 4096 pixels, so that
 * smaller frames share tables. Beyond, the far field continues as a power law, which is exact for
 * diffraction alone.
 */
template <IsSpectral TSpectral>
double CameraModel<TSpectral>::tables_reach_() const
{
    constexpr double SMALLEST_REACH = 4096.0;
    const Vec2<float> pitch = sensor_->pixel_pitch();
    const Resolution resolution = sensor_->resolution();
    const double aspect = static_cast<double>(pitch.y) / static_cast<double>(pitch.x);
    return std::max(SMALLEST_REACH,
                    2.0 * std::hypot(static_cast<double>(resolution.x),
                                     aspect * static_cast<double>(resolution.y)));
}

/**
 * @brief The blur, in pixel widths, the tables give a star: none below PsfTables::MIN_BLUR, where
 * they are the same as in focus.
 */
template <IsSpectral TSpectral>
double CameraModel<TSpectral>::tables_blur_() const
{
    return source_blur_(0.0);
}

/// Whether the renderer's tables are out of date. A smaller frame does not rebuild them.
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::render_tables_stale_() const
{
    return render_tables_ == nullptr ||
           stale_(render_tables_built_at_,
                  {OpticsInput::FocalLength,
                   OpticsInput::PixelPitch,
                   OpticsInput::Aperture,
                   OpticsInput::Scatter}) ||
           tables_blur_() != render_tables_blur_ || tables_reach_() > render_tables_reach_ ||
           render_tables_scatter_only_ != !uses_diffraction_();
}

/**
 * @brief Build the tables the renderer draws unresolved sources from, if out of date: the
 * aperture's diffraction with the scattered light, or the scattered light alone with another PSF
 * or none, blurred as a star is by the focus. See render_tables_.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_render_tables_()
{
    if (!render_tables_stale_()) {
        return;
    }
    // What the tables were built for is recorded only once they are, so that a build that
    // throws leaves them out of date:
    const double reach = tables_reach_();
    const double blur = tables_blur_();
    render_tables_ = build_psf_tables_(blur, tables_scatter_(), reach);
    render_tables_reach_ = reach;
    render_tables_blur_ = blur;
    render_tables_scatter_only_ = !uses_diffraction_();
    render_tables_built_at_ = optics_version_;
}

/**
 * @brief The tables for unresolved sources at depths (their distance along the axis, which
 * sets their defocus, as it does for path-traced bodies), given as their inverses: 0 for a
 * star. One call gives all that a render needs.
 *
 * A source whose blur is a star's, within PsfTables::BLUR_TOLERANCE, is drawn from the
 * renderer's tables: with the Jupiter camera (125 mm at f/3.3, 8.5 um pixels, 278.5 px of blur
 * per diopter), any at more than about 110,000 km focused at 10 m, or 2,800 km focused at
 * infinity, where blurs under MIN_BLUR count as none. Others get tables for their own blur,
 * built the first time a blur is needed (taking as long as the renderer's) and kept for later
 * sources and renders whose blur is within the tolerance of it: all that this call used, and
 * the most recently used others up to OBJECT_TABLES_KEPT_ in all. They are rebuilt when the
 * optics change.
 */
template <IsSpectral TSpectral>
std::vector<std::shared_ptr<const PsfTables<TSpectral>>>
CameraModel<TSpectral>::tables_at_(const std::vector<double>& inverse_depths)
{
    std::vector<std::shared_ptr<const PsfTables<TSpectral>>> tables;
    std::lock_guard<std::mutex> lock(optics_mutex_);
    tbb::this_task_arena::isolate([&] { tables = tables_at_locked_(inverse_depths); });
    return tables;
}

/// tables_at_(), with optics_mutex_ held.
template <IsSpectral TSpectral>
std::vector<std::shared_ptr<const PsfTables<TSpectral>>>
CameraModel<TSpectral>::tables_at_locked_(const std::vector<double>& inverse_depths)
{
    ensure_render_tables_();
    if (stale_(object_tables_built_at_,
               {OpticsInput::FocalLength,
                OpticsInput::PixelPitch,
                OpticsInput::Aperture,
                OpticsInput::Scatter}) ||
        tables_reach_() > object_tables_reach_ ||
        object_tables_scatter_only_ != !uses_diffraction_()) {
        object_tables_.entries.clear();
        object_tables_reach_ = tables_reach_();
        object_tables_scatter_only_ = !uses_diffraction_();
        object_tables_built_at_ = optics_version_;
    }

    std::vector<std::shared_ptr<const PsfTables<TSpectral>>> result;
    result.reserve(inverse_depths.size());
    std::size_t used = 0; // the last this many kept are those this call has used
    for (const double inverse_depth : inverse_depths) {
        const double blur = source_blur_(inverse_depth);
        if (!std::isfinite(blur) ||
            std::abs(blur - render_tables_blur_) <= PsfTables<TSpectral>::BLUR_TOLERANCE) {
            result.push_back(render_tables_);
            continue;
        }
        result.push_back(object_tables_.find(blur, used, [&] {
            note_object_build_();
            return build_psf_tables_(blur, tables_scatter_(), object_tables_reach_);
        }));
    }
    object_tables_.trim(used);
    return result;
}

/**
 * @brief The grids of a PSF that was set (see set_psf()) for unresolved sources at depths,
 * given as their inverses (0 for a star), as tables_at_() gives the aperture PSF's tables: the
 * PSF blurred by each one's defocus, unless the PSF already holds it (see
 * PSF::includes_defocus()). A source whose blur is a star's, within PsfTables::BLUR_TOLERANCE, is
 * drawn from the stars' grid, and one in focus from the PSF's own; others from grids for their
 * own blur, built the first time a blur is needed and kept as tables_at_() keeps tables. They
 * are rebuilt when the PSF or the pixels' shape changes.
 */
template <IsSpectral TSpectral>
std::vector<std::shared_ptr<const GridPsf<TSpectral>>>
CameraModel<TSpectral>::grids_at_(const std::vector<double>& inverse_depths)
{
    std::vector<std::shared_ptr<const GridPsf<TSpectral>>> grids;
    std::lock_guard<std::mutex> lock(optics_mutex_);
    tbb::this_task_arena::isolate([&] { grids = grids_at_locked_(inverse_depths); });
    return grids;
}

/// grids_at_(), with optics_mutex_ held.
template <IsSpectral TSpectral>
std::vector<std::shared_ptr<const GridPsf<TSpectral>>>
CameraModel<TSpectral>::grids_at_locked_(const std::vector<double>& inverse_depths)
{
    ensure_grid_psf_();
    if (object_grids_built_at_ < grid_source_built_at_) {
        object_grids_.entries.clear();
        object_grids_built_at_ = grid_source_built_at_;
    }
    std::vector<std::shared_ptr<const GridPsf<TSpectral>>> result;
    result.reserve(inverse_depths.size());
    std::size_t used = 0;
    for (const double inverse_depth : inverse_depths) {
        const double blur = source_blur_(inverse_depth);
        if (!std::isfinite(blur) ||
            std::abs(blur - grid_psf_blur_) <= PsfTables<TSpectral>::BLUR_TOLERANCE) {
            result.push_back(grid_psf_);
        } else if (blur == 0.0) {
            result.push_back(focused_grid_);
        } else {
            result.push_back(object_grids_.find(blur, used, [&] {
                // A disc drawn exactly takes nothing to build:
                if (psf_ != nullptr || blur >= GridPsf<TSpectral>::EXACT_DISC) {
                    note_object_build_();
                }
                const Vec2<float> pitch = sensor_->pixel_pitch();
                return blurred_grid_(blur,
                                     static_cast<double>(pitch.y) / static_cast<double>(pitch.x));
            }));
        }
    }
    object_grids_.trim(used);
    return result;
}

/**
 * @brief The blur, in pixel widths, an unresolved source is given at an inverse depth (0 for a
 * star): none below PsfTables::MIN_BLUR, or if the PSF already holds the defocus (see
 * PSF::includes_defocus()).
 */
template <IsSpectral TSpectral>
double CameraModel<TSpectral>::source_blur_(double inverse_depth) const
{
    if (psf_ != nullptr && psf_->includes_defocus()) {
        return 0.0;
    }
    const double blur = blur_pixels_(inverse_depth);
    return blur >= PsfTables<TSpectral>::MIN_BLUR ? blur : 0.0;
}

/// Whether the tables for resolved bodies are out of date: the renderer's, in focus. None are
/// needed without diffraction or scattered light.
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::body_tables_stale_() const
{
    if (!uses_tables_()) {
        return false;
    }
    if (tables_blur_() == 0.0) {
        return render_tables_stale_();
    }
    return body_tables_ == nullptr ||
           stale_(body_tables_built_at_,
                  {OpticsInput::FocalLength,
                   OpticsInput::PixelPitch,
                   OpticsInput::Aperture,
                   OpticsInput::Scatter}) ||
           tables_reach_() > body_tables_reach_ ||
           body_tables_scatter_only_ != !uses_diffraction_();
}

/**
 * @brief What resolved bodies are convolved with, in focus, since path tracing gives them their
 * defocus: the aperture's tables, which hold its diffraction and the scattered light; or a PSF
 * that was set on its grid, or a point without one, with 1 - f of the light, and the scattered
 * light's tables with f of it, f being the scattered fraction (0 without scattered light).
 */
template <IsSpectral TSpectral>
typename PsfConvolver<TSpectral>::Parts CameraModel<TSpectral>::body_parts_() const
{
    typename PsfConvolver<TSpectral>::Parts parts;
    if (uses_tables_()) {
        parts.tables = tables_blur_() == 0.0 ? render_tables_ : body_tables_;
    }
    if (uses_diffraction_()) {
        return parts;
    }
    const double scattered = scatter_enabled_ ? static_cast<double>(scatter_fraction_) : 0.0;
    parts.tables_share = scattered;
    parts.core = psf_ != nullptr ? focused_grid_ : nullptr;
    parts.core_share = 1.0 - scattered;
    return parts;
}

/// Whether the convolution for resolved bodies is out of date: its tables, its core, or the
/// resolution.
template <IsSpectral TSpectral>
bool CameraModel<TSpectral>::body_convolver_stale_() const
{
    if (body_tables_stale_()) {
        return true;
    }
    return !body_convolver_.is_ready() || !(body_convolver_.parts() == body_parts_()) ||
           !(body_convolver_.resolution() == sensor_->resolution());
}

/**
 * @brief Prepare the convolution for resolved bodies, if out of date: with the whole PSF (see
 * body_parts_()), from the renderer's tables in focus, or from tables of their own otherwise.
 * A PSF that was set must be on its grid (see ensure_grid_psf_()).
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::ensure_body_convolver_()
{
    if (!body_convolver_stale_()) {
        return;
    }
    if (uses_tables_()) {
        if (tables_blur_() == 0.0) {
            ensure_render_tables_();
            body_tables_ = nullptr;
        } else if (body_tables_stale_()) {
            const double reach = tables_reach_();
            body_tables_ = build_psf_tables_(0.0, tables_scatter_(), reach);
            body_tables_reach_ = reach;
            body_tables_scatter_only_ = !uses_diffraction_();
            body_tables_built_at_ = optics_version_;
        }
    }
    const auto start = std::chrono::steady_clock::now();
    body_convolver_.set_parts(body_parts_(), sensor_->resolution());

    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    std::ostringstream message;
    message << "CameraModel - Prepared the convolution of resolved bodies in " << std::fixed
            << std::setprecision(2) << elapsed.count() << " seconds: near part to "
            << body_convolver_.near_radius() << " pixels, coarse grid every "
            << body_convolver_.step()[0] << " pixels";
    HUIRA_LOG_INFO(message.str());
}

/**
 * @brief The radius, in pixel widths, of the defocus blur of a point at 1 / inverse_range
 * (0 for infinity): the aperture's radius times the focal length times the difference in
 * vergence between the point and the focus. See defocus_blur_radius(), which this is without
 * its threshold. 0 with depth of field off.
 */
template <IsSpectral TSpectral>
double CameraModel<TSpectral>::blur_pixels_(double inverse_range) const
{
    if (!depth_of_field_) {
        return 0.0;
    }
    const double vergence = focus_diopters().to_si() - inverse_range;
    return std::abs(vergence) * static_cast<double>(focal_length_) *
           aperture_->get_bounding_radius().to_si() / static_cast<double>(sensor_->pixel_pitch().x);
}

/**
 * @brief The scattered light in pixel widths for the tables, as set with set_scatter(); none if
 * it is off.
 */
template <IsSpectral TSpectral>
std::optional<typename PsfTables<TSpectral>::Scatter>
CameraModel<TSpectral>::scatter_for_tables_() const
{
    if (!scatter_enabled_) {
        return std::nullopt;
    }
    // An angle theta from the source lands f theta from it on the sensor.
    const double pixels_per_radian =
        static_cast<double>(focal_length_) / static_cast<double>(sensor_->pixel_pitch().x);
    return typename PsfTables<TSpectral>::Scatter{
        static_cast<double>(scatter_fraction_),
        static_cast<double>(scatter_slope_),
        scatter_shoulder_angle_ * pixels_per_radian,
        scatter_outer_angle_ > 0.0 ? std::optional<double>(scatter_outer_angle_ * pixels_per_radian)
                                   : std::nullopt};
}

/**
 * @brief The scattered light the renderer's tables hold: as set with the diffraction pattern, which
 * they hold too; and otherwise all of their light, the scattered light alone, which the renderer
 * gives its fraction (see scatter_for_tables_()).
 */
template <IsSpectral TSpectral>
std::optional<typename PsfTables<TSpectral>::Scatter>
CameraModel<TSpectral>::tables_scatter_() const
{
    auto scatter = scatter_for_tables_();
    if (scatter.has_value() && !uses_diffraction_()) {
        scatter->fraction = 1.0;
    }
    return scatter;
}

/**
 * @brief Build the tables for the current optics, the given blur (in pixel widths) and scattered
 * light, out to the given reach (see tables_reach_()). With all of the light scattered, the
 * diffraction pattern is left out.
 */
template <IsSpectral TSpectral>
std::shared_ptr<const PsfTables<TSpectral>> CameraModel<TSpectral>::build_psf_tables_(
    double blur,
    const std::optional<typename PsfTables<TSpectral>::Scatter>& scatter,
    double reach) const
{
    const auto start = std::chrono::steady_clock::now();
    const Vec2<float> pitch = sensor_->pixel_pitch();
    const double fnumber = static_cast<double>(focal_length_) / aperture_diameter().to_si();
    auto tables = std::make_shared<const PsfTables<TSpectral>>(PsfTables<TSpectral>::airy(
        fnumber, static_cast<double>(pitch.x), static_cast<double>(pitch.y), reach, scatter, blur));

    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    std::ostringstream message;
    message << "CameraModel - Built the PSF's tables in " << std::fixed << std::setprecision(2)
            << elapsed.count()
            << " seconds: " << static_cast<double>(tables->memory_bytes()) / (1024.0 * 1024.0)
            << " MiB, near table to " << tables->near_radius() << " pixels at 1/"
            << tables->near_samples() << " pixel, blur " << blur << " pixels";
    HUIRA_LOG_INFO(message.str());
    return tables;
}

/**
 * @brief About the most convolving an image with the whole PSF in a precision would put in each
 * channel of its pixels where it is dark, from rounding (see PsfConvolver::rounding_error()).
 *
 * For the renderer, after precompute_for_render_().
 */
template <IsSpectral TSpectral>
std::array<double, TSpectral::size()>
CameraModel<TSpectral>::psf_convolution_error_(const Image<TSpectral>& image,
                                               FftPrecision precision) const
{
    std::lock_guard<std::mutex> lock(optics_mutex_);
    return body_convolver_.rounding_error(image, precision);
}

/**
 * @brief Convolve an image in place with the whole PSF, across the frame (see PsfConvolver and
 * body_parts_()), in a precision, with plans made with an effort.
 *
 * For the renderer, after precompute_for_render_().
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::apply_psf_convolution_(Image<TSpectral>& image,
                                                    FftPrecision precision,
                                                    FftPlanEffort effort) const
{
    detail::convolve_finite_pixels(image, "PSF", [&](Image<TSpectral>& finite) {
        // The convolver's working buffers are shared:
        std::lock_guard<std::mutex> lock(optics_mutex_);
        tbb::this_task_arena::isolate([&] { body_convolver_.apply(finite, precision, effort); });
    });
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
 * are blurred by the defocus at their depth: stars as defocus_blur_radius() says, and objects
 * nearer by their own (see psf_image()). Off, everything is in focus, as through a pinhole,
 * whatever the focus setting.
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
 * @brief Turn motion blur on (the default) or off.
 *
 * On, a view through the camera (a SceneView) shows what moves during the exposure blurred
 * along its path: it gives each thing as many temporal samples as its motion needs, or the
 * number it is given. Off, it renders a still frame, at the middle of the exposure, as one
 * temporal sample does, whatever number it is given.
 *
 * @param motion_blur True to turn motion blur on, false to turn it off.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::enable_motion_blur(bool motion_blur)
{
    motion_blur_ = motion_blur;
}

/**
 * @brief Get the radius, in pixel widths, of the defocus blur of a point at a range, or of a
 * star without one.
 *
 * Depth of field sends light from a point through aperture point a to the sensor position for
 * direction a / d off the point's own, for focus distance d. Over an aperture of bounding radius
 * R that is a disc of radius R f |v|, for v the difference in vergence between the point and the
 * focus: exactly what the ray tracer does, since focus does not change the projection, and to
 * first order what a physical lens does (see set_focus_sensor_offset()).
 *
 * This is the blur unresolved sources are drawn with (see defocus_image() and psf_image()): 0
 * below PsfTables::MIN_BLUR, where it changes no pixel by more than a part in 10^5 of the peak,
 * and with depth of field off (see enable_depth_of_field()). A PSF that was set and already
 * holds the defocus has none added to it (see PSFFocus).
 *
 * @param range The point's distance along the camera's axis; none for a star, at infinity.
 * @throws std::runtime_error if the range is not positive.
 */
template <IsSpectral TSpectral>
float CameraModel<TSpectral>::defocus_blur_radius(std::optional<units::Meter> range) const
{
    if (range.has_value() && !(range->to_si() > 0.0)) {
        HUIRA_THROW_ERROR("CameraModel::defocus_blur_radius - The range must be positive: " +
                          std::to_string(range->to_si()) + " m");
    }
    const double blur = blur_pixels_(range.has_value() ? 1.0 / range->to_si() : 0.0);
    return blur >= PsfTables<TSpectral>::MIN_BLUR ? static_cast<float>(blur) : 0.f;
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
 * @brief As cast_ray(const Pixel&), but empty, rather than throwing, where the lens distortion
 * has no inverse at the position (see set_distortion()).
 *
 * @param pixel Position in the camera's pixel convention (see set_pixel_convention()).
 */
template <IsSpectral TSpectral>
std::optional<Ray<TSpectral>> CameraModel<TSpectral>::try_cast_ray(const Pixel& pixel) const
{
    const Pixel sensor_position = pixel_convention_.to_sensor(pixel, sensor_->resolution());
    const Ray<TSpectral> ray = sensor_ray_(sensor_position, false);
    if (!std::isfinite(ray.direction().x) || !std::isfinite(ray.direction().y) ||
        !std::isfinite(ray.direction().z)) {
        return std::nullopt;
    }
    return ray;
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
 * @brief The aperture's area projected along a direction in camera coordinates, in square
 * meters: its area times the cosine of the direction's angle from the optical axis.
 * @param direction Direction vector (need not be normalized)
 */
template <IsSpectral TSpectral>
float CameraModel<TSpectral>::projected_aperture_area(const Vec3<float>& direction) const
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
 * @brief Resize the default aperture, which keeps the camera at f/2.8, for a focal length about to
 * be stored. A focal length whose aperture can't be held (its area overflows or underflows single
 * precision) is then refused before anything changes, since set_aperture_area_() changes nothing
 * when it throws. The setters call this last of all that can throw.
 *
 * @param focal_length The new focal length, in meters.
 */
template <IsSpectral TSpectral>
void CameraModel<TSpectral>::resize_default_aperture_(float focal_length)
{
    if (aperture_source_ == ApertureSource::Default) {
        set_aperture_area_(units::Meter(focal_length / DEFAULT_FSTOP_));
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
/**
 * @brief A summary of the camera's settings, one per line, for reading: its optics, sensor,
 * focus, PSF, motion blur and conventions. Builds nothing. The format is for people, and may
 * change.
 */
template <IsSpectral TSpectral>
std::string CameraModel<TSpectral>::describe() const
{
    auto num = [](double value, int digits = 4) {
        std::ostringstream out;
        out << std::setprecision(digits) << value;
        return out.str();
    };
    auto on_off = [](bool on) { return on ? std::string("on") : std::string("off"); };
    constexpr double DEG_PER_RAD = 180.0 / PI<double>();
    std::ostringstream out;

    const Bin first = TSpectral::get_bin(0);
    const Bin last = TSpectral::get_bin(TSpectral::size() - 1);
    out << "CameraModel, " << TSpectral::size() << " spectral bins from "
        << num(std::min(first.min_wavelength, last.min_wavelength) * 1e9) << " to "
        << num(std::max(first.max_wavelength, last.max_wavelength) * 1e9) << " nm\n";

    out << "  Optics:      focal length " << num(focal_length().to_si() * 1e3) << " mm, f/"
        << num(fstop(), 3) << " (aperture " << num(aperture_diameter().to_si() * 1e3) << " mm)\n";

    const Resolution pixels = sensor_->resolution();
    const auto [pitch_x, pitch_y] = pixel_pitch();
    out << "  Sensor:      " << pixels.width << " x " << pixels.height << " px of "
        << num(pitch_x.to_si() * 1e6);
    if (pitch_x.to_si() != pitch_y.to_si()) {
        out << " x " << num(pitch_y.to_si() * 1e6);
    }
    out << " um (" << num(pixels.width * pitch_x.to_si() * 1e3) << " x "
        << num(pixels.height * pitch_y.to_si() * 1e3) << " mm), roll "
        << num(sensor_->roll().to_si() * DEG_PER_RAD) << " deg\n";

    // Column-major: k[column][row].
    const Mat3<float> k = intrinsic_matrix();
    out << "  Intrinsics:  fx " << num(k[0][0], 6) << ", fy " << num(k[1][1], 6) << ", cx "
        << num(k[2][0], 6) << ", cy " << num(k[2][1], 6);
    if (k[1][0] != 0.f) {
        out << ", skew " << num(k[1][0], 6);
    }
    out << " px, in " << pixel_convention_.to_string() << "\n";

    // Without distortion, from the principal point in sensor coordinates:
    const double fov_x = (std::atan(static_cast<double>(cx_) / fx_) +
                          std::atan((static_cast<double>(rx_) - cx_) / fx_)) *
                         DEG_PER_RAD;
    const double fov_y = (std::atan(static_cast<double>(cy_) / fy_) +
                          std::atan((static_cast<double>(ry_) - cy_) / fy_)) *
                         DEG_PER_RAD;
    out << "  Field:       " << num(fov_x) << " x " << num(fov_y)
        << " deg (pinhole, without distortion)\n";

    out << "  Axes:        "
        << (blender_convention_ ? "Blender (-z forward, y up)" : "OpenCV (z forward, y down)")
        << "\n";

    out << "  Distortion:  ";
    if (!distortion_) {
        out << "none\n";
    } else if (const auto* brown =
                   dynamic_cast<const BrownCoefficients*>(distortion_->get_coefficients())) {
        out << "Brown-Conrady, k1 " << num(brown->k1) << ", k2 " << num(brown->k2) << ", k3 "
            << num(brown->k3) << ", p1 " << num(brown->p1) << ", p2 " << num(brown->p2) << "\n";
    } else if (const auto* cv =
                   dynamic_cast<const OpenCVCoefficients*>(distortion_->get_coefficients())) {
        out << "OpenCV, k1-k6 " << num(cv->k1) << ", " << num(cv->k2) << ", " << num(cv->k3) << ", "
            << num(cv->k4) << ", " << num(cv->k5) << ", " << num(cv->k6) << "; p1, p2 "
            << num(cv->p1) << ", " << num(cv->p2) << "; s1-s4 " << num(cv->s1) << ", "
            << num(cv->s2) << ", " << num(cv->s3) << ", " << num(cv->s4) << "\n";
    } else if (const auto* owen =
                   dynamic_cast<const OwenCoefficients*>(distortion_->get_coefficients())) {
        out << "Owen, e1-e6 " << num(owen->e1) << ", " << num(owen->e2) << ", " << num(owen->e3)
            << ", " << num(owen->e4) << ", " << num(owen->e5) << ", " << num(owen->e6) << "\n";
    } else {
        out << "custom\n";
    }

    out << "  Focus:       ";
    const double focus = focus_distance().to_si();
    if (std::isinf(focus) && focus > 0) {
        out << "infinity";
    } else if (focus < 0) {
        out << "past infinity (" << num(focus_diopters().to_si()) << " diopters)";
    } else {
        out << num(focus) << " m";
    }
    out << ", depth of field " << on_off(depth_of_field_);
    const float blur_radius = depth_of_field_ ? defocus_blur_radius() : 0.f;
    if (blur_radius > 0.f) {
        out << ", unresolved sources blurred over " << num(blur_radius) << " px";
    }
    out << "\n";

    out << "  PSF:         ";
    const double pixels_per_radian =
        static_cast<double>(focal_length_) / static_cast<double>(sensor_->pixel_pitch().x);
    if (psf_ != nullptr) {
        const auto* measured = dynamic_cast<const MeasuredPSF<TSpectral>*>(psf_.get());
        if (measured != nullptr) {
            out << "measured, " << num(measured->samples_per_pixel()) << " samples per pixel, "
                << (measured->is_pixel_integrated() ? "pixel-integrated" : "point-sampled");
        } else {
            out << "custom";
        }
        out << ", to " << psf_->radius() << " px, "
            << (psf_->includes_defocus() ? "holding the defocus" : "of a point in focus")
            << " (in place of the aperture's diffraction)";
    } else if (diffraction_enabled_) {
        // The first dark ring of the Airy pattern is at 1.22 lambda N, here at the middle bin's
        // central wavelength.
        const double wavelength = TSpectral::get_bin(TSpectral::size() / 2).center_wavelength;
        const double fnumber = static_cast<double>(focal_length_) / aperture_diameter().to_si();
        out << "the aperture's diffraction, first dark ring at "
            << num(static_cast<float>(1.22 * wavelength * fnumber /
                                      static_cast<double>(sensor_->pixel_pitch().x)))
            << " px";
    } else {
        out << "none: diffraction off, a point in focus lights one pixel";
    }
    out << "\n";
    out << "  Scatter:     ";
    if (scatter_enabled_) {
        constexpr double ARCSEC_PER_RADIAN = 648000.0 / PI<double>();
        out << "Harvey-Shack, fraction " << num(scatter_fraction_) << ", slope "
            << num(scatter_slope_) << ", shoulder "
            << num(static_cast<float>(scatter_shoulder_angle_ * ARCSEC_PER_RADIAN)) << " arcsec ("
            << num(static_cast<float>(scatter_shoulder_angle_ * pixels_per_radian)) << " px)";
        if (scatter_outer_angle_ > 0.0) {
            out << ", steeper beyond "
                << num(static_cast<float>(scatter_outer_angle_ * ARCSEC_PER_RADIAN)) << " arcsec ("
                << num(static_cast<float>(scatter_outer_angle_ * pixels_per_radian)) << " px)";
        }
    } else {
        out << "none";
    }
    out << "; veiling glare ";
    out << (veiling_glare_enabled_ ? num(veiling_alpha_) : std::string("none")) << "\n";
    out << "  Resolved bodies blurred by the PSF and scatter: " << on_off(convolve_psf_) << "\n";
    {
        std::lock_guard<std::mutex> lock(optics_mutex_);
        if (render_tables_ != nullptr) {
            out << "  Tables:      "
                << num(static_cast<float>(static_cast<double>(render_tables_->memory_bytes()) /
                                          (1024.0 * 1024.0)))
                << " MiB for stars, as last built\n";
        }
    }

    const SensorModel<TSpectral>& sensor = *sensor_;
    double qe = 0.0;
    for (std::size_t i = 0; i < TSpectral::size(); ++i) {
        qe += static_cast<double>(sensor.quantum_efficiency()[i]);
    }
    qe /= static_cast<double>(TSpectral::size());
    out << "  Readout:     " << sensor.bit_depth() << "-bit, gain " << num(sensor.conversion_gain())
        << " e-/DN, full well " << num(sensor.full_well_capacity(), 6) << " e-, bias "
        << num(sensor.bias_level_dn()) << " DN, mean QE " << num(qe, 3) << "\n";
    out << "  Noise:       " << on_off(sensor.noise_enabled()) << ", read noise "
        << num(sensor.read_noise()) << " e-, dark current " << num(sensor.dark_current())
        << " e-/s, seed " << sensor.noise_seed() << "\n";
    out << "  Motion blur: " << on_off(motion_blur_) << "\n";

    out << "  Precomputed: " << (is_precomputed() ? "yes" : "no")
        << (auto_precompute_ ? " (a render builds what is out of date)"
                             : " (auto precompute off: a render throws if out of date)");
    return out.str();
}

} // namespace huira
