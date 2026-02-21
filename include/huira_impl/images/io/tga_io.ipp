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
     * @brief Raw decoded TGA data before pixel interpretation.
     */
    struct TGAData {
        Resolution resolution{ 0, 0 };
        int width = 0;
        int height = 0;

        std::vector<unsigned char> raw_data;  ///< Decoded pixel data (RGB or RGBA, top-to-bottom)
        int channels = 3;                     ///< 3 for RGB, 4 for RGBA
        bool has_alpha = false;
        bool is_gray = false;
    };

    /**
     * @brief Decodes a TGA file into raw byte data.
     *
     * Reads the 18-byte TGA header, validates the image type, and extracts pixel data.
     * Supports the following TGA image types:
     *   - Type 1: Uncompressed color-mapped (palette)
     *   - Type 2: Uncompressed true-color (RGB/RGBA)
     *   - Type 3: Uncompressed grayscale
     *   - Type 9: RLE color-mapped
     *   - Type 10: RLE true-color
     *   - Type 11: RLE grayscale
     *
     * Handles top-origin and bottom-origin images via the image descriptor byte.
     * Output is always converted to RGB(A) in top-to-bottom order with BGR-to-RGB swizzle.
     *
     * @param filepath Path to the TGA file to read
     * @return Raw decoded data with resolution and pixel buffer
     * @throws std::runtime_error if the file cannot be opened or uses an unsupported format
     */
    inline TGAData read_tga_raw_(const fs::path& filepath)
    {
        HUIRA_LOG_INFO("read_tga_raw_ - Reading image from: " + filepath.string());

#ifdef _MSC_VER
        FILE* fp = nullptr;
        errno_t err = fopen_s(&fp, filepath.string().c_str(), "rb");
        if (err != 0 || !fp) {
            HUIRA_THROW_ERROR("read_tga_raw_ - Failed to open TGA file: " + filepath.string());
        }
#else
        FILE* fp = fopen(filepath.string().c_str(), "rb");
        if (!fp) {
            HUIRA_THROW_ERROR("read_tga_raw_ - Failed to open TGA file: " + filepath.string());
        }
#endif

        // Read 18-byte TGA header
        unsigned char header[18];
        if (fread(header, 1, 18, fp) != 18) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_tga_raw_ - Failed to read TGA header: " + filepath.string());
        }

        uint8_t id_length = header[0];
        uint8_t color_map_type = header[1];
        uint8_t image_type = header[2];

        // Color map spec (bytes 3-7)
        uint16_t cm_first_index;
        uint16_t cm_length;
        uint8_t cm_entry_size;
        std::memcpy(&cm_first_index, &header[3], sizeof(uint16_t));
        std::memcpy(&cm_length, &header[5], sizeof(uint16_t));
        cm_entry_size = header[7];

        // Image spec (bytes 8-17)
        uint16_t width_u16, height_u16;
        std::memcpy(&width_u16, &header[12], sizeof(uint16_t));
        std::memcpy(&height_u16, &header[14], sizeof(uint16_t));
        uint8_t pixel_depth = header[16];
        uint8_t descriptor = header[17];

        int width = static_cast<int>(width_u16);
        int height = static_cast<int>(height_u16);
        bool top_origin = (descriptor & 0x20) != 0;

        // Validate image type
        bool is_rle = (image_type == 9 || image_type == 10 || image_type == 11);
        bool is_color_mapped = (image_type == 1 || image_type == 9);
        bool is_gray = (image_type == 3 || image_type == 11);
        bool is_truecolor = (image_type == 2 || image_type == 10);

        if (!is_truecolor && !is_gray && !is_color_mapped) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_tga_raw_ - Unsupported TGA image type (" +
                std::to_string(image_type) + "): " + filepath.string());
        }

        // Skip image ID field
        if (id_length > 0) {
            fseek(fp, id_length, SEEK_CUR);
        }

        // Read color map if present
        std::vector<unsigned char> color_map;
        int cm_bytes_per_entry = 0;
        if (color_map_type == 1 && cm_length > 0) {
            cm_bytes_per_entry = (cm_entry_size + 7) / 8;
            color_map.resize(static_cast<std::size_t>(cm_length * cm_bytes_per_entry));
            if (fread(color_map.data(), 1, color_map.size(), fp) != color_map.size()) {
                fclose(fp);
                HUIRA_THROW_ERROR("read_tga_raw_ - Failed to read TGA color map: " + filepath.string());
            }
        }
        else if (color_map_type == 1) {
            // Skip color map data even if length is 0 but type says it exists
            // (shouldn't happen, but be safe)
        }

        // Determine pixel format
        int file_channels;
        if (is_color_mapped) {
            file_channels = cm_bytes_per_entry;
        }
        else if (is_gray) {
            file_channels = 1;
        }
        else {
            file_channels = pixel_depth / 8;
        }

        bool has_alpha = false;
        if (is_color_mapped) {
            has_alpha = (cm_entry_size == 32);
        }
        else if (is_gray) {
            has_alpha = false;
        }
        else {
            has_alpha = (pixel_depth == 32);
        }

        int out_channels = has_alpha ? 4 : 3;

        // Read pixel data (raw or RLE)
        std::size_t num_pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        std::size_t index_bytes = static_cast<std::size_t>(is_color_mapped ? (pixel_depth / 8) : file_channels);

        // For RLE or raw, we need to read the pixel/index data
        std::vector<unsigned char> pixel_indices;

        if (is_rle) {
            // RLE decode
            pixel_indices.resize(num_pixels * index_bytes);
            std::size_t pixels_decoded = 0;

            while (pixels_decoded < num_pixels) {
                uint8_t packet_header;
                if (fread(&packet_header, 1, 1, fp) != 1) {
                    fclose(fp);
                    HUIRA_THROW_ERROR("read_tga_raw_ - Unexpected end of RLE data: " + filepath.string());
                }

                int count = (packet_header & 0x7F) + 1;

                if (packet_header & 0x80) {
                    // Run-length packet: one pixel value repeated 'count' times
                    unsigned char pixel[4];
                    if (fread(pixel, 1, index_bytes, fp) != index_bytes) {
                        fclose(fp);
                        HUIRA_THROW_ERROR("read_tga_raw_ - Unexpected end of RLE data: " + filepath.string());
                    }

                    for (int i = 0; i < count && pixels_decoded < num_pixels; ++i) {
                        std::memcpy(&pixel_indices[pixels_decoded * index_bytes], pixel, index_bytes);
                        ++pixels_decoded;
                    }
                }
                else {
                    // Raw packet: 'count' literal pixel values
                    std::size_t bytes_to_read = static_cast<std::size_t>(count) * index_bytes;
                    if (fread(&pixel_indices[pixels_decoded * index_bytes], 1, bytes_to_read, fp) != bytes_to_read) {
                        fclose(fp);
                        HUIRA_THROW_ERROR("read_tga_raw_ - Unexpected end of RLE data: " + filepath.string());
                    }
                    pixels_decoded += static_cast<std::size_t>(count);
                }
            }
        }
        else {
            // Uncompressed: read all pixel data at once
            pixel_indices.resize(num_pixels * static_cast<std::size_t>(index_bytes));
            if (fread(pixel_indices.data(), 1, pixel_indices.size(), fp) != pixel_indices.size()) {
                fclose(fp);
                HUIRA_THROW_ERROR("read_tga_raw_ - Failed to read pixel data: " + filepath.string());
            }
        }

        fclose(fp);

        // Convert to RGB(A) top-to-bottom
        std::vector<unsigned char> raw_data(num_pixels * static_cast<std::size_t>(out_channels));

        for (int y = 0; y < height; ++y) {
            int src_y = top_origin ? y : (height - 1 - y);

            for (int x = 0; x < width; ++x) {
                std::size_t src_pixel = static_cast<std::size_t>(src_y) * static_cast<std::size_t>(width)
                    + static_cast<std::size_t>(x);
                std::size_t dst_idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                    + static_cast<std::size_t>(x)) * static_cast<std::size_t>(out_channels);

                const unsigned char* src_ptr;
                unsigned char resolved[4] = { 0, 0, 0, 255 };

                if (is_color_mapped) {
                    // Look up the color map index
                    uint16_t index = 0;
                    if (pixel_depth == 8) {
                        index = pixel_indices[src_pixel];
                    }
                    else if (pixel_depth == 16) {
                        std::memcpy(&index, &pixel_indices[src_pixel * 2], sizeof(uint16_t));
                    }
                    index = static_cast<uint16_t>(index - cm_first_index);
                    src_ptr = &color_map[static_cast<std::size_t>(index) * static_cast<std::size_t>(cm_bytes_per_entry)];

                    // Color map entries are BGR(A)
                    resolved[0] = src_ptr[2];  // R
                    resolved[1] = src_ptr[1];  // G
                    resolved[2] = src_ptr[0];  // B
                    if (cm_bytes_per_entry >= 4) {
                        resolved[3] = src_ptr[3];  // A
                    }
                }
                else if (is_gray) {
                    unsigned char gray = pixel_indices[src_pixel];
                    resolved[0] = gray;
                    resolved[1] = gray;
                    resolved[2] = gray;
                }
                else {
                    // True-color: BGR(A) -> RGB(A)
                    src_ptr = &pixel_indices[src_pixel * static_cast<std::size_t>(index_bytes)];
                    resolved[0] = src_ptr[2];  // R
                    resolved[1] = src_ptr[1];  // G
                    resolved[2] = src_ptr[0];  // B
                    if (file_channels >= 4) {
                        resolved[3] = src_ptr[3];  // A
                    }
                }

                raw_data[dst_idx + 0] = resolved[0];
                raw_data[dst_idx + 1] = resolved[1];
                raw_data[dst_idx + 2] = resolved[2];
                if (has_alpha) {
                    raw_data[dst_idx + 3] = resolved[3];
                }
            }
        }

        TGAData tga_data{};
        tga_data.resolution = Resolution{ width, height };
        tga_data.width = width;
        tga_data.height = height;
        tga_data.raw_data = std::move(raw_data);
        tga_data.channels = out_channels;
        tga_data.has_alpha = has_alpha;
        tga_data.is_gray = is_gray;

        return tga_data;
    }

    /**
     * @brief Reads a TGA image file and returns linear RGB + alpha data.
     *
     * Always returns an Image<RGB> in linear color space.
     * Grayscale TGAs are promoted to RGB (equal values in all channels).
     * Supports uncompressed and RLE-compressed TGA files with 8-bit grayscale,
     * 24-bit RGB, and 32-bit RGBA pixel formats.
     * If the TGA has an alpha channel, the second image contains it; otherwise
     * the second image is empty (0x0).
     *
     * TGA pixel data is assumed to be sRGB encoded and is linearized on load.
     *
     * @param filepath Path to the TGA file to read
     * @param read_alpha Whether to load the alpha channel if present (default: true)
     * @return A pair containing the linear RGB image and an optional alpha image.
     * @throws std::runtime_error if the file cannot be opened, is not a valid TGA,
     *         or uses an unsupported format
     */
    inline std::pair<Image<RGB>, Image<float>> read_image_tga(const fs::path& filepath, bool read_alpha)
    {
        auto tga_data = read_tga_raw_(filepath);

        Image<RGB> image(tga_data.resolution);
        Image<float> alpha_image(0, 0);

        tga_data.has_alpha = read_alpha && tga_data.has_alpha;

        if (tga_data.has_alpha) {
            alpha_image = Image<float>(tga_data.resolution, 1.0f);
        }

        for (int y = 0; y < tga_data.height; ++y) {
            for (int x = 0; x < tga_data.width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(tga_data.width)
                    + static_cast<std::size_t>(x)) * static_cast<std::size_t>(tga_data.channels);

                float r = srgb_to_linear(static_cast<float>(tga_data.raw_data[idx + 0]) / 255.0f);
                float g = srgb_to_linear(static_cast<float>(tga_data.raw_data[idx + 1]) / 255.0f);
                float b = srgb_to_linear(static_cast<float>(tga_data.raw_data[idx + 2]) / 255.0f);

                image(x, y) = RGB{ r, g, b };

                if (tga_data.has_alpha) {
                    alpha_image(x, y) = static_cast<float>(tga_data.raw_data[idx + 3]) / 255.0f;
                }
            }
        }

        return { std::move(image), std::move(alpha_image) };
    }

    /**
     * @brief Reads a TGA image file and returns linear mono + alpha data.
     *
     * RGB channels are averaged after linearization to produce mono output.
     *
     * @param filepath Path to the TGA file to read
     * @param read_alpha Whether to load the alpha channel if present (default: true)
     * @return A pair containing the linear mono image and an optional alpha image.
     * @throws std::runtime_error if the file cannot be opened, is not a valid TGA,
     *         or uses an unsupported format
     */
    inline std::pair<Image<float>, Image<float>> read_image_tga_mono(const fs::path& filepath, bool read_alpha)
    {
        auto tga_data = read_tga_raw_(filepath);

        Image<float> image(tga_data.resolution);
        Image<float> alpha_image(0, 0);

        tga_data.has_alpha = read_alpha && tga_data.has_alpha;

        if (tga_data.has_alpha) {
            alpha_image = Image<float>(tga_data.resolution, 1.0f);
        }

        for (int y = 0; y < tga_data.height; ++y) {
            for (int x = 0; x < tga_data.width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(tga_data.width)
                    + static_cast<std::size_t>(x)) * static_cast<std::size_t>(tga_data.channels);

                float r = srgb_to_linear(static_cast<float>(tga_data.raw_data[idx + 0]) / 255.0f);
                float g = srgb_to_linear(static_cast<float>(tga_data.raw_data[idx + 1]) / 255.0f);
                float b = srgb_to_linear(static_cast<float>(tga_data.raw_data[idx + 2]) / 255.0f);

                image(x, y) = (r + g + b) / 3.0f;

                if (tga_data.has_alpha) {
                    alpha_image(x, y) = static_cast<float>(tga_data.raw_data[idx + 3]) / 255.0f;
                }
            }
        }

        return { std::move(image), std::move(alpha_image) };
    }

}
