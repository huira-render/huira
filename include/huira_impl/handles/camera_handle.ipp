#include "huira/concepts/spectral_concepts.hpp"
#include "huira/units/units.hpp"

namespace huira {
/**
 * @brief Set the focal length of the camera (in millimeters).
 * @param focal_length Focal length in millimeters
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_focal_length(units::Millimeter focal_length) const
{
    this->get_()->set_focal_length(focal_length);
}

/**
 * @brief Get the focal length of the camera (in millimeters).
 * @return units::Millimeter Focal length in millimeters
 */
template <IsSpectral TSpectral>
units::Millimeter CameraModelHandle<TSpectral>::focal_length() const
{
    return this->get_()->focal_length();
}

/**
 * @brief Set the f-stop (focal ratio) of the camera. The aperture diameter this gives is kept if
 * the focal length later changes; see CameraModel::set_fstop().
 * @param fstop F-stop value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_fstop(float fstop) const
{
    this->get_()->set_fstop(fstop);
}

/**
 * @brief Get the f-stop (focal ratio) of the camera.
 * @return float F-stop value
 */
template <IsSpectral TSpectral>
float CameraModelHandle<TSpectral>::fstop() const
{
    return this->get_()->fstop();
}

/**
 * @brief Set the aperture's diameter, which is kept if the focal length changes. See
 * CameraModel::set_aperture_diameter().
 * @param diameter The aperture's diameter.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_aperture_diameter(units::Millimeter diameter) const
{
    this->get_()->set_aperture_diameter(diameter);
}

/// Get the aperture's diameter.
template <IsSpectral TSpectral>
units::Millimeter CameraModelHandle<TSpectral>::aperture_diameter() const
{
    return this->get_()->aperture_diameter();
}

/**
 * @brief Set the distortion model for the camera.
 * @tparam TDistortion Distortion model type
 * @tparam Args Constructor arguments for the distortion model
 * @param args Arguments to construct the distortion model
 */
template <IsSpectral TSpectral>
template <IsDistortion<TSpectral> TDistortion, typename... Args>
void CameraModelHandle<TSpectral>::set_distortion(Args&&... args) const
{
    this->get_()->template set_distortion<TDistortion>(std::forward<Args>(args)...);
}

/**
 * @brief Set Brown-Conrady distortion coefficients.
 * @param coeffs Brown distortion coefficients
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_brown_conrady_distortion(BrownCoefficients coeffs) const
{
    this->get_()->set_brown_conrady_distortion(coeffs);
}

/**
 * @brief Set OpenCV distortion coefficients.
 * @param coeffs OpenCV distortion coefficients
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_opencv_distortion(OpenCVCoefficients coeffs) const
{
    this->get_()->set_opencv_distortion(coeffs);
}

/**
 * @brief Set Owen distortion coefficients.
 * @param coeffs Owen distortion coefficients
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_owen_distortion(OwenCoefficients coeffs) const
{
    this->get_()->set_owen_distortion(coeffs);
}

/**
 * @brief Delete the distortion model from the camera.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::delete_distortion() const
{
    this->get_()->delete_distortion();
}

/**
 * @brief Set the sensor model for the camera.
 * @tparam TSensor Sensor model type
 * @tparam Args Constructor arguments for the sensor
 * @param args Arguments to construct the sensor
 */
template <IsSpectral TSpectral>
template <IsSensor<TSpectral> TSensor, typename... Args>
void CameraModelHandle<TSpectral>::set_sensor(Args&&... args) const
{
    this->get_()->template set_sensor<TSensor>(std::forward<Args>(args)...);
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
void CameraModelHandle<TSpectral>::configure_sensor_from_pitch(
    const Resolution& resolution,
    units::Micrometer pitch_x,
    std::optional<units::Micrometer> pitch_y,
    std::optional<float> cx,
    std::optional<float> cy)
{
    this->get_()->configure_sensor_from_pitch(resolution, pitch_x, pitch_y, cx, cy);
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
void CameraModelHandle<TSpectral>::configure_sensor_from_size(
    const Resolution& resolution,
    units::Millimeter width,
    std::optional<units::Millimeter> height,
    std::optional<float> cx,
    std::optional<float> cy)
{
    this->get_()->configure_sensor_from_size(resolution, width, height, cx, cy);
}

/**
 * @brief Set the intrinsic matrix for the camera. Mat3 is indexed K[column][row], so cx is
 * K[2][0]; see CameraModel::set_intrinsic_matrix().
 * @param intrinsic_matrix 3x3 intrinsic matrix
 * @param resolution Sensor resolution
 * @param anchor_focal_length Anchor focal length in millimeters
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_intrinsic_matrix(const Mat3<float>& intrinsic_matrix,
                                                        const Resolution& resolution,
                                                        units::Millimeter anchor_focal_length)
{
    this->get_()->set_intrinsic_matrix(intrinsic_matrix, resolution, anchor_focal_length);
}

/**
 * @brief Set the intrinsic parameters for the camera. See CameraModel::set_intrinsics().
 * @param fx Focal length in x direction, in pixels
 * @param fy Focal length in y direction, in pixels
 * @param cx Principal point x coordinate, in the camera's pixel convention
 * @param cy Principal point y coordinate, in the camera's pixel convention
 * @param resolution Sensor resolution
 * @param anchor_focal_length Anchor focal length in millimeters
 * @param skew Skew, in pixels; 0 for perpendicular pixel axes.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_intrinsics(float fx,
                                                  float fy,
                                                  float cx,
                                                  float cy,
                                                  const Resolution& resolution,
                                                  units::Millimeter anchor_focal_length,
                                                  float skew)
{
    this->get_()->set_intrinsics(fx, fy, cx, cy, resolution, anchor_focal_length, skew);
}

/**
 * @brief Set the sensor quantum efficiency.
 * @param qe Quantum efficiency value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_quantum_efficiency(double qe) const
{
    this->set_sensor_quantum_efficiency(TSpectral(static_cast<float>(qe)));
}

/**
 * @brief Set the sensor quantum efficiency.
 * @param qe Quantum efficiency value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_quantum_efficiency(TSpectral qe) const
{
    this->get_()->sensor_->set_quantum_efficiency(qe);
}

/**
 * @brief Set the sensor full well capacity.
 * @param fwc Full well capacity value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_full_well_capacity(float fwc) const
{
    this->get_()->sensor_->set_full_well_capacity(fwc);
}

/**
 * @brief Enable or disable sensor noise simulation.
 * @param simulate_noise True to enable noise simulation, false to disable
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_simulate_noise(bool simulate_noise) const
{
    this->get_()->sensor_->set_simulate_noise(simulate_noise);
}

/**
 * @brief Set the sensor read noise.
 * @param read_noise Read noise value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_read_noise(float read_noise) const
{
    this->get_()->sensor_->set_read_noise(read_noise);
}

/**
 * @brief Set the sensor dark current.
 * @param dark_current Dark current value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_dark_current(float dark_current) const
{
    this->get_()->sensor_->set_dark_current(dark_current);
}

/**
 * @brief Set the sensor bias level.
 * @param bias_level Bias level value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_bias_level(float bias_level) const
{
    this->get_()->sensor_->set_bias_level_dn(bias_level);
}

/**
 * @brief Set the sensor bit depth.
 * @param bit_depth Bit depth value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_bit_depth(int bit_depth) const
{
    this->get_()->sensor_->set_bit_depth(bit_depth);
}

/**
 * @brief Set the sensor conversion gain (e-/ADU).
 *
 * Sets the conversion gain of the sensor, which defines how many electrons correspond to one ADU
 * (Analog-to-Digital Unit). This affects the sensor's sensitivity and noise characteristics.  This
 * value is frequently found on sensor datasheets.  A larger value will make the resulting image
 * darker.
 *
 * @param gain Gain value
 * @throws std::runtime_error if gain is not positive or finite.
 * @see set_sensor_gain_db, set_sensor_unity_db
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_conversion_gain(float gain) const
{
    this->get_()->sensor_->set_conversion_gain(gain);
}

/**
 * @brief Set the sensor gain in decibels (dB).
 *
 * Sets the gain of the sensor in decibels, a logarithmic representation of the
 * sensor's amplification. A larger value produces a brighter image. This is the
 * convention used by most machine-vision cameras.
 *
 * The dB value maps onto the conversion gain (e-/ADU) as:
 *     conversion_gain = 10^((unity_db - gain_db) / 20)
 * so at gain_db == unity_db the conversion gain is exactly 1 e-/ADU.
 *
 * @param gain_db Gain in dB
 * @throws std::runtime_error if gain_db is not finite.
 * @see set_sensor_unity_db, set_sensor_conversion_gain
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_gain_db(float gain_db) const
{
    this->get_()->sensor_->set_gain_db(gain_db);
}

/**
 * @brief Set the reference level (in dB) for the sensor's gain-in-dB scale.
 *
 * unity_db defines where the dB scale is anchored: it is the gain_db value at
 * which the conversion gain equals exactly 1 e-/ADU (i.e. one electron maps to
 * one ADU, before bias). Changing unity_db shifts the entire dB scale without
 * altering the underlying conversion gain.
 *
 * This is useful for matching a real camera's datasheet, where "0 dB" is
 * typically defined relative to the camera's own baseline analog gain rather
 * than to 1 e-/ADU. For example, if a camera reports a conversion gain of
 * 3.16 e-/ADU at its 0 dB setting, set unity_db to 10 (since
 * 20*log10(3.16) ≈ 10) and set_sensor_gain_db(0) will then reproduce that
 * camera's 0 dB behavior.
 *
 * Defaults to 0, meaning gain_db = 0 corresponds to 1 e-/ADU.
 *
 * @param unity_db Reference level in dB
 * @throws std::runtime_error if unity_db is not finite.
 * @see set_sensor_gain_db
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_unity_db(float unity_db) const
{
    this->get_()->sensor_->set_unity_db(unity_db);
}

/**
 * @brief Set the sensor rotation angle.
 *
 * Rotates the sensor around the optical axis by the specified angle.
 *
 * @param angle Rotation angle
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_rotation(units::Radian angle) const
{
    this->get_()->sensor_->set_rotation(angle);
}

/**
 * @brief Seed the sensor's noise, so that its frames can be reproduced.
 *
 * Setting the same seed again repeats the same sequence of frames. Sensors get distinct seeds by
 * default. See SensorModel::set_noise_seed().
 *
 * @param seed Any value.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_noise_seed(std::uint64_t seed) const
{
    this->get_()->sensor_->set_noise_seed(seed);
}

/// Get the seed of the sensor's noise. See set_sensor_noise_seed().
template <IsSpectral TSpectral>
std::uint64_t CameraModelHandle<TSpectral>::sensor_noise_seed() const
{
    return this->get_()->sensor_->noise_seed();
}

/**
 * @brief Set the aperture model for the camera.
 * @tparam TAperture Aperture model type
 * @tparam Args Constructor arguments for the aperture
 * @param args Arguments to construct the aperture
 */
template <IsSpectral TSpectral>
template <IsAperture TAperture, typename... Args>
void CameraModelHandle<TSpectral>::set_aperture(Args&&... args) const
{
    this->get_()->template set_aperture<TAperture>(std::forward<Args>(args)...);
}

/**
 * @brief Set the point spread function (PSF) model for the camera.
 * @tparam TPSF PSF model type
 * @tparam Args Constructor arguments for the PSF
 * @param args Arguments to construct the PSF
 */
template <IsSpectral TSpectral>
template <IsPSF TPSF, typename... Args>
void CameraModelHandle<TSpectral>::set_psf(Args&&... args) const
{
    this->get_()->template set_psf<TPSF>(std::forward<Args>(args)...);
}

/**
 * @brief Use the aperture to generate a PSF (point spread function).
 * @param value True to enable aperture PSF, false to disable
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::use_aperture_psf(bool value) const
{
    if (value) {
        this->get_()->use_aperture_psf();
    } else {
        this->get_()->delete_psf();
    }
}

/**
 * @brief Use the aperture to generate a PSF (point spread function).
 * @param radius PSF kernel radius
 * @param banks Number of PSF banks
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::use_aperture_psf(int radius, int banks) const
{
    this->get_()->use_aperture_psf(radius, banks);
}

/**
 * @brief Convolve the specified PSF with rendered extended images.
 * @param convolve_psf True to enable convolving the PSF with rendered images.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::enable_psf_convolution(bool convolve_psf) const
{
    this->get_()->enable_psf_convolution(convolve_psf);
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_psf_convolution_radius(int radius) const
{
    this->get_()->set_psf_convolution_radius(radius);
}

/**
 * @brief Delete the PSF and disable aperture PSF usage.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::delete_psf() const
{
    this->get_()->delete_psf();
}

/**
 * @brief Sets a measured (user-supplied) PSF as the core PSF of the camera.
 *
 * @param data Measured PSF samples, centered on the image.
 * @param samples_per_pixel Measurement samples per sensor pixel per axis.
 * @param radius Polyphase stamping kernel radius in sensor pixels (0 = auto).
 * @param banks Number of polyphase banks per axis for subpixel stamping.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_measured_psf(const Image<TSpectral>& data,
                                                    float samples_per_pixel,
                                                    int radius,
                                                    int banks) const
{
    this->get_()->set_measured_psf(data, samples_per_pixel, radius, banks);
}

/**
 * @brief Set the veiling glare alpha value.
 * @param alpha Veiling glare alpha (0 to 1)
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_veiling_glare(float alpha) const
{
    this->get_()->set_veiling_glare(alpha);
}

/**
 * @brief Disable veiling glare effects.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::disable_veiling_glare() const
{
    this->get_()->disable_veiling_glare();
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
void CameraModelHandle<TSpectral>::set_harvey_shack_scatter(float scatter_fraction,
                                                            float falloff_exponent,
                                                            float r0,
                                                            float radius) const
{
    this->get_()->set_harvey_shack_scatter(scatter_fraction, falloff_exponent, r0, radius);
}

/**
 * @brief Disable Harvey-Shack scatter effects.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::disable_harvey_shack_scatter() const
{
    this->get_()->disable_harvey_shack_scatter();
}

template <IsSpectral TSpectral>
int CameraModelHandle<TSpectral>::get_psf_radius() const
{
    return this->get_()->get_psf_radius();
}

template <IsSpectral TSpectral>
const Image<TSpectral>& CameraModelHandle<TSpectral>::get_psf_convolution_kernel() const
{
    return this->get_()->get_psf_convolution_kernel();
}

/**
 * @brief Build everything the camera's optics need for the next render, now, so that the time
 * is not spent in the render. See CameraModel::precompute().
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::precompute() const
{
    this->get_()->precompute();
}

/**
 * @brief Whether everything the next render needs from the camera's optics is built and up to
 * date. See CameraModel::is_precomputed().
 */
template <IsSpectral TSpectral>
bool CameraModelHandle<TSpectral>::is_precomputed() const
{
    return this->get_()->is_precomputed();
}

/**
 * @brief Choose whether a render builds out-of-date optics kernels itself (the default) or
 * throws. See CameraModel::set_auto_precompute().
 * @param auto_precompute True for the render to build them, false to throw.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_auto_precompute(bool auto_precompute) const
{
    this->get_()->set_auto_precompute(auto_precompute);
}

/// Whether a render builds out-of-date optics kernels itself. See set_auto_precompute().
template <IsSpectral TSpectral>
bool CameraModelHandle<TSpectral>::auto_precompute() const
{
    return this->get_()->auto_precompute();
}

/**
 * @brief Enable or disable depth of field effects.
 * @param depth_of_field True to enable depth of field, false to disable
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::enable_depth_of_field(bool depth_of_field) const
{
    this->get_()->enable_depth_of_field(depth_of_field);
}

/**
 * @brief Focus the camera at a distance.
 *
 * Positive values focus in front of the camera, infinity focuses at infinity, and negative
 * values focus past infinity. See CameraModel::set_focus_distance().
 *
 * @param focus_distance Focus distance (any length unit).
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_focus_distance(units::Meter focus_distance) const
{
    this->get_()->set_focus_distance(focus_distance);
}

/**
 * @brief Focus the camera in diopters: the reciprocal of the focus distance.
 *
 * 0 focuses at infinity and negative values focus past infinity. See
 * CameraModel::set_focus_diopters().
 *
 * @param diopters The reciprocal of the focus distance (diopters).
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_focus_diopters(units::Diopter diopters) const
{
    this->get_()->set_focus_diopters(diopters);
}

/**
 * @brief Focus the camera by the sensor's offset from the infinity-focus position.
 *
 * 0 focuses at infinity, positive values (sensor farther from the lens) focus in front of the
 * camera, and negative values focus past infinity. See CameraModel::set_focus_sensor_offset().
 *
 * @param offset Sensor offset (any length unit).
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_focus_sensor_offset(units::Micrometer offset) const
{
    this->get_()->set_focus_sensor_offset(offset);
}

/// Get the distance the camera is focused at: negative past infinity, +inf at infinity.
template <IsSpectral TSpectral>
units::Meter CameraModelHandle<TSpectral>::focus_distance() const
{
    return this->get_()->focus_distance();
}

/// Get the focus in diopters: the reciprocal of focus_distance(), 0 at infinity.
template <IsSpectral TSpectral>
units::Diopter CameraModelHandle<TSpectral>::focus_diopters() const
{
    return this->get_()->focus_diopters();
}

/// Get the focus as the sensor's offset from the infinity-focus position.
template <IsSpectral TSpectral>
units::Micrometer CameraModelHandle<TSpectral>::focus_sensor_offset() const
{
    return this->get_()->focus_sensor_offset();
}

/// Get the radius, in pixels, of the defocus blur applied to unresolved sources (0 if in focus).
template <IsSpectral TSpectral>
float CameraModelHandle<TSpectral>::defocus_blur_radius() const
{
    return this->get_()->defocus_blur_radius();
}

/**
 * @brief Set the pixel coordinate convention used for the principal point and projected
 * positions. The default is PixelConvention::opencv(). See CameraModel::set_pixel_convention().
 * @param convention The convention, e.g. PixelConvention::fits().
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_pixel_convention(PixelConvention convention) const
{
    this->get_()->set_pixel_convention(convention);
}

/// Get the pixel coordinate convention used for the principal point and projected positions.
template <IsSpectral TSpectral>
PixelConvention CameraModelHandle<TSpectral>::pixel_convention() const
{
    return this->get_()->pixel_convention();
}

/**
 * @brief Project a 3D point in camera coordinates onto the image plane.
 * @param point_camera_coords 3D point in camera coordinates (meters)
 * @return Pixel 2D point on the image plane, in the camera's pixel convention
 */
template <IsSpectral TSpectral>
Pixel CameraModelHandle<TSpectral>::project_point(const Vec3<float>& point_camera_coords) const
{
    return this->get_()->project_point(point_camera_coords);
}

/**
 * @brief Create a new frame buffer with the camera's resolution.
 * @return FrameBuffer<TSpectral> Frame buffer
 */
template <IsSpectral TSpectral>
FrameBuffer<TSpectral> CameraModelHandle<TSpectral>::make_frame_buffer() const
{
    return this->get_()->make_frame_buffer();
}

/**
 * @brief Set whether to use Blender's camera convention (z forward, y up).
 * @param value True to use Blender convention
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::use_blender_convention(bool value) const
{
    this->get_()->use_blender_convention(value);
}
} // namespace huira
