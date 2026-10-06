#pragma once

#include <cstddef>
#include <vector>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/pixel_concepts.hpp"
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
template <IsImagePixel T>
struct ImagePixelTraits {
    using Scalar = T;
    static constexpr int channels = 1;
};

/**
 * @brief Specialization for Vec3 pixel types.
 */
template <IsFloatingPoint T>
struct ImagePixelTraits<Vec3<T>> {
    using Scalar = T;
    static constexpr int channels = 3;
};

/**
 * @brief Specialization for SpectralBins pixel types.
 */
template <std::size_t N, auto... Args>
struct ImagePixelTraits<SpectralBins<N, Args...>> {
    using Scalar = float;
    static constexpr std::size_t channels = N;
};

/**
 * @brief Specifies how texture coordinates outside [0,1] are handled during sampling.
 */
enum class WrapMode {
    Clamp,  ///< Clamp coordinates to [0,1]
    Repeat, ///< Repeat texture by wrapping coordinates
    Mirror  ///< Mirror texture at boundaries
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
 * and y increases downward. This layout is fixed: a camera's PixelConvention only changes how
 * positions on the image are numbered, and file formats with bottom-up rows are flipped as they
 * are read and written.
 *
 * @tparam PixelT The type of pixel stored (must satisfy IsImagePixel concept)
 */
template <IsImagePixel PixelT>
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
    template <WrapMode W = WrapMode::Repeat>
    [[nodiscard]] PixelT sample_nearest_neighbor(float u, float v) const;

    template <WrapMode W = WrapMode::Repeat>
    [[nodiscard]] PixelT sample_bilinear(float u, float v) const;

    [[nodiscard]] PixelT* data() noexcept;
    [[nodiscard]] const PixelT* data() const noexcept;

    /// The bit depth of the sensor whose response this is, or 0 for any other image. A sensor
    /// response holds DN / (2^bits - 1); the image writers use this to write DN (see
    /// PixelScaling).
    [[nodiscard]] int sensor_bit_depth() const noexcept { return sensor_bit_depth_; }
    void set_sensor_bit_depth(int bits) noexcept { sensor_bit_depth_ = bits; }

    void convolve(const Image& kernel)
        requires(!IsInteger<PixelT>);

    void clear();
    void fill(const PixelT& value);
    void reset(const PixelT& value = PixelT{0}) { fill(value); }

    Image<float> get_channel(std::size_t channel) const;

    Image operator+(const Image& other) const;

  private:
    std::vector<PixelT> data_;
    Resolution resolution_;

    int sensor_bit_depth_ = 0;

    [[nodiscard]] std::size_t to_linear(int x, int y) const noexcept;

    template <WrapMode W>
    [[nodiscard]] float wrap_coordinate(float coord, float max) const noexcept;

    void convolve_direct_(const Image& kernel)
        requires(!IsInteger<PixelT>);

    void convolve_fft_(const Image& kernel)
        requires(!IsInteger<PixelT>);
};

/**
 * @brief How an image's values are encoded: as linear values (what Huira renders), or encoded
 * with the sRGB curve or a gamma. Readers record the file's encoding here, and writers label
 * files with it; neither converts anything. linear_to_srgb() and srgb_to_linear() convert, and
 * set it.
 */
enum class ColorSpaceHint { Linear, sRGB, Gamma, Unknown };

/**
 * @brief How an image writer turns the values of an image that has a sensor bit depth (see
 * Image::sensor_bit_depth(), which a sensor readout sets) into the file's values.
 *
 * - Counts: the file holds the sensor's digital numbers (DN), as each format's standard allows
 *   (see docs/design_overview/image_output.rst). Where the file has fewer bits than the sensor,
 *   the lowest bits are dropped, as a camera with a narrower output would.
 * - FullRange: the values, in [0, 1], are stretched over the file's whole range (1 becomes 255 or
 *   65535), as for an image without a sensor bit depth. For display.
 *
 * An image without a sensor bit depth is always written FullRange (or as is, to a float file).
 */
enum class PixelScaling { Counts, FullRange };

/**
 * @brief An image, with what a file holds besides the pixels: an alpha channel, the encoding
 * (color_space, gamma_value), and, for writing, the file's bit depth per sample and how values
 * are scaled to it.
 *
 * bit_depth is the file's (8 or 16 bits per sample, say); the sensor's bit depth, if the image
 * came from one, is the image's own (Image::sensor_bit_depth()).
 */
template <IsImagePixel PixelT>
struct ImageBundle {
    Image<PixelT> image;
    Image<float> alpha{};

    ColorSpaceHint color_space = ColorSpaceHint::Linear;
    float gamma_value = 1.0f;

    int bit_depth = 8;

    PixelScaling scaling = PixelScaling::Counts;

    ImageBundle() = default;

    ImageBundle(Image<PixelT> img) : image(std::move(img)) {}

    ImageBundle(Image<PixelT> img, Image<float> alpha_copy)
        : image(std::move(img)), alpha(std::move(alpha_copy))
    {
    }

    template <typename OtherPixelT>
    ImageBundle(const ImageBundle<OtherPixelT>& other_bundle, Image<PixelT> new_image)
        : image(std::move(new_image)), alpha(other_bundle.alpha),
          color_space(other_bundle.color_space), gamma_value(other_bundle.gamma_value),
          bit_depth(other_bundle.bit_depth), scaling(other_bundle.scaling)
    {
    }
};
} // namespace huira

#include "huira_impl/images/image.ipp"
