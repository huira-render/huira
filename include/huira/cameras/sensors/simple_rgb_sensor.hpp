#pragma once

#include <cstddef>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    struct SimpleRGBSensorConfig : public SensorConfig<TSpectral> {

    };

    template <IsSpectral TSpectral>
    class SimpleRGBSensor : public SensorModel<TSpectral> {
    public:
        SimpleRGBSensor(SimpleRGBSensorConfig<TSpectral> config = SimpleRGBSensorConfig<TSpectral>{})
            : SensorModel<TSpectral>(config)
        {

        }

        ~SimpleRGBSensor() override = default;

        void readout(FrameBuffer<TSpectral>& fb, float exposure_time) const override;
    };
}

#include "huira_impl/cameras/sensors/simple_rgb_sensor.ipp"
