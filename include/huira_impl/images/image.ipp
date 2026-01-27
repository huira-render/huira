#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cassert>

namespace huira {

    template<IsImagePixel PixelT>
    Image<PixelT>::Image()
        : data_{}
        , width_{ 0 }
        , height_{ 0 }
    {
    }

    template<IsImagePixel PixelT>
    Image<PixelT>::Image(int width, int height)
        : data_(static_cast<std::size_t>(width * height))
        , width_{ width }
        , height_{ height }
    {
    }

    template<IsImagePixel PixelT>
    Image<PixelT>::Image(int width, int height, const PixelT& fill_value)
        : data_(static_cast<std::size_t>(width * height), fill_value)
        , width_{ width }
        , height_{ height }
    {
    }

    template<IsImagePixel PixelT>
    bool Image<PixelT>::empty() const noexcept {
        return data_.empty();
    }

    template<IsImagePixel PixelT>
    Image<PixelT>::operator bool() const noexcept {
        return !empty();
    }

    template<IsImagePixel PixelT>
    int Image<PixelT>::width() const noexcept {
        return width_;
    }

    template<IsImagePixel PixelT>
    int Image<PixelT>::height() const noexcept {
        return height_;
    }

    template<IsImagePixel PixelT>
    std::size_t Image<PixelT>::size() const noexcept {
        return data_.size();
    }

    // Unchecked linear access
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::operator[](std::size_t index) {
        assert(index < data_.size());
        return data_[index];
    }

    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::operator[](std::size_t index) const {
        assert(index < data_.size());
        return data_[index];
    }

    // Unchecked 2D access
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::operator()(int x, int y) {
        assert(x < width_ && y < height_);
        return data_[to_linear(x, y)];
    }

    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::operator()(int x, int y) const {
        assert(x < width_ && y < height_);
        return data_[to_linear(x, y)];
    }

    // Checked linear access
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::at(std::size_t index) {
        if (index >= data_.size()) {
            throw std::out_of_range("Image index out of bounds");
        }
        return data_[index];
    }

    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::at(std::size_t index) const {
        if (index >= data_.size()) {
            throw std::out_of_range("Image index out of bounds");
        }
        return data_[index];
    }

    // Checked 2D access
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::at(int x, int y) {
        if (x >= width_ || y >= height_) {
            throw std::out_of_range("Image coordinates out of bounds");
        }
        return data_[to_linear(x, y)];
    }

    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::at(int x, int y) const {
        if (x >= width_ || y >= height_) {
            throw std::out_of_range("Image coordinates out of bounds");
        }
        return data_[to_linear(x, y)];
    }

    // Checked Pixel access
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::at(const Pixel& pixel) {
        auto x = static_cast<int>(pixel.x);
        auto y = static_cast<int>(pixel.y);
        if (x >= width_ || y >= height_) {
            throw std::out_of_range("Image pixel coordinates out of bounds");
        }
        return data_[to_linear(x, y)];
    }

    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::at(const Pixel& pixel) const {
        auto x = static_cast<int>(pixel.x);
        auto y = static_cast<int>(pixel.y);
        if (x >= width_ || y >= height_) {
            throw std::out_of_range("Image pixel coordinates out of bounds");
        }
        return data_[to_linear(x, y)];
    }

    template<IsImagePixel PixelT>
    template<WrapMode W>
    PixelT Image<PixelT>::sample_nearest_neighbor(float u, float v) const {
        if (empty()) {
            return PixelT{};
        }

        u = wrap_coordinate<W>(u, 1.0f);
        v = wrap_coordinate<W>(v, 1.0f);

        float px = u * static_cast<float>(width_ - 1);
        float py = v * static_cast<float>(height_ - 1);

        auto x = static_cast<int>(std::round(px));
        auto y = static_cast<int>(std::round(py));

        x = std::min(x, width_ - 1);
        y = std::min(y, height_ - 1);

        return data_[to_linear(x, y)];
    }

    template<IsImagePixel PixelT>
    template<WrapMode W>
    PixelT Image<PixelT>::sample_bilinear(float u, float v) const {
        if (empty()) {
            return PixelT{};
        }

        u = wrap_coordinate<W>(u, 1.0f);
        v = wrap_coordinate<W>(v, 1.0f);

        float px = u * static_cast<float>(width_ - 1);
        float py = v * static_cast<float>(height_ - 1);

        auto x0 = static_cast<int>(px);
        auto y0 = static_cast<int>(py);
        auto x1 = std::min(x0 + 1, width_ - 1);
        auto y1 = std::min(y0 + 1, height_ - 1);

        float fx = px - static_cast<float>(x0);
        float fy = py - static_cast<float>(y0);

        const PixelT& p00 = data_[to_linear(x0, y0)];
        const PixelT& p10 = data_[to_linear(x1, y0)];
        const PixelT& p01 = data_[to_linear(x0, y1)];
        const PixelT& p11 = data_[to_linear(x1, y1)];

        if constexpr (std::is_arithmetic_v<PixelT>) {
            float result = 
                static_cast<float>(p00) * (1.0f - fx) * (1.0f - fy) +
                static_cast<float>(p10) * fx * (1.0f - fy) +
                static_cast<float>(p01) * (1.0f - fx) * fy +
                static_cast<float>(p11) * fx * fy;
            return static_cast<PixelT>(result);
        } 
        else if constexpr (is_vec3_v<PixelT>) {
            using S = typename Traits::Scalar;
            return PixelT{
                static_cast<S>(p00.x * (1.0f - fx) * (1.0f - fy) + p10.x * fx * (1.0f - fy) + p01.x * (1.0f - fx) * fy + p11.x * fx * fy),
                static_cast<S>(p00.y * (1.0f - fx) * (1.0f - fy) + p10.y * fx * (1.0f - fy) + p01.y * (1.0f - fx) * fy + p11.y * fx * fy),
                static_cast<S>(p00.z * (1.0f - fx) * (1.0f - fy) + p10.z * fx * (1.0f - fy) + p01.z * (1.0f - fx) * fy + p11.z * fx * fy)
            };
        }
        else if constexpr (is_spectral_bins_v<PixelT>) {
            return p00 * ((1.0f - fx) * (1.0f - fy)) +
                   p10 * (fx * (1.0f - fy)) +
                   p01 * ((1.0f - fx) * fy) +
                   p11 * (fx * fy);
        }
        else {
            // Fallback to nearest
            return (fx < 0.5f) ? ((fy < 0.5f) ? p00 : p01) : ((fy < 0.5f) ? p10 : p11);
        }
    }

    template<IsImagePixel PixelT>
    PixelT* Image<PixelT>::data() noexcept {
        return data_.data();
    }

    template<IsImagePixel PixelT>
    const PixelT* Image<PixelT>::data() const noexcept {
        return data_.data();
    }

    template<IsImagePixel PixelT>
    void Image<PixelT>::resize(int width, int height) {
        width_ = width;
        height_ = height;
        data_.resize(static_cast<std::size_t>(width * height));
    }

    template<IsImagePixel PixelT>
    void Image<PixelT>::resize(int width, int height, const PixelT& fill_value) {
        width_ = width;
        height_ = height;
        data_.assign(static_cast<std::size_t>(width * height), fill_value);
    }

    template<IsImagePixel PixelT>
    void Image<PixelT>::clear() {
        data_.clear();
        width_ = 0;
        height_ = 0;
    }

    template<IsImagePixel PixelT>
    void Image<PixelT>::fill(const PixelT& value) {
        std::fill(data_.begin(), data_.end(), value);
    }

    template<IsImagePixel PixelT>
    std::size_t Image<PixelT>::to_linear(int x, int y) const noexcept {
        return static_cast<std::size_t>(y * width_ + x);
    }

    template<IsImagePixel PixelT>
    template<WrapMode W>
    float Image<PixelT>::wrap_coordinate(float coord, float max) const noexcept {
        if constexpr (W == WrapMode::Clamp) {
            return std::clamp(coord, 0.0f, max);
        }
        else if constexpr (W == WrapMode::Repeat) {
            coord = std::fmod(coord, max);
            if (coord < 0.0f) {
                coord += max;
            }
            return coord;
        }
        else if constexpr (W == WrapMode::Mirror) {
            coord = std::fabs(coord);
            int period = static_cast<int>(coord / max);
            coord = std::fmod(coord, max);
            if (period % 2 == 1) {
                coord = max - coord;
            }
            return std::clamp(coord, 0.0f, max);
        }
        else {
            return std::clamp(coord, 0.0f, max);
        }
    }

} // namespace huira
