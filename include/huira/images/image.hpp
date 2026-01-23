#pragma once

#include <vector>
#include <cstddef>

#include "huira/core/spectral_bins.hpp"
#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/pixel_concepts.hpp"

namespace huira {
    // Scalar type and channel count for a given pixel type
    template<IsImagePixel T>
    struct ImagePixelTraits {
        using Scalar = T;
        static constexpr std::size_t channels = 1;
    };

    template<IsFloatingPoint T>
    struct ImagePixelTraits<Vec3<T>> {
        using Scalar = T;
        static constexpr std::size_t channels = 3;
    };

    template<std::size_t N, auto... Args>
    struct ImagePixelTraits<SpectralBins<N, Args...>> {
        using Scalar = float;
        static constexpr std::size_t channels = N;
    };

    enum class WrapMode {
        Clamp,
        Repeat,
        Mirror
    };

    template<IsImagePixel PixelT>
    class Image {
    public:
        using PixelType = PixelT;
        using Traits = ImagePixelTraits<PixelT>;
        using Scalar = typename Traits::Scalar;

        Image();
        Image(std::size_t width, std::size_t height);
        Image(std::size_t width, std::size_t height, const PixelT& fill_value);

        Image(const Image&) = default;
        Image(Image&&) noexcept = default;
        Image& operator=(const Image&) = default;
        Image& operator=(Image&&) noexcept = default;

        ~Image() = default;

        [[nodiscard]] bool empty() const noexcept;
        explicit operator bool() const noexcept;

        [[nodiscard]] std::size_t width() const noexcept;
        [[nodiscard]] std::size_t height() const noexcept;
        [[nodiscard]] std::size_t size() const noexcept;

        // Unchecked access (asserts in debug builds only)
        [[nodiscard]] PixelT& operator[](std::size_t index);
        [[nodiscard]] const PixelT& operator[](std::size_t index) const;

        [[nodiscard]] PixelT& operator()(std::size_t x, std::size_t y);
        [[nodiscard]] const PixelT& operator()(std::size_t x, std::size_t y) const;

        // Checked access (throws std::out_of_range)
        [[nodiscard]] PixelT& at(std::size_t index);
        [[nodiscard]] const PixelT& at(std::size_t index) const;

        [[nodiscard]] PixelT& at(std::size_t x, std::size_t y);
        [[nodiscard]] const PixelT& at(std::size_t x, std::size_t y) const;

        [[nodiscard]] PixelT& at(const Pixel& pixel);
        [[nodiscard]] const PixelT& at(const Pixel& pixel) const;

        // Sampling (normalized UV coordinates in [0,1])
        template<WrapMode W = WrapMode::Clamp>
        [[nodiscard]] PixelT sample_nearest_neighbor(float u, float v) const;

        template<WrapMode W = WrapMode::Clamp>
        [[nodiscard]] PixelT sample_bilinear(float u, float v) const;

        [[nodiscard]] PixelT* data() noexcept;
        [[nodiscard]] const PixelT* data() const noexcept;

        void resize(std::size_t width, std::size_t height);
        void resize(std::size_t width, std::size_t height, const PixelT& fill_value);

        void clear();
        void fill(const PixelT& value);

    private:
        std::vector<PixelT> data_;
        std::size_t width_;
        std::size_t height_;

        [[nodiscard]] std::size_t to_linear(std::size_t x, std::size_t y) const noexcept;

        template<WrapMode W>
        [[nodiscard]] float wrap_coordinate(float coord, float max) const noexcept;
    };

} // namespace huira

#include "huira_impl/images/image.ipp"
