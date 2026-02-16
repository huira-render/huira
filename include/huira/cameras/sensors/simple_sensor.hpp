
#pragma once

#include <cstddef>

#include "huira/cameras/sensors/sensor_model.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"

namespace huira {

    /**
     * @brief Configuration for SimpleSensor.
     *
     * Inherits all parameters from SensorConfig. Used to initialize SimpleSensor instances.
     *
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    struct SimpleSensorConfig : public SensorConfig<TSpectral> {
        
    };


    /**
     * @brief Simple sensor model with basic noise and ADC simulation.
     *
     * Implements a basic sensor readout model, including shot noise, read noise, and quantization.
     *
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class SimpleSensor : public SensorModel<TSpectral> {
    public:
        SimpleSensor(SimpleSensorConfig<TSpectral> config = SimpleSensorConfig<TSpectral>{})
            : SensorModel<TSpectral>(config)
        {
        }

        ~SimpleSensor() override = default;

        void readout(FrameBuffer<TSpectral>& fb, float exposure_time) const override;
    };
}

#include "huira_impl/cameras/sensors/simple_sensor.ipp"
