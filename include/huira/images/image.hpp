#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <concepts>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/spectral/spectral_bins.hpp"

namespace huira {

    // Type traits for detecting SpectralBins specializations
    template<typename T>
    struct is_spectral_bins : std::false_type {};

    template<size_t N, auto... Args>
    struct is_spectral_bins<SpectralBins<N, Args...>> : std::true_type {};

    template<typename T>
    inline constexpr bool is_spectral_bins_v = is_spectral_bins<T>::value;

    // Type traits for detecting Vec3 specializations
    template<typename T>
    struct is_vec3 : std::false_type {};

    template<IsFloatingPoint T>
    struct is_vec3<Vec3<T>> : std::true_type {};

    template<typename T>
    inline constexpr bool is_vec3_v = is_vec3<T>::value;

    // Valid pixel types for Image<T>
    template<typename T>
    concept IsImagePixel = 
        std::same_as<T, std::int8_t> ||
        std::same_as<T, std::int16_t> ||
        std::same_as<T, std::int32_t> ||
        std::same_as<T, std::int64_t> ||
        std::same_as<T, std::uint8_t> ||
        std::same_as<T, std::uint16_t> ||
        std::same_as<T, std::uint32_t> ||
        std::same_as<T, std::uint64_t> ||
        std::same_as<T, std::size_t> ||
        std::same_as<T, float> ||
        std::same_as<T, double> ||
        is_vec3_v<T> ||
        is_spectral_bins_v<T>;

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
