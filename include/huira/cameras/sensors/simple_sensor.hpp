#pragma once

#include <cstddef>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    struct SimpleSensorConfig : public SensorConfig<TSpectral> {

    };

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
