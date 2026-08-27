#include <utility>

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
 * @brief Set the complete optical description of the camera.
 * @param optics The optical description to apply.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_optics(Optics<TSpectral> optics) const
{
    this->get_()->set_optics(std::move(optics));
}

/**
 * @brief Get the camera's optical description.
 * @return Reference to the current optics.
 */
template <IsSpectral TSpectral>
const Optics<TSpectral>& CameraModelHandle<TSpectral>::optics() const
{
    return this->get_()->optics();
}

/**
 * @brief Replace the PSF core, leaving the stray-light components unchanged.
 * @param core The core to apply.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_core(typename Optics<TSpectral>::Core core) const
{
    this->get_()->set_core(std::move(core));
}

/**
 * @brief Add scattered-light wings to the optics, leaving the core unchanged.
 * @param scatter The scatter parameters to apply.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_scatter(HarveyShack scatter) const
{
    this->get_()->set_scatter(std::move(scatter));
}

/**
 * @brief Set the fraction of energy redistributed uniformly across the frame.
 * @param fraction Veiling glare fraction in [0, 1].
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_veiling_glare(float fraction) const
{
    this->get_()->set_veiling_glare(fraction);
}

/**
 * @brief Remove the scattered-light wings, leaving the core and glare unchanged.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::clear_scatter() const
{
    this->get_()->clear_scatter();
}

/**
 * @brief Render with perfect optics: no PSF core, no scatter, and no glare.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_ideal_optics() const
{
    this->get_()->set_ideal_optics();
}

/**
 * @brief Check whether a PSF core is present.
 * @return True when the optics describe a core other than IdealCore.
 */
template <IsSpectral TSpectral>
bool CameraModelHandle<TSpectral>::has_core_psf() const
{
    this->get_()->build_optics();
    return this->get_()->has_core_psf();
}

/**
 * @brief Check whether the optics include scattered-light wings.
 * @return True when a scatter component is present.
 */
template <IsSpectral TSpectral>
bool CameraModelHandle<TSpectral>::has_scatter() const
{
    return this->get_()->has_scatter();
}

/**
 * @brief Get the fraction of energy diverted into the scattered-light wings.
 * @return The scatter fraction, or zero when no scatter is present.
 */
template <IsSpectral TSpectral>
float CameraModelHandle<TSpectral>::scatter_fraction() const
{
    return this->get_()->scatter_fraction();
}

/**
 * @brief Get the fraction of energy redistributed uniformly across the frame.
 * @return The veiling glare fraction.
 */
template <IsSpectral TSpectral>
float CameraModelHandle<TSpectral>::veiling_glare() const
{
    return this->get_()->veiling_glare();
}

/**
 * @brief Get the PSF stamping radius in pixels.
 * @return The radius in pixels.
 */
template <IsSpectral TSpectral>
int CameraModelHandle<TSpectral>::get_psf_radius() const
{
    this->get_()->build_optics();
    return this->get_()->get_psf_radius();
}

/**
 * @brief Get the total-system PSF kernel used for whole-image convolution.
 * @return Reference to the cached convolution kernel.
 */
template <IsSpectral TSpectral>
const Image<TSpectral>& CameraModelHandle<TSpectral>::psf_convolution_kernel() const
{
    return this->get_()->psf_convolution_kernel();
}

/**
 * @brief Get the scattered-light wings kernel alone.
 * @return Reference to the cached wings kernel.
 */
template <IsSpectral TSpectral>
const Image<TSpectral>& CameraModelHandle<TSpectral>::psf_wings_kernel() const
{
    return this->get_()->psf_wings_kernel();
}

/**
 * @brief Build every optical kernel the camera currently describes.
 *
 * Rendering does this automatically. Call it to move the build cost off the first frame.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::build_optics() const
{
    this->get_()->build_optics();
}

/**
 * @brief Set the focus distance.
 * @param distance Focus distance. Infinite focuses at infinity.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_focus(units::Meter distance) const
{
    this->get_()->set_focus(distance);
}

/**
 * @brief Set the focus as a vergence, the reciprocal of the focus distance.
 *
 * Zero focuses at infinity, positive values focus nearer, and negative values focus beyond
 * infinity.
 *
 * @param vergence Focus vergence in diopters.
 */
template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_focus(units::Diopter vergence) const
{
    this->get_()->set_focus(vergence);
}

/**
 * @brief Get the focus distance.
 * @return The focus distance, infinite when focused at infinity.
 */
template <IsSpectral TSpectral>
units::Meter CameraModelHandle<TSpectral>::focus_distance() const
{
    return this->get_()->focus_distance();
}

/**
 * @brief Get the focus setting as a vergence.
 * @return The focus vergence in diopters.
 */
template <IsSpectral TSpectral>
units::Diopter CameraModelHandle<TSpectral>::focus_vergence() const
{
    return this->get_()->focus_vergence();
}

/**
 * @brief Get the defocus blur radius for a source at infinity.
 * @return The blur radius in pixels, zero when focused at infinity.
 */
template <IsSpectral TSpectral>
float CameraModelHandle<TSpectral>::defocus_blur_pixels() const
{
    return this->get_()->defocus_blur_pixels();
}

/**
 * @brief Check whether the camera is defocused enough to blur sources at infinity.
 * @return True when a defocus kernel is in use.
 */
template <IsSpectral TSpectral>
bool CameraModelHandle<TSpectral>::has_defocus() const
{
    this->get_()->build_optics();
    return this->get_()->has_defocus();
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
void CameraModelHandle<TSpectral>::use_aperture_psf(bool value) const
{
    (void)value;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: use_aperture_psf was removed in v0.10.0. A "
                      "diffraction core is now applied by default; use set_optics() to change "
                      "it, or set_ideal_optics() to remove it.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::use_aperture_psf(int radius, int banks) const
{
    (void)radius;
    (void)banks;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: use_aperture_psf was removed in v0.10.0. A "
                      "diffraction core is now applied by default; use set_core() with a "
                      "DiffractionCore to override its radius and banks.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::enable_psf_convolution(bool convolve_psf) const
{
    (void)convolve_psf;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: enable_psf_convolution was removed in v0.10.0. The "
                      "optics are applied to the whole image by default; use "
                      "Renderer::set_psf_application() to skip it.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_psf_convolution_radius(int radius) const
{
    (void)radius;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_psf_convolution_radius was removed in v0.10.0. "
                      "The radius is derived from HarveyShack::captured_energy; set "
                      "HarveyShack::kernel_radius to override it.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::delete_psf() const
{
    HUIRA_THROW_ERROR("API BREAKING CHANGE: delete_psf was removed in v0.10.0. Use "
                      "set_ideal_optics() instead.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_measured_psf(const Image<TSpectral>& data,
                                                    float samples_per_pixel,
                                                    int radius,
                                                    int banks) const
{
    (void)data;
    (void)samples_per_pixel;
    (void)radius;
    (void)banks;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_measured_psf was removed in v0.10.0. Use "
                      "set_core() with a MeasuredCore instead.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_harvey_shack_scatter(float scatter_fraction,
                                                            float falloff_exponent,
                                                            float r0,
                                                            float radius) const
{
    (void)scatter_fraction;
    (void)falloff_exponent;
    (void)r0;
    (void)radius;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_harvey_shack_scatter was removed in v0.10.0. "
                      "Use set_scatter() with a HarveyShack instead.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::disable_harvey_shack_scatter() const
{
    HUIRA_THROW_ERROR("API BREAKING CHANGE: disable_harvey_shack_scatter was removed in "
                      "v0.10.0. Use clear_scatter() instead.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::disable_veiling_glare() const
{
    HUIRA_THROW_ERROR("API BREAKING CHANGE: disable_veiling_glare was removed in v0.10.0. Use "
                      "set_veiling_glare(0.f) instead.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::enable_depth_of_field(bool depth_of_field) const
{
    (void)depth_of_field;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: enable_depth_of_field was removed in v0.10.0. "
                      "Aperture sampling now follows the focus setting; use "
                      "Renderer::set_aperture_sampling() to override it.");
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_focus_distance(units::Meter focus_distance) const
{
    (void)focus_distance;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_focus_distance was removed in v0.10.0. Use "
                      "set_focus() instead.");
}

template <IsSpectral TSpectral>
units::Meter CameraModelHandle<TSpectral>::get_focus_distance() const
{
    HUIRA_THROW_ERROR("API BREAKING CHANGE: get_focus_distance was removed in v0.10.0. Use "
                      "focus_distance() instead.");
    return units::Meter(0.0);
}

template <IsSpectral TSpectral>
void CameraModelHandle<TSpectral>::set_diopters(units::Diopter diopters) const
{
    (void)diopters;
    HUIRA_THROW_ERROR("API BREAKING CHANGE: set_diopters was removed in v0.10.0. Use "
                      "set_focus() with a Diopter argument instead.");
}

template <IsSpectral TSpectral>
units::Diopter CameraModelHandle<TSpectral>::get_diopters() const
{
    HUIRA_THROW_ERROR("API BREAKING CHANGE: get_diopters was removed in v0.10.0. Use "
                      "focus_vergence() instead.");
    return units::Diopter(0.0);
}

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
