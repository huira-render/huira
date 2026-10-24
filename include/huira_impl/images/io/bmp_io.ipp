#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <utility>
#include <vector>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/convert_pixel.hpp"
#include "huira/images/io/io_util.hpp"
#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {

struct BMPData {
    Resolution resolution{0, 0};
    int width = 0;
    int height = 0;

    std::vector<unsigned char> raw_data;
    int channels = 3;
    bool has_alpha = false;
};

/// BMP compression types this reader reads: none, and (for 32-bit pixels) color masks, with or
/// without one for alpha.
constexpr uint32_t BMP_BI_RGB = 0;
constexpr uint32_t BMP_BI_BITFIELDS = 3;
constexpr uint32_t BMP_BI_ALPHABITFIELDS = 6;

/**
 * @brief Where one channel is in a 32-bit BMP pixel: the mask's bits, and how far up they are.
 */
struct BMPChannelMask_ {
    uint32_t mask = 0;
    int shift = 0;
    uint32_t max = 0; ///< The channel's largest value: mask >> shift (0 without the channel)

    explicit BMPChannelMask_(uint32_t bits = 0) : mask(bits)
    {
        if (mask != 0) {
            shift = std::countr_zero(mask);
            max = mask >> shift;
        }
    }

    /// The channel's value in a pixel, scaled to 8 bits.
    [[nodiscard]] unsigned char value(uint32_t pixel) const
    {
        if (max == 0) {
            return 0;
        }
        const uint64_t v = (pixel & mask) >> shift;
        return static_cast<unsigned char>((v * 255 + max / 2) / max);
    }
};

/**
 * @brief Decodes a BMP from an in-memory buffer into raw byte data.
 *
 * Reads the BMP file header and DIB header, validates the format, and extracts
 * pixel data. Handles bottom-up row ordering (standard for BMP) by flipping
 * rows during extraction. Supports 24-bit RGB and 32-bit pixels, uncompressed or with color
 * masks (BI_BITFIELDS, BI_ALPHABITFIELDS).
 *
 * A 32-bit pixel's alpha is its alpha mask's bits, if it has one. Without masks (BI_RGB), the
 * fourth byte is officially unused, and many programs leave it 0 though the image is opaque: it
 * is taken for alpha only if some pixel sets it.
 *
 * @param data Pointer to the BMP data in memory
 * @param size Size of the data in bytes
 * @return Raw decoded data with resolution and pixel buffer
 * @throws std::runtime_error if the data is not a valid or supported BMP
 */
inline BMPData read_bmp_raw_(const unsigned char* data, std::size_t size)
{
    HUIRA_LOG_INFO("read_bmp_raw_ - Reading BMP from memory (" + std::to_string(size) + " bytes)");

    if (size < 54) {
        HUIRA_THROW_ERROR("read_bmp_raw_ - Data too small to be a valid BMP (" +
                          std::to_string(size) + " bytes)");
    }

    // Validate signature
    if (data[0] != 'B' || data[1] != 'M') {
        HUIRA_THROW_ERROR("read_bmp_raw_ - Data is not a valid BMP (bad signature)");
    }

    // Read BMP file header (14 bytes)
    uint32_t pixel_offset;
    std::memcpy(&pixel_offset, &data[10], sizeof(uint32_t));

    // Read DIB header
    uint32_t dib_size;
    std::memcpy(&dib_size, &data[14], sizeof(uint32_t));

    if (dib_size < 40) {
        HUIRA_THROW_ERROR("read_bmp_raw_ - Unsupported BMP DIB header (size " +
                          std::to_string(dib_size) + ")");
    }

    if (size < 14 + dib_size) {
        HUIRA_THROW_ERROR("read_bmp_raw_ - Data truncated in DIB header");
    }

    const unsigned char* dib = &data[14];

    int32_t raw_width, raw_height;
    uint16_t bits_per_pixel;
    uint32_t compression;

    std::memcpy(&raw_width, &dib[4], sizeof(int32_t));
    std::memcpy(&raw_height, &dib[8], sizeof(int32_t));
    std::memcpy(&bits_per_pixel, &dib[14], sizeof(uint16_t));
    std::memcpy(&compression, &dib[16], sizeof(uint32_t));

    if (compression != BMP_BI_RGB && compression != BMP_BI_BITFIELDS &&
        compression != BMP_BI_ALPHABITFIELDS) {
        HUIRA_THROW_ERROR("read_bmp_raw_ - Unsupported BMP compression type (" +
                          std::to_string(compression) + ")");
    }

    if (bits_per_pixel != 24 && bits_per_pixel != 32) {
        HUIRA_THROW_ERROR("read_bmp_raw_ - Unsupported BMP bit depth (" +
                          std::to_string(static_cast<int>(bits_per_pixel)) +
                          "), only 24 and 32 supported");
    }

    int width = static_cast<int>(raw_width);
    bool flip_vertical = (raw_height > 0);
    int height = (raw_height > 0) ? static_cast<int>(raw_height) : static_cast<int>(-raw_height);

    // A 32-bit pixel's channels: by default, B, G, R and alpha bytes, in that order (a
    // little-endian 0xAARRGGBB).
    BMPChannelMask_ red(0x00FF0000u);
    BMPChannelMask_ green(0x0000FF00u);
    BMPChannelMask_ blue(0x000000FFu);
    BMPChannelMask_ alpha(0xFF000000u);
    if (bits_per_pixel == 32 && compression != BMP_BI_RGB) {
        // The masks are in the DIB header if it is long enough to hold them (52 bytes, or 56
        // with alpha's), and otherwise follow it:
        const bool in_header = dib_size >= 52;
        const std::size_t count = in_header ? (dib_size >= 56 ? 4u : 3u)
                                            : (compression == BMP_BI_ALPHABITFIELDS ? 4u : 3u);
        const std::size_t at = in_header ? 14 + 40 : 14 + static_cast<std::size_t>(dib_size);
        if (size < at + 4 * count) {
            HUIRA_THROW_ERROR("read_bmp_raw_ - Data truncated in color masks");
        }
        uint32_t masks[4] = {0, 0, 0, 0};
        std::memcpy(masks, &data[at], 4 * count);
        red = BMPChannelMask_(masks[0]);
        green = BMPChannelMask_(masks[1]);
        blue = BMPChannelMask_(masks[2]);
        alpha = BMPChannelMask_(masks[3]);
    }

    bool has_alpha = (bits_per_pixel == 32 && alpha.mask != 0);
    int file_channels = bits_per_pixel / 8;

    // BMP rows are padded to 4-byte boundaries
    std::size_t row_stride =
        (static_cast<std::size_t>(width * file_channels) + 3) & ~static_cast<std::size_t>(3);

    std::size_t required_size =
        static_cast<std::size_t>(pixel_offset) + row_stride * static_cast<std::size_t>(height);
    if (size < required_size) {
        HUIRA_THROW_ERROR("read_bmp_raw_ - Data truncated in pixel data (need " +
                          std::to_string(required_size) + ", have " + std::to_string(size) + ")");
    }

    const unsigned char* pixel_data = data + pixel_offset;

    const auto pixel_at = [&](int x, int y) {
        uint32_t pixel = 0;
        std::memcpy(&pixel,
                    pixel_data + static_cast<std::size_t>(y) * row_stride +
                        static_cast<std::size_t>(x) * 4,
                    sizeof(uint32_t));
        return pixel;
    };

    // Without masks, the fourth byte is alpha only if some pixel sets it:
    if (has_alpha && compression == BMP_BI_RGB) {
        has_alpha = false;
        for (int y = 0; y < height && !has_alpha; ++y) {
            for (int x = 0; x < width && !has_alpha; ++x) {
                has_alpha = (pixel_at(x, y) & alpha.mask) != 0;
            }
        }
    }
    const int out_channels = has_alpha ? 4 : 3;

    // Convert from BGR(A) bottom-up to RGB(A) top-down
    std::vector<unsigned char> raw_data(static_cast<std::size_t>(width) *
                                        static_cast<std::size_t>(height * out_channels));

    for (int y = 0; y < height; ++y) {
        int src_y = flip_vertical ? (height - 1 - y) : y;
        const unsigned char* src_row = pixel_data + static_cast<std::size_t>(src_y) * row_stride;

        for (int x = 0; x < width; ++x) {
            std::size_t src_idx = static_cast<std::size_t>(x * file_channels);
            std::size_t dst_idx =
                (static_cast<std::size_t>(y * width) + static_cast<std::size_t>(x)) *
                static_cast<std::size_t>(out_channels);

            if (bits_per_pixel == 32) {
                const uint32_t pixel = pixel_at(x, src_y);
                raw_data[dst_idx + 0] = red.value(pixel);
                raw_data[dst_idx + 1] = green.value(pixel);
                raw_data[dst_idx + 2] = blue.value(pixel);
                if (has_alpha) {
                    raw_data[dst_idx + 3] = alpha.value(pixel);
                }
            } else {
                // BMP stores BGR, convert to RGB
                raw_data[dst_idx + 0] = src_row[src_idx + 2]; // R
                raw_data[dst_idx + 1] = src_row[src_idx + 1]; // G
                raw_data[dst_idx + 2] = src_row[src_idx + 0]; // B
            }
        }
    }

    BMPData bmp_data{};
    bmp_data.resolution = Resolution{width, height};
    bmp_data.width = width;
    bmp_data.height = height;
    bmp_data.raw_data = std::move(raw_data);
    bmp_data.channels = out_channels;
    bmp_data.has_alpha = has_alpha;

    return bmp_data;
}

// =========================================================================
// RGB readers
// =========================================================================

/**
 * @brief Reads a BMP from an in-memory buffer and returns RGB + alpha data.
 *
 * @param data Pointer to the BMP data in memory
 * @param size Size of the data in bytes
 * @param read_alpha Whether to load the alpha channel if present (default: true)
 * @return An ImageBundle<RGB> containing the RGB image and an optional alpha image.
 */
inline ImageBundle<RGB> read_image_bmp(const unsigned char* data, std::size_t size, bool read_alpha)
{
    auto bmp_data = read_bmp_raw_(data, size);

    ImageBundle<RGB> bundle{Image<RGB>(bmp_data.resolution)};
    // Stored values, labelled with the encoding they have (nothing is converted):
    bundle.color_space = ColorSpaceHint::sRGB;

    bmp_data.has_alpha = read_alpha && bmp_data.has_alpha;

    if (bmp_data.has_alpha) {
        bundle.alpha = Image<float>(bmp_data.resolution, 1.0f);
    }

    for (int y = 0; y < bmp_data.height; ++y) {
        for (int x = 0; x < bmp_data.width; ++x) {
            std::size_t idx =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(bmp_data.width) +
                 static_cast<std::size_t>(x)) *
                static_cast<std::size_t>(bmp_data.channels);

            float r = integer_to_float<std::uint8_t>(bmp_data.raw_data[idx + 0]);
            float g = integer_to_float<std::uint8_t>(bmp_data.raw_data[idx + 1]);
            float b = integer_to_float<std::uint8_t>(bmp_data.raw_data[idx + 2]);

            bundle.image(x, y) = RGB{r, g, b};

            if (bmp_data.has_alpha) {
                bundle.alpha(x, y) = integer_to_float<std::uint8_t>(bmp_data.raw_data[idx + 3]);
            }
        }
    }

    return bundle;
}

/**
 * @brief Reads a BMP file and returns RGB + alpha data.
 *
 * Convenience overload that reads the file into memory and forwards
 * to the buffer-based implementation.
 *
 * @param filepath Path to the BMP file to read
 * @param read_alpha Whether to load the alpha channel if present (default: true)
 * @return An ImageBundle<RGB> containing the RGB image and an optional alpha image.
 */
inline ImageBundle<RGB> read_image_bmp(const fs::path& filepath, bool read_alpha)
{
    auto file_data = read_file_to_buffer(filepath);
    return read_image_bmp(file_data.data(), file_data.size(), read_alpha);
}

// =========================================================================
// Mono readers
// =========================================================================

/**
 * @brief Reads a BMP from an in-memory buffer and returns mono + alpha data.
 *
 * RGB channels are averaged to produce mono output.
 *
 * @param data Pointer to the BMP data in memory
 * @param size Size of the data in bytes
 * @param read_alpha Whether to load the alpha channel if present (default: true)
 * @return An ImageBundle<float> containing the mono image and an optional alpha image.
 */
inline ImageBundle<float>
read_image_bmp_mono(const unsigned char* data, std::size_t size, bool read_alpha)
{
    auto bmp_data = read_bmp_raw_(data, size);

    ImageBundle<float> bundle{Image<float>(bmp_data.resolution)};
    // Stored values, labelled with the encoding they have (nothing is converted):
    bundle.color_space = ColorSpaceHint::sRGB;

    bmp_data.has_alpha = read_alpha && bmp_data.has_alpha;

    if (bmp_data.has_alpha) {
        bundle.alpha = Image<float>(bmp_data.resolution, 1.0f);
    }

    for (int y = 0; y < bmp_data.height; ++y) {
        for (int x = 0; x < bmp_data.width; ++x) {
            std::size_t idx =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(bmp_data.width) +
                 static_cast<std::size_t>(x)) *
                static_cast<std::size_t>(bmp_data.channels);

            float r = integer_to_float<std::uint8_t>(bmp_data.raw_data[idx + 0]);
            float g = integer_to_float<std::uint8_t>(bmp_data.raw_data[idx + 1]);
            float b = integer_to_float<std::uint8_t>(bmp_data.raw_data[idx + 2]);

            bundle.image(x, y) = (r + g + b) / 3.0f;

            if (bmp_data.has_alpha) {
                bundle.alpha(x, y) = integer_to_float<std::uint8_t>(bmp_data.raw_data[idx + 3]);
            }
        }
    }

    return bundle;
}

/**
 * @brief Reads a BMP file and returns mono + alpha data.
 *
 * Convenience overload that reads the file into memory and forwards
 * to the buffer-based implementation.
 *
 * @param filepath Path to the BMP file to read
 * @param read_alpha Whether to load the alpha channel if present (default: true)
 * @return An ImageBundle<float> containing the mono image and an optional alpha image.
 */
inline ImageBundle<float> read_image_bmp_mono(const fs::path& filepath, bool read_alpha)
{
    auto file_data = read_file_to_buffer(filepath);
    return read_image_bmp_mono(file_data.data(), file_data.size(), read_alpha);
}

} // namespace huira
