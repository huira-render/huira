#include <array>
#include <csetjmp>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#include <png.h>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/color_space.hpp"
#include "huira/images/io/convert_pixel.hpp"
#include "huira/images/io/io_util.hpp"
#include "huira/images/io/pixel_encoding.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/paths.hpp"

namespace fs = std::filesystem;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4611 5039)
#endif

namespace huira {
enum class PngColorSpace { SRGB, LINEAR, GAMMA, ICC_PROFILE, UNKNOWN };

struct PngColorInfo {
    PngColorSpace space = PngColorSpace::SRGB;
    double gamma = 2.2;
};

inline PngColorInfo detect_png_color_space_(png_structp png_ptr, png_infop info_ptr)
{
    PngColorInfo info;

    png_charp name;
    int compression_type;
    png_bytep profile;
    png_uint_32 proflen;
    if (png_get_iCCP(png_ptr, info_ptr, &name, &compression_type, &profile, &proflen)) {
        info.space = PngColorSpace::ICC_PROFILE;
        return info;
    }

    int srgb_intent;
    if (png_get_sRGB(png_ptr, info_ptr, &srgb_intent)) {
        info.space = PngColorSpace::SRGB;
        return info;
    }

    double file_gamma;
    if (png_get_gAMA(png_ptr, info_ptr, &file_gamma)) {
        if (std::abs(file_gamma - 1.0) < 0.01) {
            info.space = PngColorSpace::LINEAR;
        } else if (std::abs(file_gamma - 0.45455) < 0.01) {
            info.space = PngColorSpace::SRGB;
        } else {
            info.space = PngColorSpace::GAMMA;
            info.gamma = 1.0 / file_gamma;
        }
        return info;
    }

    info.space = PngColorSpace::SRGB;
    return info;
}

struct PNGData {
    Resolution resolution{0, 0};
    png_uint_32 height;
    png_uint_32 width;
    unsigned int channels;

    std::vector<png_byte> raw_data;
    std::size_t row_bytes;
    PngColorInfo color_info;

    png_byte final_bit_depth = 8;

    /// The sBIT chunk's significant bits for the color (or gray) samples, when it gives one
    /// number for all of them; 0 without it. A sensor's DN, scaled up to the file's bits by
    /// left-bit replication (see write_image_png()), are the samples' top bits.
    int significant_bits = 0;

    bool has_alpha = false;
    bool is_gray = false;
};

/// A sample as a value in [0, 1]: with significant bits, the DN they hold over the largest DN
/// (as a sensor response holds them); otherwise over the file's range.
inline float png_sample_to_float_(std::uint32_t sample, int file_bits, int significant_bits)
{
    if (significant_bits > 0 && significant_bits <= file_bits) {
        const std::uint32_t dn = sample >> (file_bits - significant_bits);
        return static_cast<float>(static_cast<double>(dn) /
                                  static_cast<double>((std::uint64_t{1} << significant_bits) - 1));
    }
    return static_cast<float>(static_cast<double>(sample) /
                              static_cast<double>((std::uint64_t{1} << file_bits) - 1));
}

/**
 * @brief State for libpng custom memory read callback.
 */
struct PngMemReadState_ {
    const unsigned char* data;
    std::size_t size;
    std::size_t pos;
};

extern "C" {
/**
 * @brief Custom libpng read callback that reads from a memory buffer.
 *
 * Registered via png_set_read_fn to replace file-based I/O. libpng calls
 * this function whenever it needs to read bytes from the data source.
 *
 * @param png_ptr PNG read structure (io_ptr points to PngMemReadState_)
 * @param out_bytes Destination buffer
 * @param byte_count Number of bytes to read
 */
static void png_mem_read_callback_(png_structp png_ptr, png_bytep out_bytes, png_size_t byte_count)
{
    auto* state = reinterpret_cast<PngMemReadState_*>(png_get_io_ptr(png_ptr));
    if (state->pos + byte_count > state->size) {
        png_error(png_ptr, "Read past end of PNG memory buffer");
        return;
    }
    std::memcpy(out_bytes, state->data + state->pos, byte_count);
    state->pos += byte_count;
}

static void png_warning_handler_(png_structp /*png_ptr*/, png_const_charp message)
{
    HUIRA_LOG_DEBUG(std::string("libpng warning: ") + message);
}

[[noreturn]] static void png_error_handler_(png_structp png_ptr, png_const_charp message)
{
    HUIRA_LOG_ERROR(std::string("libpng error: ") + message);
    longjmp(png_jmpbuf(png_ptr), 1);
}
}

/**
 * @brief Decodes a PNG from an in-memory buffer into raw byte data.
 *
 * Uses libpng with a custom read callback to decompress from memory.
 * Handles palette expansion, low bit depth expansion, tRNS alpha expansion,
 * and byte order normalization for 16-bit images.
 *
 * @param data Pointer to the PNG data in memory
 * @param size Size of the data in bytes
 * @return Raw decoded data with resolution, color info, and pixel buffer
 * @throws std::runtime_error if the data is not a valid or supported PNG
 */
inline PNGData read_png_raw_(const unsigned char* data, std::size_t size)
{
    HUIRA_LOG_INFO("read_png_raw_ - Reading PNG from memory (" + std::to_string(size) + " bytes)");

    if (size < 8) {
        HUIRA_THROW_ERROR("read_png_raw_ - Data too small to be a valid PNG");
    }

    // Verify PNG signature
    if (png_sig_cmp(data, 0, 8) != 0) {
        HUIRA_THROW_ERROR("read_png_raw_ - Data is not a valid PNG (bad signature)");
    }

    png_structp png_ptr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, nullptr, png_error_handler_, png_warning_handler_);
    if (!png_ptr) {
        HUIRA_THROW_ERROR("read_png_raw_ - Failed to create PNG read struct");
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        HUIRA_THROW_ERROR("read_png_raw_ - Failed to create PNG info struct");
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        HUIRA_THROW_ERROR("read_png_raw_ - Error during PNG read");
    }

    // Set up custom memory read
    PngMemReadState_ read_state{data, size, 8}; // Skip past signature
    png_set_read_fn(png_ptr, &read_state, png_mem_read_callback_);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    // Detect color space before any transformations
    PngColorInfo color_info = detect_png_color_space_(png_ptr, info_ptr);

    png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
    png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    Resolution resolution{static_cast<int>(width), static_cast<int>(height)};

    // Significant bits (sBIT), for samples of 8 or 16 bits that are not palette indices:
    int significant_bits = 0;
    png_color_8p sig_bit = nullptr;
    if (bit_depth >= 8 && color_type != PNG_COLOR_TYPE_PALETTE &&
        png_get_sBIT(png_ptr, info_ptr, &sig_bit) != 0 && sig_bit != nullptr) {
        const bool gray = (color_type & PNG_COLOR_MASK_COLOR) == 0;
        const int bits = gray ? sig_bit->gray : sig_bit->red;
        const bool uniform =
            gray || (sig_bit->green == sig_bit->red && sig_bit->blue == sig_bit->red);
        if (uniform && bits > 0 && bits <= bit_depth) {
            significant_bits = bits;
        }
    }

    // Expand palette to RGB(A)
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }

    // Expand low bit depth grayscale to 8-bit
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }

    // Expand tRNS to full alpha channel
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }

    // For 16-bit, ensure native byte order
    if (bit_depth == 16) {
        png_set_swap(png_ptr);
    }

    png_read_update_info(png_ptr, info_ptr);

    // Get final format after transformations
    png_byte final_color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte final_bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    bool is_gray =
        (final_color_type == PNG_COLOR_TYPE_GRAY || final_color_type == PNG_COLOR_TYPE_GRAY_ALPHA);
    bool has_alpha = (final_color_type & PNG_COLOR_MASK_ALPHA) != 0;

    unsigned int channels = 0;
    if (final_color_type == PNG_COLOR_TYPE_GRAY) {
        channels = 1;
    } else if (final_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        channels = 2;
    } else if (final_color_type == PNG_COLOR_TYPE_RGB) {
        channels = 3;
    } else if (final_color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
        channels = 4;
    }

    std::size_t row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    std::vector<png_byte> raw_data(row_bytes * height);
    std::vector<png_bytep> row_pointers(height);

    for (png_uint_32 y = 0; y < height; ++y) {
        row_pointers[y] = raw_data.data() + y * row_bytes;
    }

    png_read_image(png_ptr, row_pointers.data());
    png_read_end(png_ptr, nullptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

    PNGData png_data{};
    png_data.resolution = resolution;
    png_data.height = height;
    png_data.width = width;
    png_data.channels = channels;
    png_data.raw_data = std::move(raw_data);
    png_data.row_bytes = row_bytes;
    png_data.color_info = color_info;
    png_data.final_bit_depth = final_bit_depth;
    png_data.significant_bits = significant_bits;
    png_data.has_alpha = has_alpha;
    png_data.is_gray = is_gray;

    return png_data;
}

template <IsImagePixel PixelT>
void read_color_space(PngColorInfo color_info, ImageBundle<PixelT>& bundle)
{
    switch (color_info.space) {
    case PngColorSpace::LINEAR:
        bundle.color_space = ColorSpaceHint::Linear;
        break;

    case PngColorSpace::SRGB:
        bundle.color_space = ColorSpaceHint::sRGB;
        break;

    case PngColorSpace::ICC_PROFILE:
        bundle.color_space = ColorSpaceHint::Unknown;
        break;

    case PngColorSpace::UNKNOWN:
        bundle.color_space = ColorSpaceHint::Unknown;
        break;

    case PngColorSpace::GAMMA:
        bundle.color_space = ColorSpaceHint::Gamma;
        bundle.gamma_value = static_cast<float>(color_info.gamma);
        break;

    default:
        bundle.color_space = ColorSpaceHint::Unknown;
        break;
    }
}

// =========================================================================
// RGB readers
// =========================================================================

/**
 * @brief Reads a PNG from an in-memory buffer and returns RGB + alpha data.
 *
 * @param data Pointer to the PNG data in memory
 * @param size Size of the data in bytes
 * @param read_alpha Whether to load the alpha channel if present (default: true)
 * @return An ImageBundle<RGB> containing the RGB image and an optional alpha image.
 */
inline ImageBundle<RGB> read_image_png(const unsigned char* data, std::size_t size, bool read_alpha)
{
    auto png_data = read_png_raw_(data, size);

    ImageBundle<RGB> bundle{Image<RGB>(png_data.resolution)};

    read_color_space(png_data.color_info, bundle);
    bundle.image.set_sensor_bit_depth(png_data.significant_bits);
    bundle.bit_depth = png_data.final_bit_depth;

    png_data.has_alpha = read_alpha && png_data.has_alpha;
    if (png_data.has_alpha) {
        bundle.alpha = Image<float>(png_data.resolution, 1.0f);
    }

    for (int y = 0; y < static_cast<int>(png_data.height); ++y) {
        for (int x = 0; x < static_cast<int>(png_data.width); ++x) {
            std::size_t x_u = static_cast<std::size_t>(x);
            std::size_t y_u = static_cast<std::size_t>(y);

            if (png_data.final_bit_depth == 16) {
                const png_byte* byte_ptr = png_data.raw_data.data() + y_u * png_data.row_bytes +
                                           x_u * png_data.channels * 2;

                const int sbits = png_data.significant_bits;
                auto read_u16 = [sbits](const png_byte* p) -> float {
                    std::uint16_t val;
                    std::memcpy(&val, p, sizeof(std::uint16_t));
                    return png_sample_to_float_(val, 16, sbits);
                };
                auto read_alpha_u16 = [](const png_byte* p) -> float {
                    std::uint16_t val;
                    std::memcpy(&val, p, sizeof(std::uint16_t));
                    return integer_to_float<std::uint16_t>(val);
                };

                if (png_data.is_gray) {
                    float mono = read_u16(byte_ptr);
                    bundle.image(x, y) = RGB{mono, mono, mono};
                } else {
                    float r = read_u16(byte_ptr);
                    float g = read_u16(byte_ptr + 2);
                    float b = read_u16(byte_ptr + 4);
                    bundle.image(x, y) = RGB{r, g, b};
                }

                if (png_data.has_alpha) {
                    std::size_t alpha_byte_offset =
                        static_cast<std::size_t>(png_data.is_gray ? 2 : 6);
                    bundle.alpha(x, y) = read_alpha_u16(byte_ptr + alpha_byte_offset);
                }
            } else {
                const png_byte* ptr =
                    png_data.raw_data.data() + y_u * png_data.row_bytes + x_u * png_data.channels;

                if (png_data.is_gray) {
                    float mono = png_sample_to_float_(ptr[0], 8, png_data.significant_bits);
                    bundle.image(x, y) = RGB{mono, mono, mono};
                } else {
                    float r = png_sample_to_float_(ptr[0], 8, png_data.significant_bits);
                    float g = png_sample_to_float_(ptr[1], 8, png_data.significant_bits);
                    float b = png_sample_to_float_(ptr[2], 8, png_data.significant_bits);
                    bundle.image(x, y) = RGB{r, g, b};
                }

                if (png_data.has_alpha) {
                    std::size_t alpha_idx = static_cast<std::size_t>(png_data.is_gray ? 1 : 3);
                    bundle.alpha(x, y) = integer_to_float<std::uint8_t>(ptr[alpha_idx]);
                }
            }
        }
    }

    return bundle;
}

/**
 * @brief Reads a PNG file and returns RGB + alpha data.
 *
 * Convenience overload that reads the file into memory and forwards
 * to the buffer-based implementation.
 */
inline ImageBundle<RGB> read_image_png(const fs::path& filepath, bool read_alpha)
{
    auto file_data = read_file_to_buffer(filepath);
    return read_image_png(file_data.data(), file_data.size(), read_alpha);
}

// =========================================================================
// Mono readers
// =========================================================================

/**
 * @brief Reads a PNG from an in-memory buffer and returns mono + alpha data.
 *
 * @param data Pointer to the PNG data in memory
 * @param size Size of the data in bytes
 * @param read_alpha Whether to load the alpha channel if present (default: true)
 * @return An ImageBundle<float> containing the mono image and an optional alpha image.
 */
inline ImageBundle<float>
read_image_png_mono(const unsigned char* data, std::size_t size, bool read_alpha)
{
    auto png_data = read_png_raw_(data, size);

    ImageBundle<float> bundle{Image<float>(png_data.resolution)};

    read_color_space(png_data.color_info, bundle);
    bundle.image.set_sensor_bit_depth(png_data.significant_bits);
    bundle.bit_depth = png_data.final_bit_depth;

    png_data.has_alpha = read_alpha && png_data.has_alpha;

    if (png_data.has_alpha) {
        bundle.alpha = Image<float>(png_data.resolution, 1.0f);
    }

    for (int y = 0; y < static_cast<int>(png_data.height); ++y) {
        for (int x = 0; x < static_cast<int>(png_data.width); ++x) {
            std::size_t x_u = static_cast<std::size_t>(x);
            std::size_t y_u = static_cast<std::size_t>(y);

            if (png_data.final_bit_depth == 16) {
                const png_byte* byte_ptr = png_data.raw_data.data() + y_u * png_data.row_bytes +
                                           x_u * png_data.channels * 2;

                const int sbits = png_data.significant_bits;
                auto read_u16 = [sbits](const png_byte* p) -> float {
                    std::uint16_t val;
                    std::memcpy(&val, p, sizeof(std::uint16_t));
                    return png_sample_to_float_(val, 16, sbits);
                };
                auto read_alpha_u16 = [](const png_byte* p) -> float {
                    std::uint16_t val;
                    std::memcpy(&val, p, sizeof(std::uint16_t));
                    return integer_to_float<std::uint16_t>(val);
                };

                if (png_data.is_gray) {
                    float mono = read_u16(byte_ptr);
                    bundle.image(x, y) = mono;
                } else {
                    float r = read_u16(byte_ptr);
                    float g = read_u16(byte_ptr + 2);
                    float b = read_u16(byte_ptr + 4);
                    bundle.image(x, y) = (r + g + b) / 3.f;
                }

                if (png_data.has_alpha) {
                    std::size_t alpha_byte_offset =
                        static_cast<std::size_t>(png_data.is_gray ? 2 : 6);
                    bundle.alpha(x, y) = read_alpha_u16(byte_ptr + alpha_byte_offset);
                }
            } else {
                const png_byte* ptr =
                    png_data.raw_data.data() + y_u * png_data.row_bytes + x_u * png_data.channels;

                if (png_data.is_gray) {
                    float mono = png_sample_to_float_(ptr[0], 8, png_data.significant_bits);
                    bundle.image(x, y) = mono;
                } else {
                    float r = png_sample_to_float_(ptr[0], 8, png_data.significant_bits);
                    float g = png_sample_to_float_(ptr[1], 8, png_data.significant_bits);
                    float b = png_sample_to_float_(ptr[2], 8, png_data.significant_bits);
                    bundle.image(x, y) = (r + g + b) / 3.f;
                }

                if (png_data.has_alpha) {
                    std::size_t alpha_idx = static_cast<std::size_t>(png_data.is_gray ? 1 : 3);
                    bundle.alpha(x, y) = integer_to_float<std::uint8_t>(ptr[alpha_idx]);
                }
            }
        }
    }

    return bundle;
}

/**
 * @brief Reads a PNG file and returns mono + alpha data.
 *
 * Convenience overload that reads the file into memory and forwards
 * to the buffer-based implementation.
 */
inline ImageBundle<float> read_image_png_mono(const fs::path& filepath, bool read_alpha)
{
    auto file_data = read_file_to_buffer(filepath);
    return read_image_png_mono(file_data.data(), file_data.size(), read_alpha);
}

// =========================================================================
// Writers
// =========================================================================

/**
 * @brief Writes an image of color_channels samples per pixel (1, gray, or 3, RGB), plus alpha
 * if the bundle has it. sample(x, y, c) gives channel c of pixel (x, y).
 *
 * Values: see write_image_png().
 */
template <typename TBundle, typename TSample>
void write_png_impl_(const fs::path& filepath,
                     const TBundle& output_image,
                     unsigned int color_channels,
                     TSample&& sample)
{
    HUIRA_LOG_INFO("write_image_png - Writing to: " + filepath.string());
    if (output_image.image.width() == 0 || output_image.image.height() == 0) {
        HUIRA_THROW_ERROR("write_image_png - Cannot write empty image: " + filepath.string());
    }

    bool has_alpha = (output_image.alpha.width() != 0 && output_image.alpha.height() != 0);
    if (has_alpha) {
        if (output_image.alpha.width() != output_image.image.width() ||
            output_image.alpha.height() != output_image.image.height()) {
            HUIRA_LOG_WARNING(
                "write_image_png - image is [" + std::to_string(output_image.image.width()) +
                " x " + std::to_string(output_image.image.height()) + "], but alpha is [" +
                std::to_string(output_image.alpha.width()) + " x " +
                std::to_string(output_image.alpha.height()) + "]. The alpha mask will be ignored.");
            has_alpha = false;
        }
    }

    if (output_image.bit_depth != 8 && output_image.bit_depth != 16) {
        HUIRA_THROW_ERROR("write_image_png - bit_depth must be 8 or 16, got: " +
                          std::to_string(output_image.bit_depth));
    }

    const detail::IntegerEncoding encoding(
        output_image.bit_depth, output_image.image.sensor_bit_depth(), output_image.scaling);
    const detail::IntegerEncoding alpha_encoding(
        output_image.bit_depth, 0, PixelScaling::FullRange);
    detail::NonFiniteCounter non_finite;

    // A sensor's DN with no more bits than the file are scaled up by left-bit replication, and
    // sBIT records how many bits they have; with more, the lowest bits are dropped.
    auto encode = [&](float value) -> std::uint32_t {
        if (non_finite.not_finite(value)) {
            return 0;
        }
        if (!encoding.counts()) {
            return static_cast<std::uint32_t>(encoding.full_range(value));
        }
        const std::uint64_t dn = encoding.dn(value);
        if (encoding.shift > 0) {
            return static_cast<std::uint32_t>(dn >> encoding.shift);
        }
        return static_cast<std::uint32_t>(
            detail::replicate_bits(dn, encoding.sensor_bits, encoding.file_bits));
    };
    auto encode_alpha = [&](float value) -> std::uint32_t {
        return std::isfinite(value) ? static_cast<std::uint32_t>(alpha_encoding.full_range(value))
                                    : 0;
    };

    make_path(filepath);

    const bool gray = (color_channels == 1);
    int color_type = gray ? (has_alpha ? PNG_COLOR_TYPE_GRAY_ALPHA : PNG_COLOR_TYPE_GRAY)
                          : (has_alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB);
    const unsigned int channels = color_channels + (has_alpha ? 1u : 0u);

    int width = output_image.image.width();
    int height = output_image.image.height();

#ifdef _MSC_VER
    FILE* fp = nullptr;
    errno_t err = fopen_s(&fp, filepath.string().c_str(), "wb");
    if (err != 0 || !fp) {
        HUIRA_THROW_ERROR("write_image_png - Failed to open PNG file for writing: " +
                          filepath.string());
    }
#else
    FILE* fp = fopen(filepath.string().c_str(), "wb");
    if (!fp) {
        HUIRA_THROW_ERROR("write_image_png - Failed to open PNG file for writing: " +
                          filepath.string());
    }
#endif

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        fclose(fp);
        HUIRA_THROW_ERROR("write_image_png - Failed to create PNG write struct");
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(fp);
        HUIRA_THROW_ERROR("write_image_png - Failed to create PNG info struct");
    }

    // Row buffers are allocated before setjmp, so that a longjmp out of libpng leaks nothing.
    std::vector<std::uint16_t> row16(static_cast<std::size_t>(width) * channels);
    std::vector<std::uint8_t> row8(static_cast<std::size_t>(width) * channels);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4611 5039)
#endif
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        HUIRA_THROW_ERROR("write_image_png - Error during PNG write: " + filepath.string());
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr,
                 info_ptr,
                 static_cast<png_uint_32>(width),
                 static_cast<png_uint_32>(height),
                 output_image.bit_depth,
                 color_type,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    // Label the encoding the values have (nothing is converted):
    switch (output_image.color_space) {
    case ColorSpaceHint::sRGB:
        png_set_sRGB(png_ptr, info_ptr, PNG_sRGB_INTENT_PERCEPTUAL);
        break;

    case ColorSpaceHint::Linear:
        png_set_gAMA(png_ptr, info_ptr, 1.0);
        break;

    case ColorSpaceHint::Gamma:
        png_set_gAMA(png_ptr, info_ptr, 1.0 / output_image.gamma_value);
        break;

    case ColorSpaceHint::Unknown:
    default:
        break;
    }

    // A sensor's bit depth, when it fits in the file's:
    if (encoding.counts() && encoding.shift == 0) {
        png_color_8 sig_bit{};
        const auto bits = static_cast<png_byte>(encoding.sensor_bits);
        sig_bit.red = sig_bit.green = sig_bit.blue = sig_bit.gray = bits;
        sig_bit.alpha = static_cast<png_byte>(output_image.bit_depth);
        png_set_sBIT(png_ptr, info_ptr, &sig_bit);
    }

    png_write_info(png_ptr, info_ptr);

    if (output_image.bit_depth == 16) {
        png_set_swap(png_ptr);
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t base = static_cast<std::size_t>(x) * channels;
            for (unsigned int c = 0; c < color_channels; ++c) {
                const std::uint32_t value = encode(sample(x, y, c));
                row16[base + c] = static_cast<std::uint16_t>(value);
                row8[base + c] = static_cast<std::uint8_t>(value);
            }
            if (has_alpha) {
                const std::uint32_t value = encode_alpha(output_image.alpha(x, y));
                row16[base + color_channels] = static_cast<std::uint16_t>(value);
                row8[base + color_channels] = static_cast<std::uint8_t>(value);
            }
        }
        png_bytep row_ptr = (output_image.bit_depth == 16)
                                ? reinterpret_cast<png_bytep>(row16.data())
                                : reinterpret_cast<png_bytep>(row8.data());
        png_write_row(png_ptr, row_ptr);
    }

    png_write_end(png_ptr, nullptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    non_finite.warn("write_image_png", filepath.string());
}

/**
 * @brief Writes a gray image (with optional alpha) to a PNG file, of 8 or 16 bits per sample
 * (bundle.bit_depth).
 *
 * Values are written as they are, labelled with the bundle's color_space (nothing is converted;
 * see linear_to_srgb()). An image with a sensor bit depth, such as a sensor response, is written
 * as the sensor's digital numbers (DN) unless bundle.scaling is PixelScaling::FullRange: scaled
 * up to the file's bits by left-bit replication, as the PNG specification recommends, with an
 * sBIT chunk giving the sensor's bit depth, so that DN = value >> (file bits - sensor bits)
 * exactly (read_image_png() does this); or, for a sensor with more bits than the file, with the
 * lowest bits dropped. Other images are stretched over the file's range: 1 becomes 255 or 65535.
 * Values that are not finite are written as 0, with a warning.
 */
inline void write_image_png(const fs::path& filepath, const ImageBundle<float>& output_image)
{
    write_png_impl_(filepath, output_image, 1, [&](int x, int y, unsigned int /*c*/) {
        return output_image.image(x, y);
    });
}

/**
 * @brief Writes an RGB image (with optional alpha) to a PNG file. See the gray overload.
 */
inline void write_image_png(const fs::path& filepath, const ImageBundle<RGB>& output_image)
{
    write_png_impl_(filepath, output_image, 3, [&](int x, int y, unsigned int c) {
        return output_image.image(x, y)[c];
    });
}

} // namespace huira

#ifdef _MSC_VER
#pragma warning(pop)
#endif
