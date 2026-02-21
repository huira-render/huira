#include <array>
#include <csetjmp>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <utility>
#include <vector>
#include <tuple>

#include <png.h>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/color_space.hpp"
#include "huira/images/io/convert_pixel.hpp"
#include "huira/images/io/io_util.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/paths.hpp"

namespace fs = std::filesystem;

namespace huira {
    enum class PngColorSpace {
        SRGB,
        LINEAR,
        GAMMA,
        ICC_PROFILE,
        UNKNOWN
    };

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
            }
            else if (std::abs(file_gamma - 0.45455) < 0.01) {
                info.space = PngColorSpace::SRGB;
            }
            else {
                info.space = PngColorSpace::GAMMA;
                info.gamma = 1.0 / file_gamma;
            }
            return info;
        }

        info.space = PngColorSpace::SRGB;
        return info;
    }

    template <IsInteger T>
    float linearize_png_pixel_(T encoded, const PngColorInfo& color_info)
    {
        float encoded_f = integer_to_float<T>(encoded, 0.f, 1.f);

        switch (color_info.space) {
        case PngColorSpace::LINEAR:
            return encoded_f;
        case PngColorSpace::SRGB:
            return srgb_to_linear(encoded_f);
        case PngColorSpace::ICC_PROFILE:
            return srgb_to_linear(encoded_f);
        case PngColorSpace::UNKNOWN:
            return srgb_to_linear(encoded_f);
        case PngColorSpace::GAMMA:
            return gamma_to_linear(encoded_f, static_cast<float>(color_info.gamma));
        default:
            return srgb_to_linear(encoded_f);
        }
    }

    struct PNGData {
        Resolution resolution{ 0, 0 };
        png_uint_32 height;
        png_uint_32 width;
        unsigned int channels;

        std::vector<png_byte> raw_data;
        std::size_t row_bytes;
        PngColorInfo color_info;

        png_byte final_bit_depth = 8;

        bool has_alpha = false;
        bool is_gray = false;
    };

    /**
     * @brief State for libpng custom memory read callback.
     */
    struct PngMemReadState_ {
        const unsigned char* data;
        std::size_t size;
        std::size_t pos;
    };

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
    inline void png_mem_read_callback_(png_structp png_ptr, png_bytep out_bytes, png_size_t byte_count) noexcept
    {
        auto* state = reinterpret_cast<PngMemReadState_*>(png_get_io_ptr(png_ptr));
        if (state->pos + byte_count > state->size) {
            png_error(png_ptr, "Read past end of PNG memory buffer");
            return;
        }
        std::memcpy(out_bytes, state->data + state->pos, byte_count);
        state->pos += byte_count;
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
            PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png_ptr) {
            HUIRA_THROW_ERROR("read_png_raw_ - Failed to create PNG read struct");
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            png_destroy_read_struct(&png_ptr, nullptr, nullptr);
            HUIRA_THROW_ERROR("read_png_raw_ - Failed to create PNG info struct");
        }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4611 5039)
#endif
        if (setjmp(png_jmpbuf(png_ptr))) {
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            HUIRA_THROW_ERROR("read_png_raw_ - Error during PNG read");
        }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

        // Set up custom memory read
        PngMemReadState_ read_state{ data, size, 8 };  // Skip past signature
        png_set_read_fn(png_ptr, &read_state, png_mem_read_callback_);
        png_set_sig_bytes(png_ptr, 8);

        png_read_info(png_ptr, info_ptr);

        // Detect color space before any transformations
        PngColorInfo color_info = detect_png_color_space_(png_ptr, info_ptr);

        png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
        png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
        png_byte color_type = png_get_color_type(png_ptr, info_ptr);
        png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

        Resolution resolution{ static_cast<int>(width), static_cast<int>(height) };

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

        bool is_gray = (final_color_type == PNG_COLOR_TYPE_GRAY ||
            final_color_type == PNG_COLOR_TYPE_GRAY_ALPHA);
        bool has_alpha = (final_color_type & PNG_COLOR_MASK_ALPHA) != 0;

        unsigned int channels = 0;
        if (final_color_type == PNG_COLOR_TYPE_GRAY) channels = 1;
        else if (final_color_type == PNG_COLOR_TYPE_GRAY_ALPHA) channels = 2;
        else if (final_color_type == PNG_COLOR_TYPE_RGB) channels = 3;
        else if (final_color_type == PNG_COLOR_TYPE_RGB_ALPHA) channels = 4;

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
        png_data.has_alpha = has_alpha;
        png_data.is_gray = is_gray;

        return png_data;
    }

    // =========================================================================
    // RGB readers
    // =========================================================================

    /**
     * @brief Reads a PNG from an in-memory buffer and returns linear RGB + alpha data.
     *
     * @param data Pointer to the PNG data in memory
     * @param size Size of the data in bytes
     * @param read_alpha Whether to load the alpha channel if present (default: true)
     * @return A pair containing the linear RGB image and an optional alpha image.
     */
    inline std::pair<Image<RGB>, Image<float>> read_image_png(const unsigned char* data, std::size_t size, bool read_alpha)
    {
        auto png_data = read_png_raw_(data, size);

        Image<RGB> image(png_data.resolution);
        Image<float> alpha_image(0, 0);

        png_data.has_alpha = read_alpha && png_data.has_alpha;

        if (png_data.has_alpha) {
            alpha_image = Image<float>(png_data.resolution, 1.0f);
        }

        for (int y = 0; y < static_cast<int>(png_data.height); ++y) {
            for (int x = 0; x < static_cast<int>(png_data.width); ++x) {
                std::size_t x_u = static_cast<std::size_t>(x);
                std::size_t y_u = static_cast<std::size_t>(y);

                if (png_data.final_bit_depth == 16) {
                    const png_byte* byte_ptr = png_data.raw_data.data() + y_u * png_data.row_bytes + x_u * png_data.channels * 2;

                    auto read_u16 = [](const png_byte* p) -> uint16_t {
                        uint16_t val;
                        std::memcpy(&val, p, sizeof(uint16_t));
                        return val;
                    };

                    if (png_data.is_gray) {
                        float mono = linearize_png_pixel_<uint16_t>(read_u16(byte_ptr), png_data.color_info);
                        image(x, y) = RGB{ mono, mono, mono };
                    }
                    else {
                        float r = linearize_png_pixel_<uint16_t>(read_u16(byte_ptr), png_data.color_info);
                        float g = linearize_png_pixel_<uint16_t>(read_u16(byte_ptr + 2), png_data.color_info);
                        float b = linearize_png_pixel_<uint16_t>(read_u16(byte_ptr + 4), png_data.color_info);
                        image(x, y) = RGB{ r, g, b };
                    }

                    if (png_data.has_alpha) {
                        std::size_t alpha_byte_offset = static_cast<std::size_t>(png_data.is_gray ? 2 : 6);
                        alpha_image(x, y) = static_cast<float>(read_u16(byte_ptr + alpha_byte_offset)) / 65535.0f;
                    }
                }
                else {
                    const png_byte* ptr = png_data.raw_data.data() + y_u * png_data.row_bytes + x_u * png_data.channels;

                    if (png_data.is_gray) {
                        float mono = linearize_png_pixel_<uint8_t>(ptr[0], png_data.color_info);
                        image(x, y) = RGB{ mono, mono, mono };
                    }
                    else {
                        float r = linearize_png_pixel_<uint8_t>(ptr[0], png_data.color_info);
                        float g = linearize_png_pixel_<uint8_t>(ptr[1], png_data.color_info);
                        float b = linearize_png_pixel_<uint8_t>(ptr[2], png_data.color_info);
                        image(x, y) = RGB{ r, g, b };
                    }

                    if (png_data.has_alpha) {
                        std::size_t alpha_idx = static_cast<std::size_t>(png_data.is_gray ? 1 : 3);
                        alpha_image(x, y) = static_cast<float>(ptr[alpha_idx]) / 255.0f;
                    }
                }
            }
        }

        return { std::move(image), std::move(alpha_image) };
    }

    /**
     * @brief Reads a PNG file and returns linear RGB + alpha data.
     *
     * Convenience overload that reads the file into memory and forwards
     * to the buffer-based implementation.
     */
    inline std::pair<Image<RGB>, Image<float>> read_image_png(const fs::path& filepath, bool read_alpha)
    {
        auto file_data = read_file_to_buffer(filepath);
        return read_image_png(file_data.data(), file_data.size(), read_alpha);
    }

    // =========================================================================
    // Mono readers
    // =========================================================================

    /**
     * @brief Reads a PNG from an in-memory buffer and returns linear mono + alpha data.
     *
     * @param data Pointer to the PNG data in memory
     * @param size Size of the data in bytes
     * @param read_alpha Whether to load the alpha channel if present (default: true)
     * @return A pair containing the linear mono image and an optional alpha image.
     */
    inline std::pair<Image<float>, Image<float>> read_image_png_mono(const unsigned char* data, std::size_t size, bool read_alpha)
    {
        auto png_data = read_png_raw_(data, size);

        Image<float> image(png_data.resolution);
        Image<float> alpha_image(0, 0);

        png_data.has_alpha = read_alpha && png_data.has_alpha;

        if (png_data.has_alpha) {
            alpha_image = Image<float>(png_data.resolution, 1.0f);
        }

        for (int y = 0; y < static_cast<int>(png_data.height); ++y) {
            for (int x = 0; x < static_cast<int>(png_data.width); ++x) {
                std::size_t x_u = static_cast<std::size_t>(x);
                std::size_t y_u = static_cast<std::size_t>(y);

                if (png_data.final_bit_depth == 16) {
                    const png_byte* byte_ptr = png_data.raw_data.data() + y_u * png_data.row_bytes + x_u * png_data.channels * 2;

                    auto read_u16 = [](const png_byte* p) -> uint16_t {
                        uint16_t val;
                        std::memcpy(&val, p, sizeof(uint16_t));
                        return val;
                    };

                    if (png_data.is_gray) {
                        float mono = linearize_png_pixel_<uint16_t>(read_u16(byte_ptr), png_data.color_info);
                        image(x, y) = mono;
                    }
                    else {
                        float r = linearize_png_pixel_<uint16_t>(read_u16(byte_ptr), png_data.color_info);
                        float g = linearize_png_pixel_<uint16_t>(read_u16(byte_ptr + 2), png_data.color_info);
                        float b = linearize_png_pixel_<uint16_t>(read_u16(byte_ptr + 4), png_data.color_info);
                        image(x, y) = (r + g + b) / 3.f;
                    }

                    if (png_data.has_alpha) {
                        std::size_t alpha_byte_offset = static_cast<std::size_t>(png_data.is_gray ? 2 : 6);
                        alpha_image(x, y) = static_cast<float>(read_u16(byte_ptr + alpha_byte_offset)) / 65535.0f;
                    }
                }
                else {
                    const png_byte* ptr = png_data.raw_data.data() + y_u * png_data.row_bytes + x_u * png_data.channels;

                    if (png_data.is_gray) {
                        float mono = linearize_png_pixel_<uint8_t>(ptr[0], png_data.color_info);
                        image(x, y) = mono;
                    }
                    else {
                        float r = linearize_png_pixel_<uint8_t>(ptr[0], png_data.color_info);
                        float g = linearize_png_pixel_<uint8_t>(ptr[1], png_data.color_info);
                        float b = linearize_png_pixel_<uint8_t>(ptr[2], png_data.color_info);
                        image(x, y) = (r + g + b) / 3.f;
                    }

                    if (png_data.has_alpha) {
                        std::size_t alpha_idx = static_cast<std::size_t>(png_data.is_gray ? 1 : 3);
                        alpha_image(x, y) = static_cast<float>(ptr[alpha_idx]) / 255.0f;
                    }
                }
            }
        }

        return { std::move(image), std::move(alpha_image) };
    }

    /**
     * @brief Reads a PNG file and returns linear mono + alpha data.
     *
     * Convenience overload that reads the file into memory and forwards
     * to the buffer-based implementation.
     */
    inline std::pair<Image<float>, Image<float>> read_image_png_mono(const fs::path& filepath, bool read_alpha)
    {
        auto file_data = read_file_to_buffer(filepath);
        return read_image_png_mono(file_data.data(), file_data.size(), read_alpha);
    }

    // =========================================================================
    // Writers (file-only, unchanged)
    // =========================================================================

    inline void write_image_png(const fs::path& filepath, const Image<RGB>& image, int bit_depth)
    {
        Image<float> alpha(0, 0);
        write_image_png(filepath, image, alpha, bit_depth);
    }

    inline void write_image_png(const fs::path& filepath, const Image<float>& image, int bit_depth)
    {
        Image<float> alpha(0, 0);

        Image<RGB> image_rgb(image.width(), image.height());
        for (std::size_t i = 0; i < image.size(); ++i) {
            image_rgb[i] = RGB{ image[i], image[i], image[i] };
        }

        write_image_png(filepath, image_rgb, alpha, bit_depth);
    }

    inline void write_image_png(const fs::path& filepath, const Image<float>& image,
        const Image<float>& alpha, int bit_depth)
    {
        Image<RGB> image_rgb(image.width(), image.height());
        for (std::size_t i = 0; i < image.size(); ++i) {
            image_rgb[i] = RGB{ image[i], image[i], image[i] };
        }

        write_image_png(filepath, image_rgb, alpha, bit_depth);
    }

    inline void write_image_png(const fs::path& filepath, const Image<RGB>& image,
        const Image<float>& alpha, int bit_depth)
    {
        HUIRA_LOG_INFO("write_image_png - Writing to: " + filepath.string());
        if (image.width() == 0 || image.height() == 0) {
            HUIRA_THROW_ERROR("write_image_png - Cannot write empty image: " + filepath.string());
        }

        bool has_alpha = (alpha.width() != 0 && alpha.height() != 0);
        if (has_alpha) {
            if (alpha.width() != image.width() || alpha.height() != image.height()) {
                HUIRA_LOG_WARNING("write_image_png - image is [" +
                    std::to_string(image.width()) + " x " + std::to_string(image.height()) + "], but alpha is [" +
                    std::to_string(alpha.width()) + " x " + std::to_string(alpha.height()) + "]. The alpha mask will be ignored.");
                has_alpha = false;
            }
        }

        if (bit_depth != 8 && bit_depth != 16) {
            HUIRA_THROW_ERROR("write_image_png - bit_depth must be 8 or 16, got: " + std::to_string(bit_depth));
        }

        make_path(filepath);

        int color_type = has_alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB;
        unsigned int channels = has_alpha ? 4 : 3;

        int width = image.width();
        int height = image.height();

#ifdef _MSC_VER
        FILE* fp = nullptr;
        errno_t err = fopen_s(&fp, filepath.string().c_str(), "wb");
        if (err != 0 || !fp) {
            HUIRA_THROW_ERROR("write_image_png - Failed to open PNG file for writing: " + filepath.string());
        }
#else
        FILE* fp = fopen(filepath.string().c_str(), "wb");
        if (!fp) {
            HUIRA_THROW_ERROR("write_image_png - Failed to open PNG file for writing: " + filepath.string());
        }
#endif

        png_structp png_ptr = png_create_write_struct(
            PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4611 5039)
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

        png_set_IHDR(png_ptr, info_ptr,
            static_cast<png_uint_32>(width),
            static_cast<png_uint_32>(height),
            bit_depth,
            color_type,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);

        png_set_sRGB_gAMA_and_cHRM(png_ptr, info_ptr, PNG_sRGB_INTENT_PERCEPTUAL);
        png_write_info(png_ptr, info_ptr);

        if (bit_depth == 16) {
            png_set_swap(png_ptr);
        }

        if (bit_depth == 16) {
            std::vector<uint16_t> row_data(static_cast<std::size_t>(width) * channels);

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    std::size_t x_u = static_cast<std::size_t>(x);
                    const RGB& pixel = image(x, y);

                    row_data[x_u * channels + 0] = float_to_integer<uint16_t>(linear_to_srgb(pixel[0]));
                    row_data[x_u * channels + 1] = float_to_integer<uint16_t>(linear_to_srgb(pixel[1]));
                    row_data[x_u * channels + 2] = float_to_integer<uint16_t>(linear_to_srgb(pixel[2]));

                    if (has_alpha) {
                        row_data[x_u * channels + 3] = float_to_integer<uint16_t>(alpha(x, y));
                    }
                }

                png_bytep row_ptr = reinterpret_cast<png_bytep>(row_data.data());
                png_write_row(png_ptr, row_ptr);
            }
        }
        else {
            std::vector<uint8_t> row_data(static_cast<std::size_t>(width) * channels);

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    std::size_t x_u = static_cast<std::size_t>(x);
                    const RGB& pixel = image(x, y);

                    row_data[x_u * channels + 0] = float_to_integer<uint8_t>(linear_to_srgb(pixel[0]));
                    row_data[x_u * channels + 1] = float_to_integer<uint8_t>(linear_to_srgb(pixel[1]));
                    row_data[x_u * channels + 2] = float_to_integer<uint8_t>(linear_to_srgb(pixel[2]));

                    if (has_alpha) {
                        row_data[x_u * channels + 3] = float_to_integer<uint8_t>(alpha(x, y));
                    }
                }

                png_bytep row_ptr = row_data.data();
                png_write_row(png_ptr, row_ptr);
            }
        }

        png_write_end(png_ptr, nullptr);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
    }

}
