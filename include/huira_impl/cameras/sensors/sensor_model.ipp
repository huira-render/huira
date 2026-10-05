
#include <cmath>
#include <string>

#include "huira/core/types.hpp"
#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Constructs a SensorModel with the given configuration.
 *
 * @param config The sensor configuration parameters.
 * @throws std::runtime_error if any parameter is invalid; see the setters.
 */
template <IsSpectral TSpectral>
SensorModel<TSpectral>::SensorModel(SensorConfig<TSpectral> config)
{
    // Each setter checks its own value:
    set_resolution(config.resolution);
    set_pixel_pitch(config.pitch_x, config.pitch_y);
    set_quantum_efficiency(config.quantum_efficiency);
    set_full_well_capacity(config.full_well_capacity);
    set_simulate_noise(config.simulate_noise);
    set_read_noise(config.read_noise);
    set_dark_current(config.dark_current);
    set_bias_level_dn(config.bias_level_dn);
    set_bit_depth(config.bit_depth);
    set_conversion_gain(config.gain);
    set_unity_db(config.unity_db);
    set_rotation(config.rotation);
}

/**
 * @brief Checks that a resolution has at least one pixel in each direction.
 *
 * @param resolution The resolution to check.
 * @param caller Name for the error message.
 * @throws std::runtime_error if it does not.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::check_resolution(const Resolution& resolution,
                                              const std::string& caller)
{
    if (resolution.x < 1 || resolution.y < 1) {
        HUIRA_THROW_ERROR(caller + " - Resolution must be at least 1x1: " +
                          std::to_string(resolution.x) + "x" + std::to_string(resolution.y));
    }
}

/**
 * @brief Checks that pixel pitches are positive and finite.
 *
 * @param pitch_x The horizontal pixel pitch.
 * @param pitch_y The vertical pixel pitch.
 * @param caller Name for the error message.
 * @throws std::runtime_error if either is not.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::check_pixel_pitch(units::Micrometer pitch_x,
                                               units::Micrometer pitch_y,
                                               const std::string& caller)
{
    // Checked as stored, in single precision, so that a value that rounds to 0 or infinity
    // there is caught too:
    const float x = pitch_x.to_si_f();
    const float y = pitch_y.to_si_f();
    if (!(x > 0.f) || !std::isfinite(x) || !(y > 0.f) || !std::isfinite(y)) {
        HUIRA_THROW_ERROR(caller + " - Pixel pitch must be positive and finite: " +
                          std::to_string(pitch_x.to_si()) + " m x " +
                          std::to_string(pitch_y.to_si()) + " m");
    }
}

/**
 * @brief Checks that sensor dimensions are positive and finite.
 *
 * @param width The sensor width.
 * @param height The sensor height.
 * @param caller Name for the error message.
 * @throws std::runtime_error if either is not.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::check_sensor_size(units::Millimeter width,
                                               units::Millimeter height,
                                               const std::string& caller)
{
    const double w = width.to_si();
    const double h = height.to_si();
    if (!(w > 0.0) || !std::isfinite(w) || !(h > 0.0) || !std::isfinite(h)) {
        HUIRA_THROW_ERROR(caller + " - Sensor size must be positive and finite: " +
                          std::to_string(w) + " m x " + std::to_string(h) + " m");
    }
}

/**
 * @brief Sets the sensor resolution.
 *
 * @param resolution The new sensor resolution (width x height).
 * @throws std::runtime_error if the resolution is not at least 1x1.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_resolution(Resolution resolution)
{
    check_resolution(resolution, "SensorModel::set_resolution");
    config_.resolution = resolution;
}

/**
 * @brief Sets the pixel pitch of the sensor.
 *
 * @param pitch_x The horizontal pixel pitch in micrometers.
 * @param pitch_y The vertical pixel pitch in micrometers.
 * @throws std::runtime_error if either pitch is not positive and finite.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_pixel_pitch(units::Micrometer pitch_x, units::Micrometer pitch_y)
{
    check_pixel_pitch(pitch_x, pitch_y, "SensorModel::set_pixel_pitch");
    config_.pitch_x = pitch_x;
    config_.pitch_y = pitch_y;
}

/**
 * @brief Returns the pixel pitch of the sensor.
 *
 * @return The pixel pitch as a 2D vector (x, y) in meters.
 */
template <IsSpectral TSpectral>
Vec2<float> SensorModel<TSpectral>::pixel_pitch() const
{
    return {config_.pitch_x.to_si(), config_.pitch_y.to_si()};
}

/**
 * @brief Sets the sensor size in millimeters.
 *
 * Computes and sets the pixel pitch based on the given sensor size and current resolution.
 *
 * @param width The sensor width in millimeters.
 * @param height The sensor height in millimeters.
 * @throws std::runtime_error if either dimension is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_sensor_size(units::Millimeter width, units::Millimeter height)
{
    check_sensor_size(width, height, "SensorModel::set_sensor_size");
    units::Meter pitch_x(width.to_si() / static_cast<double>(config_.resolution.x));
    units::Meter pitch_y(height.to_si() / static_cast<double>(config_.resolution.y));
    set_pixel_pitch(pitch_x, pitch_y);
}

/**
 * @brief Returns the sensor size in meters.
 *
 * @return The sensor size as a 2D vector (width, height) in meters.
 */
template <IsSpectral TSpectral>
Vec2<float> SensorModel<TSpectral>::sensor_size() const
{
    Vec2<float> pitch = pixel_pitch();
    return {config_.resolution.x * pitch.x, config_.resolution.y * pitch.y};
}

/**
 * @brief Sets the quantum efficiency of the sensor.
 *
 * @param qe The quantum efficiency spectrum (values between 0 and 1).
 * @throws std::runtime_error if the values are invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_quantum_efficiency(const TSpectral& qe)
{
    if (!qe.valid_ratio()) {
        HUIRA_THROW_ERROR("SensorModel::set_quantum_efficiency - Quantum efficiency values must be "
                          "valid values between 0 and 1.");
    }
    config_.quantum_efficiency = qe;
}

/**
 * @brief Sets the full well capacity of the sensor.
 *
 * @param fwc The full well capacity in electrons.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_full_well_capacity(float fwc)
{
    if (fwc <= 0 || std::isinf(fwc) || std::isnan(fwc)) {
        HUIRA_THROW_ERROR(
            "SensorModel::set_full_well_capacity - Full well capacity must be a positive value: " +
            std::to_string(fwc) + " e-");
    }
    config_.full_well_capacity = fwc;
}

/**
 * @brief Sets the read noise of the sensor.
 *
 * @param read_noise The read noise in electrons RMS.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_read_noise(float read_noise)
{
    if (read_noise < 0 || std::isinf(read_noise) || std::isnan(read_noise)) {
        HUIRA_THROW_ERROR(
            "SensorModel::set_read_noise - Read noise must be a non-negative value: " +
            std::to_string(read_noise) + " e-");
    }
    config_.read_noise = read_noise;
}

/**
 * @brief Sets the dark current of the sensor.
 *
 * @param dark_current The dark current in electrons per second.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_dark_current(float dark_current)
{
    if (dark_current < 0 || std::isinf(dark_current) || std::isnan(dark_current)) {
        HUIRA_THROW_ERROR(
            "SensorModel::set_dark_current - Dark current must be a non-negative value: " +
            std::to_string(dark_current) + " e-/s");
    }
    config_.dark_current = dark_current;
}

/**
 * @brief Sets the bias level of the sensor in ADU.
 *
 * @param bias_level_dn The bias level in ADU.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_bias_level_dn(float bias_level_dn)
{
    if (bias_level_dn < 0 || std::isinf(bias_level_dn) || std::isnan(bias_level_dn)) {
        HUIRA_THROW_ERROR(
            "SensorModel::set_bias_level_dn - Bias level must be a non-negative value: " +
            std::to_string(bias_level_dn) + " ADU");
    }
    config_.bias_level_dn = bias_level_dn;
}

/**
 * @brief Sets the bit depth of the sensor.
 *
 * @param bit_depth The bit depth (number of bits per pixel).
 * @throws std::runtime_error if the value is not between 1 and 64.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_bit_depth(int bit_depth)
{
    if (bit_depth < 1 || bit_depth > MAX_BIT_DEPTH) {
        HUIRA_THROW_ERROR("SensorModel::set_bit_depth - Bit depth must be between 1 and " +
                          std::to_string(MAX_BIT_DEPTH) + ": " + std::to_string(bit_depth) +
                          " bits");
    }
    config_.bit_depth = bit_depth;
}

/**
 * @brief Sets the conversion gain of the sensor in e-/ADU.
 *
 * @param gain The gain in electrons per ADU.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_conversion_gain(float gain)
{
    if (gain <= 0 || std::isinf(gain) || std::isnan(gain)) {
        HUIRA_THROW_ERROR("SensorModel::set_conversion_gain - Gain must be a positive value: " +
                          std::to_string(gain) + " e-/ADU");
    }
    config_.gain = gain;
}

/**
 * @brief Sets the unity dB reference level for gain.
 *
 * @param unity_db The unity dB reference level.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_unity_db(float unity_db)
{
    if (std::isinf(unity_db) || std::isnan(unity_db)) {
        HUIRA_THROW_ERROR(
            "SensorModel::set_unity_db - Unity dB reference level cannot be infinite or NaN: " +
            std::to_string(unity_db) + " dB");
    }
    config_.unity_db = unity_db;
}

/**
 * @brief Sets the gain in dB for the sensor.
 *
 * @param gain_db The gain in decibels.
 * @throws std::runtime_error if the value is not finite, or gives a conversion gain that is not a
 *         positive finite value.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_gain_db(float gain_db)
{
    config_.set_gain_db(gain_db);
}

/**
 * @brief Sets the rotation of the sensor about the optical axis.
 *
 * @param angle The rotation angle.
 * @throws std::runtime_error if the angle is not finite.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_rotation(units::Radian angle)
{
    if (!std::isfinite(angle.to_si())) {
        HUIRA_THROW_ERROR("SensorModel::set_rotation - Rotation must be finite: " +
                          std::to_string(angle.to_si()) + " rad");
    }
    config_.rotation = angle;
}
} // namespace huira
