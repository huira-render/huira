#pragma once

#include <cstddef>
#include <vector>

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/pixel_concepts.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/core/types.hpp"

namespace huira {
    /**
     * @brief Type traits for image pixel types.
     * 
     * Provides compile-time information about pixel types including the underlying
     * scalar type and the number of channels.
     * 
     * @tparam T The pixel type (must satisfy IsImagePixel concept)
     */
    template<IsImagePixel T>
    struct ImagePixelTraits {
        using Scalar = T;
        static constexpr int channels = 1;
    };

    /**
     * @brief Specialization for Vec3 pixel types.
     */
    template<IsFloatingPoint T>
    struct ImagePixelTraits<Vec3<T>> {
        using Scalar = T;
        static constexpr int channels = 3;
    };

    /**
     * @brief Specialization for SpectralBins pixel types.
     */
    template<std::size_t N, auto... Args>
    struct ImagePixelTraits<SpectralBins<N, Args...>> {
        using Scalar = float;
        static constexpr std::size_t channels = N;
    };

    /**
     * @brief Specifies how texture coordinates outside [0,1] are handled during sampling.
     */
    enum class WrapMode {
        Clamp,   ///< Clamp coordinates to [0,1]
        Repeat,  ///< Repeat texture by wrapping coordinates
        Mirror   ///< Mirror texture at boundaries
    };

    /**
     * @brief A 2D image container with templated pixel types.
     * 
     * The Image class provides a flexible container for 2D image data with support
     * for various pixel types including scalar values, Vec3 for RGB/color data, and
     * SpectralBins for spectral imaging. It offers both checked and unchecked access
     * methods, as well as sampling operations with different wrap modes.
     * 
     * Memory is stored in row-major order, with the origin at the top-left corner.
     * Pixel coordinates (x, y) map to image space where x increases to the right
     * and y increases downward.
     * 
     * @tparam PixelT The type of pixel stored (must satisfy IsImagePixel concept)
     */
    template<IsImagePixel PixelT>
    class Image {
    public:
        using PixelType = PixelT;
        using Traits = ImagePixelTraits<PixelT>;
        using Scalar = typename Traits::Scalar;

        Image();
        Image(Resolution resolution);
        Image(Resolution resolution, const PixelT& fill_value);
        Image(int width, int height);
        Image(int width, int height, const PixelT& fill_value);

        Image(const Image&) = default;
        Image(Image&&) noexcept = default;
        Image& operator=(const Image&) = default;
        Image& operator=(Image&&) noexcept = default;

        ~Image() = default;

        [[nodiscard]] bool empty() const noexcept;
        explicit operator bool() const noexcept;

        [[nodiscard]] Resolution resolution() const noexcept;
        [[nodiscard]] int width() const noexcept;
        [[nodiscard]] int height() const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;

        // Unchecked access (asserts in debug builds only)
        [[nodiscard]] PixelT& operator[](std::size_t index);
        [[nodiscard]] const PixelT& operator[](std::size_t index) const;

        [[nodiscard]] PixelT& operator()(int x, int y);
        [[nodiscard]] const PixelT& operator()(int x, int y) const;

        [[nodiscard]] PixelT& operator()(const Pixel& pixel);
        [[nodiscard]] const PixelT& operator()(const Pixel& pixel) const;

        // Checked access (throws std::out_of_range)
        [[nodiscard]] PixelT& at(std::size_t index);
        [[nodiscard]] const PixelT& at(std::size_t index) const;

        [[nodiscard]] PixelT& at(int x, int y);
        [[nodiscard]] const PixelT& at(int x, int y) const;

        [[nodiscard]] PixelT& at(const Pixel& pixel);
        [[nodiscard]] const PixelT& at(const Pixel& pixel) const;

        // Sampling (normalized UV coordinates in [0,1])
        template<WrapMode W = WrapMode::Clamp>
        [[nodiscard]] PixelT sample_nearest_neighbor(float u, float v) const;

        template<WrapMode W = WrapMode::Clamp>
        [[nodiscard]] PixelT sample_bilinear(float u, float v) const;

        [[nodiscard]] PixelT* data() noexcept;
        [[nodiscard]] const PixelT* data() const noexcept;

        [[nodiscard]] int sensor_bit_depth() const noexcept { return sensor_bit_depth_; }
        void set_sensor_bit_depth(int bits) noexcept { sensor_bit_depth_ = bits; }

        void clear();
        void fill(const PixelT& value);
        void reset(const PixelT& value = PixelT{ 0 }) { fill(value); }

    private:
        std::vector<PixelT> data_;
        Resolution resolution_;

        int sensor_bit_depth_ = 0;

        [[nodiscard]] std::size_t to_linear(int x, int y) const noexcept;

        template<WrapMode W>
        [[nodiscard]] float wrap_coordinate(float coord, float max) const noexcept;
    };

}

#include "huira_impl/images/image.ipp"
