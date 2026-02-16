
#pragma once

#include <cstddef>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/units/units.hpp"
#include "huira/render/frame_buffer.hpp"

namespace huira {
    // Forward Declare
    template <IsSpectral TSpectral>
    class CameraModel;


    /**
     * @brief Configuration parameters for a sensor model.
     *
     * Holds all physical and electronic parameters needed to describe a sensor, including resolution, pixel pitch,
     * quantum efficiency, noise, gain, and rotation. Used to initialize and configure SensorModel instances.
     *
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    struct SensorConfig {
        Resolution resolution{ 1024, 1024 };
        units::Micrometer pitch_x{ 8.5 };
        units::Micrometer pitch_y{ 8.5 };

        TSpectral quantum_efficiency = TSpectral{ 0.7f };

        float full_well_capacity = 20000.f; // e-

        float read_noise = 10.f;           // e- RMS
        float dark_current = 1.f;          // e-/s
        float bias_level_dn = 10.f;        // ADU

        int bit_depth = 12;

        float gain = 1.22f;                 // e-/ADU

        units::Radian rotation = units::Radian{ 0 }; // Sensor rotation angle

        float unity_db = 0.f; // Reference level for gain in dB

        /**
         * @brief Sets the gain in dB for the sensor.
         * @param gain_db The gain in decibels.
         * @throws std::runtime_error if the value is not finite.
         */
        void set_gain_db(float gain_db) {
            if (std::isinf(gain_db) || std::isnan(gain_db)) {
                HUIRA_THROW_ERROR("SensorModel::set_gain_db - Gain in dB must be a finite value: " + std::to_string(gain_db) + " dB");
            }
            gain = std::pow(10.f, (unity_db - gain_db) / 20.f); 
        }
        /**
         * @brief Returns the gain in dB for the sensor.
         * @return The gain in decibels.
         */
        float gain_db() const { return unity_db - 20.f * std::log10(gain); }
    };


    /**
     * @brief Abstract base class for sensor models.
     *
     * Defines the interface and configuration for all sensor models, including pixel pitch, quantum efficiency, and noise parameters.
     *
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class SensorModel {
    public:
        SensorModel() = default;
        SensorModel(SensorConfig<TSpectral> config);

        virtual ~SensorModel() = default;

        void set_resolution(Resolution resolution);
        Resolution resolution() const { return { config_.resolution }; }

        void set_pixel_pitch(units::Micrometer pitch_x, units::Micrometer pitch_y);
        Vec2<float> pixel_pitch() const;

        void set_sensor_size(units::Millimeter width, units::Millimeter height);
        Vec2<float> sensor_size() const;

        void set_quantum_efficiency(const TSpectral& qe);
        TSpectral quantum_efficiency() const { return config_.quantum_efficiency; }

        void set_full_well_capacity(float fwc);
        float full_well_capacity() const { return config_.full_well_capacity; }

        void set_read_noise(float read_noise);
        float read_noise() const { return config_.read_noise; }

        void set_dark_current(float dark_current);
        float dark_current() const { return config_.dark_current; }

        void set_bias_level_dn(float bias_level_dn);
        float bias_level_dn() const { return config_.bias_level_dn; }

        void set_bit_depth(int bit_depth);
        int bit_depth() const { return config_.bit_depth; }

        void set_gain_adu(float gain);
        float gain_adu() const { return config_.gain; }

        void set_unity_db(float unity_db);
        float unity_db() const { return config_.unity_db; }

        void set_gain_db(float gain_db);
        float gain_db() const { return config_.gain_db(); }

        void set_rotation(units::Radian angle) { config_.rotation = angle; }
        units::Radian rotation() const { return config_.rotation; }

        virtual void readout(FrameBuffer<TSpectral>& fb, float exposure_time) const = 0;

    protected:
        SensorConfig<TSpectral> config_;

        // TODO function to sample poisson distribution
        friend class CameraModel<TSpectral>;
    };

    template <typename T>
    struct is_sensor : std::false_type {};

    // Forward Declare
    class SimpleRGBSensor;

    template <>
    struct is_sensor<SimpleRGBSensor> : std::true_type {};

    template <template <typename> class Derived, typename TSpectral>
        requires std::derived_from<Derived<TSpectral>, SensorModel<TSpectral>>
    struct is_distortion<Derived<TSpectral>> : std::true_type {};

    template <typename T>
    concept IsSensor = is_sensor<T>::value;
}

#include "huira_impl/cameras/sensors/sensor_model.ipp"
