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
 * @brief Set the f-stop (aperture ratio) of the camera.
 * @param fstop F-stop value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_fstop(float fstop) const
{
    this->get_()->set_fstop(fstop);
}

/**
 * @brief Get the f-stop (aperture ratio) of the camera.
 * @return float F-stop value
 */
template <IsSpectral TSpectral>
float CameraModelHandle<TSpectral>::fstop() const
{
    return this->get_()->fstop();
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
 * @param cx Principal point x coordinate (must be within resolution bounds)
 * @param cy Principal point y coordinate (must be within resolution bounds)
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
 * @param cx Principal point x coordinate (must be within resolution bounds)
 * @param cy Principal point y coordinate (must be within resolution bounds)
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
 * @brief Set the intrinsic matrix for the camera.
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
 * @brief Set the intrinsic parameters for the camera.
 * @param fx Focal length in x direction
 * @param fy Focal length in y direction
 * @param cx Principal point x coordinate
 * @param cy Principal point y coordinate
 * @param resolution Sensor resolution
 * @param anchor_focal_length Anchor focal length in millimeters
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_intrinsics(float fx,
                                                  float fy,
                                                  float cx,
                                                  float cy,
                                                  const Resolution& resolution,
                                                  units::Millimeter anchor_focal_length)
{
    this->get_()->set_intrinsics(fx, fy, cx, cy, resolution, anchor_focal_length);
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
 * @brief Set the sensor gain (ADU).
 * @param gain Gain value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_gain(float gain) const
{
    this->get_()->sensor_->set_gain_adu(gain);
}

/**
 * @brief Set the sensor gain in decibels (dB).
 * @param gain_db Gain in dB
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_gain_db(float gain_db) const
{
    this->get_()->sensor_->set_gain_db(gain_db);
}

/**
 * @brief Set the sensor unity gain in decibels (dB).
 * @param unity_db Unity gain in dB
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_uinty_db(float unity_db) const
{
    this->get_()->sensor_->set_unity_db(unity_db);
}

/**
 * @brief Set the sensor rotation angle.
 * @param angle Rotation angle in radians
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_rotation(units::Radian angle) const
{
    this->get_()->sensor_->set_rotation(angle);
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

/**
 * @brief Delete the PSF and disable aperture PSF usage.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::delete_psf() const
{
    this->get_()->delete_psf();
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
 * @param scatter_fraction Fraction of light scattered (0 to 1)
 * @param falloff_exponent Exponent for scatter falloff (typically > 1)
 * @param r0 Radius at which scatter fraction is measured (default 0.5)
 * @param radius Maximum scatter radius in pixels (default 0, meaning infinite)
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
 * @brief Set the focus distance for depth of field calculations.
 * @param focus_distance Focus distance in meters
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_focus_distance(units::Meter focus_distance) const
{
    this->get_()->set_focus_distance(focus_distance);
}

/**
 * @brief Get the current focus distance for depth of field calculations.
 * @return units::Meter Focus distance in meters
 */
template <IsSpectral TSpectral>
units::Meter CameraModelHandle<TSpectral>::get_focus_distance() const
{
    return this->get_()->get_focus_distance();
}

/**
 * @brief Set the diopters for depth of field calculations.
 * @param diopters Diopters value
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_diopters(units::Diopter diopters) const
{
    this->get_()->set_diopters(diopters);
}

/**
 * @brief Get the current diopters value for depth of field calculations.
 * @return units::Diopter Diopters value
 */
template <IsSpectral TSpectral>
units::Diopter CameraModelHandle<TSpectral>::get_diopters() const
{
    return this->get_()->get_diopters();
}

/**
 * @brief Project a 3D point in camera coordinates onto the image plane.
 * @param point_camera_coords 3D point in camera coordinates (meters)
 * @return Pixel 2D point on the image plane (pixels)
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

// ================== //
// === DEPRECATED === //
// ================== //
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_resolution(Resolution resolution) const
{
    (void)resolution;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_sensor_resolution was removed in v0.9.4. Use "
                      "configure_sensor_from_pitch() or configure_sensor_from_size() instead.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_resolution(int width, int height) const
{
    (void)width;
    (void)height;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_sensor_resolution was removed in v0.9.4. Use "
                      "configure_sensor_from_pitch() or configure_sensor_from_size() instead.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_pixel_pitch(units::Millimeter pitch_x,
                                                          units::Millimeter pitch_y) const
{
    (void)pitch_x;
    (void)pitch_y;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_sensor_pixel_pitch was removed in v0.9.4. Use "
                      "configure_sensor_from_pitch() instead.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_pixel_pitch(units::Millimeter pitch) const
{
    (void)pitch;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_sensor_pixel_pitch was removed in v0.9.4. Use "
                      "configure_sensor_from_pitch() instead.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_size(units::Millimeter width,
                                                   units::Millimeter height) const
{
    (void)width;
    (void)height;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_sensor_size was removed in v0.9.4. Use "
                      "configure_sensor_from_size() instead.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_sensor_size(units::Millimeter width) const
{
    (void)width;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_sensor_size was removed in v0.9.4. Use "
                      "configure_sensor_from_size() instead.");
}
} // namespace huira
