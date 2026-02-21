#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <utility>
#include <vector>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/color_space.hpp"
#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {

    /**
     * @brief Raw decoded BMP data before pixel interpretation.
     */
    struct BMPData {
        Resolution resolution{ 0, 0 };
        int width = 0;
        int height = 0;

        std::vector<unsigned char> raw_data;  ///< Decoded pixel data (RGB or RGBA, top-to-bottom)
        int channels = 3;                     ///< 3 for RGB, 4 for RGBA
        bool has_alpha = false;
    };

    /**
     * @brief Decodes a BMP file into raw byte data.
     *
     * Reads the BMP file header and DIB header, validates the format, and extracts
     * pixel data. Handles bottom-up row ordering (standard for BMP) by flipping
     * rows during extraction. Supports 24-bit RGB and 32-bit RGBA uncompressed formats.
     *
     * @param filepath Path to the BMP file to read
     * @return Raw decoded data with resolution and pixel buffer
     * @throws std::runtime_error if the file cannot be opened or uses an unsupported format
     */
    inline BMPData read_bmp_raw_(const fs::path& filepath)
    {
        HUIRA_LOG_INFO("read_bmp_raw_ - Reading image from: " + filepath.string());

#ifdef _MSC_VER
        FILE* fp = nullptr;
        errno_t err = fopen_s(&fp, filepath.string().c_str(), "rb");
        if (err != 0 || !fp) {
            HUIRA_THROW_ERROR("read_bmp_raw_ - Failed to open BMP file: " + filepath.string());
        }
#else
        FILE* fp = fopen(filepath.string().c_str(), "rb");
        if (!fp) {
            HUIRA_THROW_ERROR("read_bmp_raw_ - Failed to open BMP file: " + filepath.string());
        }
#endif

        // Read BMP file header (14 bytes)
        unsigned char file_header[14];
        if (fread(file_header, 1, 14, fp) != 14) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_bmp_raw_ - Failed to read BMP file header: " + filepath.string());
        }

        // Validate signature
        if (file_header[0] != 'B' || file_header[1] != 'M') {
            fclose(fp);
            HUIRA_THROW_ERROR("read_bmp_raw_ - File is not a valid BMP: " + filepath.string());
        }

        uint32_t pixel_offset;
        std::memcpy(&pixel_offset, &file_header[10], sizeof(uint32_t));

        // Read DIB header (at least 40 bytes for BITMAPINFOHEADER)
        unsigned char dib_header[124];  // Large enough for BITMAPV5HEADER
        if (fread(dib_header, 1, 4, fp) != 4) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_bmp_raw_ - Failed to read DIB header size: " + filepath.string());
        }

        uint32_t dib_size;
        std::memcpy(&dib_size, &dib_header[0], sizeof(uint32_t));

        if (dib_size < 40) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_bmp_raw_ - Unsupported BMP DIB header (size " +
                std::to_string(dib_size) + "): " + filepath.string());
        }

        // Read the rest of the DIB header
        std::size_t remaining = static_cast<std::size_t>(dib_size) - 4;
        if (remaining > sizeof(dib_header) - 4) {
            remaining = sizeof(dib_header) - 4;
        }
        if (fread(&dib_header[4], 1, remaining, fp) != remaining) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_bmp_raw_ - Failed to read DIB header: " + filepath.string());
        }

        int32_t raw_width, raw_height;
        uint16_t bits_per_pixel;
        uint32_t compression;

        std::memcpy(&raw_width, &dib_header[4], sizeof(int32_t));
        std::memcpy(&raw_height, &dib_header[8], sizeof(int32_t));
        std::memcpy(&bits_per_pixel, &dib_header[14], sizeof(uint16_t));
        std::memcpy(&compression, &dib_header[16], sizeof(uint32_t));

        // We support BI_RGB (0) and BI_BITFIELDS (3) for 32-bit
        if (compression != 0 && compression != 3) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_bmp_raw_ - Unsupported BMP compression type (" +
                std::to_string(compression) + "): " + filepath.string());
        }

        if (bits_per_pixel != 24 && bits_per_pixel != 32) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_bmp_raw_ - Unsupported BMP bit depth (" +
                std::to_string(bits_per_pixel) + "), only 24 and 32 supported: " + filepath.string());
        }

        int width = static_cast<int>(raw_width);
        bool flip_vertical = (raw_height > 0);  // Positive height = bottom-up
        int height = (raw_height > 0) ? static_cast<int>(raw_height) : static_cast<int>(-raw_height);

        bool has_alpha = (bits_per_pixel == 32);
        int out_channels = has_alpha ? 4 : 3;
        int file_channels = bits_per_pixel / 8;

        // BMP rows are padded to 4-byte boundaries
        std::size_t row_stride = (static_cast<std::size_t>(width * file_channels) + 3) & ~static_cast<std::size_t>(3);

        // Seek to pixel data
        fseek(fp, static_cast<long>(pixel_offset), SEEK_SET);

        // Read raw pixel data
        std::vector<unsigned char> file_pixels(row_stride * static_cast<std::size_t>(height));
        if (fread(file_pixels.data(), 1, file_pixels.size(), fp) != file_pixels.size()) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_bmp_raw_ - Failed to read pixel data: " + filepath.string());
        }
        fclose(fp);

        // Convert from BGR(A) bottom-up to RGB(A) top-down
        std::vector<unsigned char> raw_data(static_cast<std::size_t>(width) * static_cast<std::size_t>(height * out_channels));

        for (int y = 0; y < height; ++y) {
            int src_y = flip_vertical ? (height - 1 - y) : y;
            const unsigned char* src_row = file_pixels.data() + static_cast<std::size_t>(src_y) * row_stride;

            for (int x = 0; x < width; ++x) {
                std::size_t src_idx = static_cast<std::size_t>(x * file_channels);
                std::size_t dst_idx = (static_cast<std::size_t>(y * width)
                    + static_cast<std::size_t>(x)) * static_cast<std::size_t>(out_channels);

                // BMP stores BGR(A), convert to RGB(A)
                raw_data[dst_idx + 0] = src_row[src_idx + 2];  // R
                raw_data[dst_idx + 1] = src_row[src_idx + 1];  // G
                raw_data[dst_idx + 2] = src_row[src_idx + 0];  // B

                if (has_alpha) {
                    raw_data[dst_idx + 3] = src_row[src_idx + 3];  // A
                }
            }
        }

        BMPData bmp_data{};
        bmp_data.resolution = Resolution{ width, height };
        bmp_data.width = width;
        bmp_data.height = height;
        bmp_data.raw_data = std::move(raw_data);
        bmp_data.channels = out_channels;
        bmp_data.has_alpha = has_alpha;

        return bmp_data;
    }

    /**
     * @brief Reads a BMP image file and returns linear RGB + alpha data.
     *
     * Always returns an Image<RGB> in linear color space.
     * Supports 24-bit (RGB) and 32-bit (RGBA) uncompressed BMPs.
     * If the BMP has an alpha channel, the second image contains it; otherwise
     * the second image is empty (0x0).
     *
     * BMP pixel data is assumed to be sRGB encoded and is linearized on load.
     *
     * @param filepath Path to the BMP file to read
     * @param read_alpha Whether to load the alpha channel if present (default: true)
     * @return A pair containing the linear RGB image and an optional alpha image.
     * @throws std::runtime_error if the file cannot be opened, is not a valid BMP,
     *         or uses an unsupported format (e.g. compressed or palettized)
     */
    inline std::pair<Image<RGB>, Image<float>> read_image_bmp(const fs::path& filepath, bool read_alpha)
    {
        auto bmp_data = read_bmp_raw_(filepath);

        Image<RGB> image(bmp_data.resolution);
        Image<float> alpha_image(0, 0);

        bmp_data.has_alpha = read_alpha && bmp_data.has_alpha;

        if (bmp_data.has_alpha) {
            alpha_image = Image<float>(bmp_data.resolution, 1.0f);
        }

        for (int y = 0; y < bmp_data.height; ++y) {
            for (int x = 0; x < bmp_data.width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(bmp_data.width)
                    + static_cast<std::size_t>(x)) * static_cast<std::size_t>(bmp_data.channels);

                float r = srgb_to_linear(static_cast<float>(bmp_data.raw_data[idx + 0]) / 255.0f);
                float g = srgb_to_linear(static_cast<float>(bmp_data.raw_data[idx + 1]) / 255.0f);
                float b = srgb_to_linear(static_cast<float>(bmp_data.raw_data[idx + 2]) / 255.0f);

                image(x, y) = RGB{ r, g, b };

                if (bmp_data.has_alpha) {
                    alpha_image(x, y) = static_cast<float>(bmp_data.raw_data[idx + 3]) / 255.0f;
                }
            }
        }

        return { std::move(image), std::move(alpha_image) };
    }

    /**
     * @brief Reads a BMP image file and returns linear mono data.
     *
     * RGB channels are averaged after linearization to produce mono output.
     *
     * @param filepath Path to the BMP file to read
     * @return A pair containing the linear mono image and an optional alpha image.
     * @throws std::runtime_error if the file cannot be opened, is not a valid BMP,
     *         or uses an unsupported format
     */
    inline std::pair<Image<float>, Image<float>> read_image_bmp_mono(const fs::path& filepath, bool read_alpha)
    {
        auto bmp_data = read_bmp_raw_(filepath);

        Image<float> image(bmp_data.resolution);
        Image<float> alpha_image(0, 0);

        bmp_data.has_alpha = read_alpha && bmp_data.has_alpha;

        if (bmp_data.has_alpha) {
            alpha_image = Image<float>(bmp_data.resolution, 1.0f);
        }

        for (int y = 0; y < bmp_data.height; ++y) {
            for (int x = 0; x < bmp_data.width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(bmp_data.width)
                    + static_cast<std::size_t>(x)) * static_cast<std::size_t>(bmp_data.channels);

                float r = srgb_to_linear(static_cast<float>(bmp_data.raw_data[idx + 0]) / 255.0f);
                float g = srgb_to_linear(static_cast<float>(bmp_data.raw_data[idx + 1]) / 255.0f);
                float b = srgb_to_linear(static_cast<float>(bmp_data.raw_data[idx + 2]) / 255.0f);

                image(x, y) = (r + g + b) / 3.0f;

                if (bmp_data.has_alpha) {
                    alpha_image(x, y) = static_cast<float>(bmp_data.raw_data[idx + 3]) / 255.0f;
                }
            }
        }

        return { std::move(image), std::move(alpha_image) };
    }

}
