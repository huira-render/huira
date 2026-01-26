#include <cstddef>
#include <cstdio>
#include <csetjmp>
#include <cstring>
#include <filesystem>
#include <utility>
#include <limits>
#include <vector>
#include <array>

#include <png.h>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/detail/logger.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/convert_pixel.hpp"
#include "huira/images/io/color_space.hpp"

namespace fs = std::filesystem;

namespace huira {

    enum class PngColorSpace {
        SRGB,        // sRGB chunk present, or assumed default
        LINEAR,      // gAMA = 1.0
        GAMMA,       // gAMA chunk with custom gamma
        ICC_PROFILE, // iCCP chunk present
        UNKNOWN      // Unknown or unsupported
    };

    struct PngColorInfo {
        PngColorSpace space = PngColorSpace::SRGB;
        double gamma = 2.2;  // Only used if space == GAMMA
        // TODO add ICC profile data
    };

    inline PngColorInfo detect_png_color_space_(png_structp png_ptr, png_infop info_ptr)
    {
        PngColorInfo info;

        // Check for ICC profile:
        png_charp name;
        int compression_type;
        png_bytep profile;
        png_uint_32 proflen;
        if (png_get_iCCP(png_ptr, info_ptr, &name, &compression_type, &profile, &proflen)) {
            info.space = PngColorSpace::ICC_PROFILE;
            return info;
        }

        // Check for sRGB chunk:
        int srgb_intent;
        if (png_get_sRGB(png_ptr, info_ptr, &srgb_intent)) {
            info.space = PngColorSpace::SRGB;
            return info;
        }

        // Check for gAMA chunk:
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

    template <IsInteger T>
    Vec3<float> linearize_png_pixel_(T encoded1, T encoded2, T encoded3, const PngColorInfo& color_info)
    {
        return Vec3<float>{
            linearize_png_pixel_<T>(encoded1, color_info),
                linearize_png_pixel_<T>(encoded2, color_info),
                linearize_png_pixel_<T>(encoded3, color_info)
        };
    }





    template <IsNonSpectralPixel T>
    std::pair<Image<T>, Image<float>> read_image_png(const fs::path& filepath)
    {
        HUIRA_LOG_INFO("read_image_png - Reading image from: " + filepath.string());
        // Cross-platform file opening
#ifdef _MSC_VER
        FILE* fp = nullptr;
        errno_t err = fopen_s(&fp, filepath.string().c_str(), "rb");
        if (err != 0 || !fp) {
            HUIRA_THROW_ERROR("Failed to open PNG file: " + filepath.string());
        }
#else
        FILE* fp = fopen(filepath.string().c_str(), "rb");
        if (!fp) {
            HUIRA_THROW_ERROR("Failed to open PNG file: " + filepath.string());
        }
#endif

        // Verify PNG signature
        std::array<png_byte, 8> sig{};
        if (fread(sig.data(), 1, 8, fp) != 8 ||
            png_sig_cmp(sig.data(), 0, 8) != 0) {
            fclose(fp);
            HUIRA_THROW_ERROR("File is not a valid PNG: " + filepath.string());
        }

        png_structp png_ptr = png_create_read_struct(
            PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png_ptr) {
            fclose(fp);
            HUIRA_THROW_ERROR("Failed to create PNG read struct");
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            png_destroy_read_struct(&png_ptr, nullptr, nullptr);
            fclose(fp);
            HUIRA_THROW_ERROR("Failed to create PNG info struct");
        }

        // libpng error handling via setjmp (required for C library)
        // Suppress MSVC warnings about setjmp/C++ interaction - this is safe because
        // no C++ objects with non-trivial destructors exist between setjmp and longjmp
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4611 5039)
#endif
        if (setjmp(png_jmpbuf(png_ptr))) {
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            fclose(fp);
            HUIRA_THROW_ERROR("Error during PNG read: " + filepath.string());
        }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

        png_init_io(png_ptr, fp);
        png_set_sig_bytes(png_ptr, 8);
        png_read_info(png_ptr, info_ptr);

        // Detect color space before any transformations
        PngColorInfo color_info = detect_png_color_space_(png_ptr, info_ptr);

        png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
        png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
        png_byte color_type = png_get_color_type(png_ptr, info_ptr);
        png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

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
        fclose(fp);

        // Create output images
        Image<T> image(static_cast<std::size_t>(width),
            static_cast<std::size_t>(height));
        Image<float> alpha_image(0, 0);

        if (has_alpha) {
            alpha_image = Image<float>(static_cast<std::size_t>(width),
                static_cast<std::size_t>(height),
                1.0f);
        }

        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {

                if (final_bit_depth == 16) {
                    // Safe 16-bit extraction without alignment issues
                    const png_byte* byte_ptr = raw_data.data() + y * row_bytes + x * channels * 2;

                    auto read_u16 = [](const png_byte* p) -> uint16_t {
                        uint16_t val;
                        std::memcpy(&val, p, sizeof(uint16_t));
                        return val;
                        };

                    if (is_gray) {
                        uint16_t gray = read_u16(byte_ptr);
                        float mono = linearize_png_pixel_<uint16_t>(gray, color_info);
                        image(x, y) = convert_float_to_pixel<T>(mono);
                    }
                    else {
                        uint16_t r = read_u16(byte_ptr);
                        uint16_t g = read_u16(byte_ptr + 2);
                        uint16_t b = read_u16(byte_ptr + 4);
                        Vec3<float> rgb = linearize_png_pixel_<uint16_t>(r, g, b, color_info);
                        image(x, y) = convert_vec3_to_pixel<T>(rgb);
                    }

                    if (has_alpha) {
                        std::size_t alpha_byte_offset = static_cast<std::size_t>(is_gray ? 2 : 6);
                        alpha_image(x, y) = static_cast<float>(read_u16(byte_ptr + alpha_byte_offset)) / 65535.0f;
                    }
                }
                else {
                    // 8-bit
                    const png_byte* ptr = raw_data.data() + y * row_bytes + x * channels;

                    if (is_gray) {
                        float mono = linearize_png_pixel_<uint8_t>(ptr[0], color_info);
                        image(x, y) = convert_float_to_pixel<T>(mono);
                    }
                    else {
                        Vec3<float> rgb = linearize_png_pixel_<uint8_t>(ptr[0], ptr[1], ptr[2], color_info);
                        image(x, y) = convert_vec3_to_pixel<T>(rgb);
                    }

                    if (has_alpha) {
                        std::size_t alpha_idx = static_cast<std::size_t>(is_gray ? 1 : 3);
                        alpha_image(x, y) = static_cast<float>(ptr[alpha_idx]) / 255.0f;
                    }
                }
            }
        }

        return { std::move(image), std::move(alpha_image) };
    }


    template <IsNonSpectralPixel T>
    void write_image_png(const fs::path& filepath, const Image<T>& image, int bit_depth)
    {
        Image<float> albedo(0, 0);
        write_image_png<T>(filepath, image, albedo, bit_depth);
    }

    template <IsNonSpectralPixel T>
    void write_image_png(const fs::path& filepath, const Image<T>& image, const Image<float>& alpha, int bit_depth)
    {
        HUIRA_LOG_INFO("write_image_png - Writing to: " + filepath.string());
        if (image.width() == 0 || image.height() == 0) {
            HUIRA_THROW_ERROR("write_image_png - Cannot write empty image to PNG: " + filepath.string());
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

        // Determine if source pixel type is grayscale or RGB
        constexpr bool is_gray = IsNumeric<T>;

        int color_type;
        unsigned int channels;
        if constexpr (is_gray) {
            color_type = has_alpha ? PNG_COLOR_TYPE_GRAY_ALPHA : PNG_COLOR_TYPE_GRAY;
            channels = has_alpha ? 2 : 1;
        }
        else {
            color_type = has_alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB;
            channels = has_alpha ? 4 : 3;
        }

        std::size_t width = image.width();
        std::size_t height = image.height();

        // Cross-platform file opening
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

        // Set image header
        png_set_IHDR(png_ptr, info_ptr,
            static_cast<png_uint_32>(width),
            static_cast<png_uint_32>(height),
            bit_depth,
            color_type,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT);

        // Mark as sRGB (we're converting from linear to sRGB on write)
        png_set_sRGB_gAMA_and_cHRM(png_ptr, info_ptr, PNG_sRGB_INTENT_PERCEPTUAL);

        png_write_info(png_ptr, info_ptr);

        // PNG stores 16-bit values in big-endian; swap bytes on little-endian systems
        if (bit_depth == 16) {
            png_set_swap(png_ptr);
        }

        // Write image data row by row
        if (bit_depth == 16) {
            std::vector<uint16_t> row_data(width * channels);

            for (std::size_t y = 0; y < height; ++y) {
                for (std::size_t x = 0; x < width; ++x) {
                    const T& pixel = image(x, y);

                    if constexpr (is_gray) {
                        float linear = convert_pixel_to_float<T>(pixel);
                        float srgb = linear_to_srgb(linear);
                        row_data[x * channels] = float_to_integer<uint16_t>(srgb);

                        if (has_alpha) {
                            row_data[x * channels + 1] = float_to_integer<uint16_t>(alpha(x, y));
                        }
                    }
                    else {
                        Vec3<float> linear = convert_pixel_to_vec3<T>(pixel);

                        row_data[x * channels + 0] = float_to_integer<uint16_t>(linear_to_srgb(linear.x));
                        row_data[x * channels + 1] = float_to_integer<uint16_t>(linear_to_srgb(linear.y));
                        row_data[x * channels + 2] = float_to_integer<uint16_t>(linear_to_srgb(linear.z));

                        if (has_alpha) {
                            row_data[x * channels + 3] = float_to_integer<uint16_t>(alpha(x, y));
                        }
                    }
                }

                png_bytep row_ptr = reinterpret_cast<png_bytep>(row_data.data());
                png_write_row(png_ptr, row_ptr);
            }
        }
        else {
            // 8-bit
            std::vector<uint8_t> row_data(width * channels);

            for (std::size_t y = 0; y < height; ++y) {
                for (std::size_t x = 0; x < width; ++x) {
                    const T& pixel = image(x, y);

                    if constexpr (is_gray) {
                        float linear = convert_pixel_to_float<T>(pixel);
                        float srgb = linear_to_srgb(linear);
                        row_data[x * channels] = float_to_integer<uint8_t>(srgb);

                        if (has_alpha) {
                            row_data[x * channels + 1] = float_to_integer<uint8_t>(alpha(x, y));
                        }
                    }
                    else {
                        Vec3<float> linear = convert_pixel_to_vec3<T>(pixel);

                        row_data[x * channels + 0] = float_to_integer<uint8_t>(linear_to_srgb(linear.x));
                        row_data[x * channels + 1] = float_to_integer<uint8_t>(linear_to_srgb(linear.y));
                        row_data[x * channels + 2] = float_to_integer<uint8_t>(linear_to_srgb(linear.z));

                        if (has_alpha) {
                            row_data[x * channels + 3] = float_to_integer<uint8_t>(alpha(x, y));
                        }
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
