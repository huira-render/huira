#pragma once

#include <cstddef>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class SensorModel {
    public:
        SensorModel(int res_x, int res_y, float width, float height) :
            res_x_{ res_x }, res_y_{ res_y }, width_{ width }, height_{ height } {
        }

        virtual ~SensorModel() = default;

        int res_x() const { return res_x_; }
        int res_y() const { return res_y_; }
        float width() const { return width_; }
        float height() const { return height_; }

        virtual void readout(FrameBuffer<TSpectral>& fb, float exposure_time) const = 0;

    protected:
        int res_x_;
        int res_y_;
        float width_;
        float height_;

        // TODO function to sample poisson distribution
    };
}

#include "huira_impl/cameras/sensors/sensor_model.ipp"
