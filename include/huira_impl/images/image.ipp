#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

namespace huira {

    /**
     * @brief Default constructor creating an empty image.
     */
    template<IsImagePixel PixelT>
    Image<PixelT>::Image()
        : data_{}
        , resolution_{ 0, 0 }
    {
    }

    /**
     * @brief Constructs an image with the specified resolution.
     * 
     * Pixels are default-initialized.
     * 
     * @param resolution The width and height of the image
     */
    template<IsImagePixel PixelT>
    Image<PixelT>::Image(Resolution resolution)
        : data_(static_cast<std::size_t>(resolution.x* resolution.y))
        , resolution_{ resolution }
    {
    }

    /**
     * @brief Constructs an image with the specified resolution and fill value.
     * 
     * All pixels are initialized to the specified fill value.
     * 
     * @param resolution The width and height of the image
     * @param fill_value The value to initialize all pixels with
     */
    template<IsImagePixel PixelT>
    Image<PixelT>::Image(Resolution resolution, const PixelT& fill_value)
        : data_(static_cast<std::size_t>(resolution.x* resolution.y), fill_value)
        , resolution_{ resolution }
    {
    }

    /**
     * @brief Constructs an image with the specified width and height.
     * 
     * Pixels are default-initialized.
     * 
     * @param width The width of the image in pixels
     * @param height The height of the image in pixels
     */
    template<IsImagePixel PixelT>
    Image<PixelT>::Image(int width, int height)
        : data_(static_cast<std::size_t>(width * height))
        , resolution_{ width, height }
    {
    }

    /**
     * @brief Constructs an image with the specified width, height, and fill value.
     * 
     * All pixels are initialized to the specified fill value.
     * 
     * @param width The width of the image in pixels
     * @param height The height of the image in pixels
     * @param fill_value The value to initialize all pixels with
     */
    template<IsImagePixel PixelT>
    Image<PixelT>::Image(int width, int height, const PixelT& fill_value)
        : data_(static_cast<std::size_t>(width * height), fill_value)
        , resolution_{ width, height }
    {
    }

    /**
     * @brief Checks if the image has no pixels.
     * 
     * @return true if the image is empty (zero width or height), false otherwise
     */
    template<IsImagePixel PixelT>
    bool Image<PixelT>::empty() const noexcept {
        return data_.empty();
    }

    /**
     * @brief Checks if the image contains any pixels.
     * 
     * @return true if the image is not empty, false otherwise
     */
    template<IsImagePixel PixelT>
    Image<PixelT>::operator bool() const noexcept {
        return !empty();
    }

    /**
     * @brief Gets the resolution (width and height) of the image.
     * 
     * @return The image resolution
     */
    template<IsImagePixel PixelT>
    Resolution Image<PixelT>::resolution() const noexcept {
        return resolution_;
    }

    /**
     * @brief Gets the width of the image in pixels.
     * 
     * @return The image width
     */
    template<IsImagePixel PixelT>
    int Image<PixelT>::width() const noexcept {
        return resolution_.width;
    }

    /**
     * @brief Gets the height of the image in pixels.
     * 
     * @return The image height
     */
    template<IsImagePixel PixelT>
    int Image<PixelT>::height() const noexcept {
        return resolution_.height;
    }

    /**
     * @brief Gets the total number of pixels in the image.
     * 
     * @return The number of pixels (width × height)
     */
    template<IsImagePixel PixelT>
    std::size_t Image<PixelT>::size() const noexcept {
        return data_.size();
    }

    /**
     * @brief Provides unchecked access to a pixel by linear index.
     * 
     * This operator does not perform bounds checking in release builds.
     * In debug builds, an assertion will fail if the index is out of bounds.
     * 
     * @param index Linear index into the image data (0 to size()-1)
     * @return Reference to the pixel at the specified index
     */
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::operator[](std::size_t index) {
        assert(index < data_.size());
        return data_[index];
    }

    /**
     * @brief Provides unchecked read-only access to a pixel by linear index.
     * 
     * This operator does not perform bounds checking in release builds.
     * In debug builds, an assertion will fail if the index is out of bounds.
     * 
     * @param index Linear index into the image data (0 to size()-1)
     * @return Const reference to the pixel at the specified index
     */
    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::operator[](std::size_t index) const {
        assert(index < data_.size());
        return data_[index];
    }

    /**
     * @brief Provides unchecked access to a pixel by 2D coordinates.
     * 
     * This operator does not perform bounds checking in release builds.
     * In debug builds, an assertion will fail if the coordinates are out of bounds.
     * 
     * @param x The x-coordinate (column) of the pixel
     * @param y The y-coordinate (row) of the pixel
     * @return Reference to the pixel at (x, y)
     */
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::operator()(int x, int y) {
        assert(x < resolution_.width && y < resolution_.height);
        return data_[to_linear(x, y)];
    }

    /**
     * @brief Provides unchecked read-only access to a pixel by 2D coordinates.
     * 
     * This operator does not perform bounds checking in release builds.
     * In debug builds, an assertion will fail if the coordinates are out of bounds.
     * 
     * @param x The x-coordinate (column) of the pixel
     * @param y The y-coordinate (row) of the pixel
     * @return Const reference to the pixel at (x, y)
     */
    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::operator()(int x, int y) const {
        assert(x < resolution_.width && y < resolution_.height);
        return data_[to_linear(x, y)];
    }

    /**
     * @brief Provides unchecked access to a pixel by Pixel coordinates.
     * 
     * The Pixel coordinates are converted to integers by truncation.
     * This operator does not perform bounds checking in release builds.
     * In debug builds, an assertion will fail if the coordinates are out of bounds.
     * 
     * @param pixel The pixel coordinates (floating point values are truncated)
     * @return Reference to the pixel at the converted coordinates
     */
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::operator()(const Pixel& pixel) {
        // TODO Do proper rounding/conversion
        auto x = static_cast<int>(pixel.x);
        auto y = static_cast<int>(pixel.y);
        assert(x < resolution_.width && y < resolution_.height);
        return data_[to_linear(x, y)];
    }

    /**
     * @brief Provides unchecked read-only access to a pixel by Pixel coordinates.
     * 
     * The Pixel coordinates are converted to integers by truncation.
     * This operator does not perform bounds checking in release builds.
     * In debug builds, an assertion will fail if the coordinates are out of bounds.
     * 
     * @param pixel The pixel coordinates (floating point values are truncated)
     * @return Const reference to the pixel at the converted coordinates
     */
    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::operator()(const Pixel& pixel) const {
        // TODO Do proper rounding/conversion
        auto x = static_cast<int>(pixel.x);
        auto y = static_cast<int>(pixel.y);
        assert(x < resolution_.width && y < resolution_.height);
        return data_[to_linear(x, y)];
    }

    /**
     * @brief Provides bounds-checked access to a pixel by linear index.
     * 
     * @param index Linear index into the image data (0 to size()-1)
     * @return Reference to the pixel at the specified index
     * @throws std::out_of_range if the index is out of bounds
     */
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::at(std::size_t index) {
        if (index >= data_.size()) {
            throw std::out_of_range("Image index out of bounds");
        }
        return data_[index];
    }

    /**
     * @brief Provides bounds-checked read-only access to a pixel by linear index.
     * 
     * @param index Linear index into the image data (0 to size()-1)
     * @return Const reference to the pixel at the specified index
     * @throws std::out_of_range if the index is out of bounds
     */
    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::at(std::size_t index) const {
        if (index >= data_.size()) {
            throw std::out_of_range("Image index out of bounds");
        }
        return data_[index];
    }

    /**
     * @brief Provides bounds-checked access to a pixel by 2D coordinates.
     * 
     * @param x The x-coordinate (column) of the pixel
     * @param y The y-coordinate (row) of the pixel
     * @return Reference to the pixel at (x, y)
     * @throws std::out_of_range if the coordinates are out of bounds
     */
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::at(int x, int y) {
        if (x >= resolution_.width || y >= resolution_.height) {
            throw std::out_of_range("Image coordinates out of bounds");
        }
        return data_[to_linear(x, y)];
    }

    /**
     * @brief Provides bounds-checked read-only access to a pixel by 2D coordinates.
     * 
     * @param x The x-coordinate (column) of the pixel
     * @param y The y-coordinate (row) of the pixel
     * @return Const reference to the pixel at (x, y)
     * @throws std::out_of_range if the coordinates are out of bounds
     */
    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::at(int x, int y) const {
        if (x >= resolution_.width || y >= resolution_.height) {
            throw std::out_of_range("Image coordinates out of bounds");
        }
        return data_[to_linear(x, y)];
    }

    /**
     * @brief Provides bounds-checked access to a pixel by Pixel coordinates.
     * 
     * The Pixel coordinates are converted to integers by truncation.
     * 
     * @param pixel The pixel coordinates (floating point values are truncated)
     * @return Reference to the pixel at the converted coordinates
     * @throws std::out_of_range if the coordinates are out of bounds
     */
    template<IsImagePixel PixelT>
    PixelT& Image<PixelT>::at(const Pixel& pixel) {
        // TODO Do proper rounding/conversion
        auto x = static_cast<int>(pixel.x);
        auto y = static_cast<int>(pixel.y);
        if (x >= resolution_.width || y >= resolution_.height) {
            throw std::out_of_range("Image pixel coordinates out of bounds");
        }
        return data_[to_linear(x, y)];
    }

    /**
     * @brief Provides bounds-checked read-only access to a pixel by Pixel coordinates.
     * 
     * The Pixel coordinates are converted to integers by truncation.
     * 
     * @param pixel The pixel coordinates (floating point values are truncated)
     * @return Const reference to the pixel at the converted coordinates
     * @throws std::out_of_range if the coordinates are out of bounds
     */
    template<IsImagePixel PixelT>
    const PixelT& Image<PixelT>::at(const Pixel& pixel) const {
        // TODO Do proper rounding/conversion
        auto x = static_cast<int>(pixel.x);
        auto y = static_cast<int>(pixel.y);
        if (x >= resolution_.width || y >= resolution_.height) {
            throw std::out_of_range("Image pixel coordinates out of bounds");
        }
        return data_[to_linear(x, y)];
    }

    /**
     * @brief Samples the image using nearest-neighbor interpolation.
     * 
     * UV coordinates are expected in the range [0, 1], where (0, 0) is the
     * top-left corner and (1, 1) is the bottom-right corner. Coordinates outside
     * this range are handled according to the specified wrap mode.
     * 
     * @tparam W The wrap mode for handling coordinates outside [0, 1]
     * @param u The horizontal texture coordinate (0 to 1)
     * @param v The vertical texture coordinate (0 to 1)
     * @return The sampled pixel value, or a default-constructed pixel if the image is empty
     */
    template<IsImagePixel PixelT>
    template<WrapMode W>
    PixelT Image<PixelT>::sample_nearest_neighbor(float u, float v) const {
        if (empty()) {
            return PixelT{};
        }

        u = wrap_coordinate<W>(u, 1.0f);
        v = wrap_coordinate<W>(v, 1.0f);

        float px = u * static_cast<float>(resolution_.width - 1);
        float py = v * static_cast<float>(resolution_.height - 1);

        auto x = static_cast<int>(std::round(px));
        auto y = static_cast<int>(std::round(py));

        x = std::min(x, resolution_.width - 1);
        y = std::min(y, resolution_.height - 1);

        return data_[to_linear(x, y)];
    }

    /**
     * @brief Samples the image using bilinear interpolation.
     * 
     * UV coordinates are expected in the range [0, 1], where (0, 0) is the
     * top-left corner and (1, 1) is the bottom-right corner. Coordinates outside
     * this range are handled according to the specified wrap mode.
     * 
     * Bilinear interpolation is supported for arithmetic types, Vec3 types, and
     * SpectralBins. For other pixel types, the function falls back to nearest-neighbor sampling.
     * 
     * @tparam W The wrap mode for handling coordinates outside [0, 1]
     * @param u The horizontal texture coordinate (0 to 1)
     * @param v The vertical texture coordinate (0 to 1)
     * @return The interpolated pixel value, or a default-constructed pixel if the image is empty
     */
    template<IsImagePixel PixelT>
    template<WrapMode W>
    PixelT Image<PixelT>::sample_bilinear(float u, float v) const {
        if (empty()) {
            return PixelT{};
        }

        u = wrap_coordinate<W>(u, 1.0f);
        v = wrap_coordinate<W>(v, 1.0f);

        float px = u * static_cast<float>(resolution_.width - 1);
        float py = v * static_cast<float>(resolution_.height - 1);

        auto x0 = static_cast<int>(px);
        auto y0 = static_cast<int>(py);
        auto x1 = std::min(x0 + 1, resolution_.width - 1);
        auto y1 = std::min(y0 + 1, resolution_.height - 1);

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

    /**
     * @brief Gets a pointer to the underlying pixel data.
     * 
     * @return Pointer to the first pixel in the image data
     */
    template<IsImagePixel PixelT>
    PixelT* Image<PixelT>::data() noexcept {
        return data_.data();
    }

    /**
     * @brief Gets a const pointer to the underlying pixel data.
     * 
     * @return Const pointer to the first pixel in the image data
     */
    template<IsImagePixel PixelT>
    const PixelT* Image<PixelT>::data() const noexcept {
        return data_.data();
    }

    /**
     * @brief Clears the image, removing all pixel data.
     * 
     * After calling this method, the image will be empty with zero width and height.
     */
    template<IsImagePixel PixelT>
    void Image<PixelT>::clear() {
        data_.clear();
        resolution_ = Resolution{ 0, 0 };
    }

    /**
     * @brief Fills all pixels in the image with the specified value.
     * 
     * @param value The value to set for all pixels
     */
    template<IsImagePixel PixelT>
    void Image<PixelT>::fill(const PixelT& value) {
        std::fill(data_.begin(), data_.end(), value);
    }

    /**
     * @brief Converts 2D pixel coordinates to a linear array index.
     * 
     * @param x The x-coordinate (column)
     * @param y The y-coordinate (row)
     * @return The linear index in row-major order
     */
    template<IsImagePixel PixelT>
    std::size_t Image<PixelT>::to_linear(int x, int y) const noexcept {
        return static_cast<std::size_t>(y * resolution_.width + x);
    }

    /**
     * @brief Wraps a texture coordinate according to the specified wrap mode.
     * 
     * @tparam W The wrap mode to apply
     * @param coord The coordinate to wrap
     * @param max The maximum valid coordinate value
     * @return The wrapped coordinate
     */
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

}
