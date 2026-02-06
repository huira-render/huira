#pragma once

#include <cstddef>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    struct SensorConfig {
        Resolution resolution{ 1024, 1024 };
        Vec2<float> pixel_pitch{ 8.5f * 1e-6f, 8.5f * 1e-6f }; // 8.5 microns

        TSpectral quantum_efficiency = TSpectral{ 0.7f };

        float full_well_capacity = 20000.f; // e-

        float read_noise = 50.f;           // e- RMS
        float dark_current = 1.f;          // e-/s
        float bias_level_dn = 20.f;        // ADU

        int bit_depth = 12;

        float gain = 4.8f;                 // e-/ADU

        float unity_db = 0.f; // Reference level for gain in dB
        void set_gain_db(float gain_db) { gain = std::pow(10.f, (unity_db - gain_db) / 20.f); }
        float gain_db() const { return unity_db - 20.f * std::log10(gain); }
    };

    template <IsSpectral TSpectral>
    class SensorModel {
    public:
        SensorModel() = default;
        SensorModel(SensorConfig<TSpectral> config) : config_{ config } {}

        virtual ~SensorModel() = default;

        Resolution resolution() const { return { config_.resolution }; }
        void set_resolution(Resolution res) {
            config_.resolution = res;
        }

        Vec2<float> pixel_pitch() const { return { config_.pixel_pitch }; }
        void set_pixel_pitch(const Vec2<float>& pitch) {
            config_.pixel_pitch = pitch;
        }

        TSpectral quantum_efficiency() const { return config_.quantum_efficiency; }
        void set_quantum_efficiency(const TSpectral& qe) {
            config_.quantum_efficiency = qe;
        }

        Vec2<float> sensor_size() const { return { config_.resolution * config_.pixel_pitch }; }

        void set_full_well_capacity(float fwc) { config_.full_well_capacity = fwc; }
        float full_well_capacity() const { return config_.full_well_capacity; }

        void set_read_noise(float read_noise) { config_.read_noise = read_noise; }
        float read_noise() const { return config_.read_noise; }

        void set_dark_current(float dark_current) { config_.dark_current = dark_current; }
        float dark_current() const { return config_.dark_current; }

        void set_bias_level_dn(float bias_level_dn) { config_.bias_level_dn = bias_level_dn; }
        float bias_level_dn() const { return config_.bias_level_dn; }

        void set_bit_depth(int bit_depth) { config_.bit_depth = bit_depth; }
        int bit_depth() const { return config_.bit_depth; }

        void set_gain_adu(float gain) { config_.gain = gain; }
        float gain_adu() const { return config_.gain; }

        void set_unity_db(float unity_db) { config_.unity_db = unity_db; }
        float unity_db() const { return config_.unity_db; }

        void set_gain_db(float gain_db) { config_.set_gain_db(gain_db); }
        float gain_db() const { return config_.gain_db(); }


        virtual void readout(FrameBuffer<TSpectral>& fb, float exposure_time) const = 0;

    protected:
        SensorConfig<TSpectral> config_;

        // TODO function to sample poisson distribution
    };

    template <typename T>
    struct is_sensor : std::false_type {};

    template <template <typename> class Derived, typename TSpectral>
        requires std::derived_from<Derived<TSpectral>, SensorModel<TSpectral>>
    struct is_distortion<Derived<TSpectral>> : std::true_type {};

    template <typename T>
    concept IsSensor = is_sensor<T>::value;
}

#include "huira_impl/cameras/sensors/sensor_model.ipp"
