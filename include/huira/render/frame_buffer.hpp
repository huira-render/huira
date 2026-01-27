#pragma once

#include <cstddef>

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

        void ensure_depth() {
            if (depth_.width() != width_ || depth_.height() != height_) {
                depth_ = Image<float>(width_, height_);
            }
        }
        void enable_depth() { if (!depth_) depth_ = Image<float>(width_, height_); }
        Image<float>& depth() { return depth_; }

    private:
        FrameBuffer(std::size_t width, std::size_t height)
            : width_(width), height_(height) {}

        std::size_t width_;
        std::size_t height_;

        Image<float> depth_;

        friend class CameraModel<TSpectral>;
    };
}
