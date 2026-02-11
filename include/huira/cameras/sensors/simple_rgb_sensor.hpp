#pragma once

#include <cstddef>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"

namespace huira {
    struct SimpleRGBSensorConfig : public SensorConfig<RGB> {

    };

    class SimpleRGBSensor : public SensorModel<RGB> {
    public:
        SimpleRGBSensor(SimpleRGBSensorConfig config = SimpleRGBSensorConfig{})
            : SensorModel<RGB>(config)
        {

        }

        ~SimpleRGBSensor() override = default;

        void readout(FrameBuffer<RGB>& fb, float exposure_time) const override;
    };
}

#include "huira_impl/cameras/sensors/simple_rgb_sensor.ipp"
