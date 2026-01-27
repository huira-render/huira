#pragma once

#include <cstddef>
#include <limits>

#include "huira/images/image.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class CameraModel;

    template <IsSpectral TSpectral>
    class FrameBuffer {
    public:
        FrameBuffer() = delete;

        std::size_t width() const { return width_; }
        std::size_t height() const { return height_; }


        void enable_depth() {
            if (depth_.width() != width_ || depth_.height() != height_) {
                depth_ = Image<float>(width_, height_, std::numeric_limits<float>::infinity());
            }
        }
        Image<float>& depth() { return depth_; }


        void enable_received_power() {
            if (received_power_.width() != width_ || received_power_.height() != height_) {
                received_power_ = Image<TSpectral>(width_, height_);
            }
        }
        Image<TSpectral>& received_power() { return received_power_; }


        void enable_sensor_response() {
            if (sensor_response_.width() != width_ || sensor_response_.height() != height_) {
                sensor_response_ = Image<float>(width_, height_, 0.f);
            }
        }
        Image<float>& sensor_response() { return sensor_response_; }

    private:
        FrameBuffer(int width, int height)
            : width_(width), height_(height) {}

        int width_;
        int height_;

        Image<TSpectral> received_power_;
        Image<float> sensor_response_;

        Image<float> depth_;

        friend class CameraModel<TSpectral>;
    };
}
