
#include "huira/core/types.hpp"
#include "huira/util/logger.hpp"

namespace huira {

    /**
     * @brief Constructs a SensorModel with the given configuration.
     *
     * @param config The sensor configuration parameters.
     * @throws std::runtime_error if the resolution or pixel pitch is invalid.
     */
    template <IsSpectral TSpectral>
    SensorModel<TSpectral>::SensorModel(SensorConfig<TSpectral> config)
        : config_{ config }
    {
        if (config.resolution.x < 0 || config.resolution.y < 0) {
            HUIRA_THROW_ERROR("SensorModel::SensorModel - Invalid resolution: " + std::to_string(config.resolution.x) + "x" + std::to_string(config.resolution.y));
        }

        if (config.pitch_x.to_si() <= 0 || config.pitch_y.to_si() <= 0) {
            HUIRA_THROW_ERROR("SensorModel::SensorModel - Pixel pitch must be positive: " + std::to_string(config.pitch_x.to_si()) + "m x " + std::to_string(config.pitch_y.to_si()) + "m");
        }
    }


    /**
     * @brief Sets the sensor resolution.
     *
     * @param resolution The new sensor resolution (width x height).
     * @throws std::runtime_error if the resolution is invalid.
     */
    template <IsSpectral TSpectral>
    void SensorModel<TSpectral>::set_resolution(Resolution resolution) {
        if (resolution.x < 0 || resolution.y < 0) {
            HUIRA_THROW_ERROR("SensorModel::set_resolution - Invalid resolution: " + std::to_string(resolution.x) + "x" + std::to_string(resolution.y));
        }
        config_.resolution = resolution;
    }


    /**
     * @brief Sets the pixel pitch of the sensor.
     *
     * @param pitch_x The horizontal pixel pitch in micrometers.
     * @param pitch_y The vertical pixel pitch in micrometers.
     * @throws std::runtime_error if either pitch is invalid.
     */
    template <IsSpectral TSpectral>
    void SensorModel<TSpectral>::set_pixel_pitch(units::Micrometer pitch_x, units::Micrometer pitch_y) {
        if (pitch_x.to_si() <= 0 || std::isinf(pitch_x.to_si()) || std::isnan(pitch_x.to_si()) ||
            pitch_y.to_si() <= 0 || std::isinf(pitch_y.to_si()) || std::isnan(pitch_y.to_si())) {
            HUIRA_THROW_ERROR("SensorModel::set_pixel_pitch - Pixel pitch must be positive: " + std::to_string(pitch_x.to_si()) + "m x " + std::to_string(pitch_y.to_si()) + "m");
        }
        config_.pitch_x = pitch_x;
        config_.pitch_y = pitch_y;
    }


    /**
     * @brief Returns the pixel pitch of the sensor.
     *
     * @return The pixel pitch as a 2D vector (x, y) in meters.
     */
    template <IsSpectral TSpectral>
    Vec2<float> SensorModel<TSpectral>::pixel_pitch() const {
        return { config_.pitch_x.to_si(), config_.pitch_y.to_si() };
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
    void SensorModel<TSpectral>::set_sensor_size(units::Millimeter width, units::Millimeter height) {
        if (width.to_si() <= 0 || std::isinf(width.to_si()) || std::isnan(width.to_si()) ||
            height.to_si() <= 0 || std::isinf(height.to_si()) || std::isnan(height.to_si())) {
            HUIRA_THROW_ERROR("SensorModel::set_sensor_size - Sensor size must be positive: " + std::to_string(width.to_si()) + "m x " + std::to_string(height.to_si()) + "m");
        }
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
    Vec2<float> SensorModel<TSpectral>::sensor_size() const {
        Vec2<float> pitch = pixel_pitch();
        return { config_.resolution.x * pitch.x, config_.resolution.y * pitch.y };
    }


    /**
     * @brief Sets the quantum efficiency of the sensor.
     *
     * @param qe The quantum efficiency spectrum (values between 0 and 1).
     * @throws std::runtime_error if the values are invalid.
     */
    template <IsSpectral TSpectral>
    void SensorModel<TSpectral>::set_quantum_efficiency(const TSpectral& qe) {
        if (!qe.valid_ratio()) {
            HUIRA_THROW_ERROR("SensorModel::set_quantum_efficiency - Quantum efficiency values must be valid values between 0 and 1.");
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
    void SensorModel<TSpectral>::set_full_well_capacity(float fwc) {
        if (fwc <= 0 || std::isinf(fwc) || std::isnan(fwc)) {
            HUIRA_THROW_ERROR("SensorModel::set_full_well_capacity - Full well capacity must be a positive value: " + std::to_string(fwc) + " e-");
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
    void SensorModel<TSpectral>::set_read_noise(float read_noise) {
        if (read_noise < 0 || std::isinf(read_noise) || std::isnan(read_noise)) {
            HUIRA_THROW_ERROR("SensorModel::set_read_noise - Read noise must be a non-negative value: " + std::to_string(read_noise) + " e-");
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
    void SensorModel<TSpectral>::set_dark_current(float dark_current) {
        if (dark_current < 0 || std::isinf(dark_current) || std::isnan(dark_current)) {
            HUIRA_THROW_ERROR("SensorModel::set_dark_current - Dark current must be a non-negative value: " + std::to_string(dark_current) + " e-/s");
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
    void SensorModel<TSpectral>::set_bias_level_dn(float bias_level_dn) {
        if (bias_level_dn < 0 || std::isinf(bias_level_dn) || std::isnan(bias_level_dn)) {
            HUIRA_THROW_ERROR("SensorModel::set_bias_level_dn - Bias level must be a non-negative value: " + std::to_string(bias_level_dn) + " ADU");
        }
        config_.bias_level_dn = bias_level_dn;
    }


    /**
     * @brief Sets the bit depth of the sensor.
     *
     * @param bit_depth The bit depth (number of bits per pixel).
     * @throws std::runtime_error if the value is invalid.
     */
    template <IsSpectral TSpectral>
    void SensorModel<TSpectral>::set_bit_depth(int bit_depth) {
        if (bit_depth <= 0) {
            HUIRA_THROW_ERROR("SensorModel::set_bit_depth - Bit depth must be a positive integer: " + std::to_string(bit_depth) + " bits");
        }
        config_.bit_depth = bit_depth;
    }


    /**
     * @brief Sets the gain of the sensor in e-/ADU.
     *
     * @param gain The gain in electrons per ADU.
     * @throws std::runtime_error if the value is invalid.
     */
    template <IsSpectral TSpectral>
    void SensorModel<TSpectral>::set_gain_adu(float gain) {
        if (gain <= 0 || std::isinf(gain) || std::isnan(gain)) {
            HUIRA_THROW_ERROR("SensorModel::set_gain_adu - Gain must be a positive value: " + std::to_string(gain) + " e-/ADU");
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
    void SensorModel<TSpectral>::set_unity_db(float unity_db) {
        if (std::isinf(unity_db) || std::isnan(unity_db)) {
            HUIRA_THROW_ERROR("SensorModel::set_unity_db - Unity dB reference level cannot be infinite or NaN: " + std::to_string(unity_db) + " dB");
        }
        config_.unity_db = unity_db;
    }


    /**
     * @brief Sets the gain in dB for the sensor.
     *
     * @param gain_db The gain in decibels.
     */
    template <IsSpectral TSpectral>
    void SensorModel<TSpectral>::set_gain_db(float gain_db) {
        config_.set_gain_db(gain_db);
    }
}
