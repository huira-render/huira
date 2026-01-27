#pragma once

#include <cstddef>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class SimpleSensor : public SensorModel<TSpectral> {
    public:
        SimpleSensor(int res_x, int res_y, float width, float height)
            : SensorModel<TSpectral>(res_x, res_y, width, height) {}
        ~SimpleSensor() override = default;

        // Configuration
        float full_well_capacity = 20000.f;
        float gain = 1.f;                   // e-/ADU
        float read_noise = 10.f;            // e- RMS
        float dark_current = 0.1f;          // e-/s
        int bit_depth = 12;
        TSpectral quantum_efficiency = TSpectral{ 0.6f };

        void readout(FrameBuffer<TSpectral>& fb, float exposure_time) const override;
    };
}

#include "huira_impl/cameras/sensors/simple_sensor.ipp"
