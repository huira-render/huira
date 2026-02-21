#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {

    /**
     * @brief Raw decoded HDR data before pixel interpretation.
     */
    struct HDRData {
        Resolution resolution{ 0, 0 };
        int width = 0;
        int height = 0;

        std::vector<float> raw_data;  ///< Decoded pixel data (RGB float, 3 floats per pixel)
    };

    /**
     * @brief Converts an RGBE (Radiance) pixel to linear RGB floats.
     *
     * RGBE stores RGB values with a shared exponent: the first three bytes are
     * the mantissas for R, G, B, and the fourth byte is a shared exponent.
     * The formula is: channel = (mantissa + 0.5) / 256.0 * 2^(exponent - 128)
     *
     * @param rgbe Pointer to 4 bytes of RGBE data
     * @param r Output red channel
     * @param g Output green channel
     * @param b Output blue channel
     */
    inline void rgbe_to_float_(const unsigned char* rgbe, float& r, float& g, float& b)
    {
        if (rgbe[3] == 0) {
            r = g = b = 0.0f;
        }
        else {
            float scale = std::ldexp(1.0f, static_cast<int>(rgbe[3]) - (128 + 8));
            r = static_cast<float>(rgbe[0]) * scale;
            g = static_cast<float>(rgbe[1]) * scale;
            b = static_cast<float>(rgbe[2]) * scale;
        }
    }

    /**
     * @brief Reads a single line of text from a FILE, handling \\n and \\r\\n.
     *
     * @param fp File pointer
     * @param line Output string (without line terminator)
     * @return true if a line was read, false on EOF
     */
    inline bool read_hdr_line_(FILE* fp, std::string& line)
    {
        line.clear();
        int c;
        while ((c = fgetc(fp)) != EOF) {
            if (c == '\n') return true;
            if (c == '\r') {
                // Consume optional \n after \r
                int next = fgetc(fp);
                if (next != '\n' && next != EOF) {
                    ungetc(next, fp);
                }
                return true;
            }
            line += static_cast<char>(c);
        }
        return !line.empty();
    }

    /**
     * @brief Decodes a Radiance HDR file into raw float RGB data.
     *
     * Parses the Radiance header to extract resolution, then decodes the RGBE
     * pixel data. Supports both uncompressed RGBE scanlines and new-style
     * adaptive run-length encoding (RLE) where each channel is encoded separately.
     *
     * @param filepath Path to the HDR file to read
     * @return Raw decoded data with resolution and float RGB buffer
     * @throws std::runtime_error if the file cannot be opened or decoded
     */
    inline HDRData read_hdr_raw_(const fs::path& filepath)
    {
        HUIRA_LOG_INFO("read_hdr_raw_ - Reading image from: " + filepath.string());

#ifdef _MSC_VER
        FILE* fp = nullptr;
        errno_t err = fopen_s(&fp, filepath.string().c_str(), "rb");
        if (err != 0 || !fp) {
            HUIRA_THROW_ERROR("read_hdr_raw_ - Failed to open HDR file: " + filepath.string());
        }
#else
        FILE* fp = fopen(filepath.string().c_str(), "rb");
        if (!fp) {
            HUIRA_THROW_ERROR("read_hdr_raw_ - Failed to open HDR file: " + filepath.string());
        }
#endif

        // Parse header: look for magic number and resolution string
        std::string line;

        // First line should be "#?RADIANCE" or "#?RGBE"
        if (!read_hdr_line_(fp, line)) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_hdr_raw_ - Failed to read HDR header: " + filepath.string());
        }

        if (line.substr(0, 2) != "#?") {
            fclose(fp);
            HUIRA_THROW_ERROR("read_hdr_raw_ - File is not a valid Radiance HDR: " + filepath.string());
        }

        // Read header lines until empty line
        bool found_format = false;
        while (read_hdr_line_(fp, line)) {
            if (line.empty()) break;

            if (line.find("FORMAT=32-bit_rle_rgbe") != std::string::npos ||
                line.find("FORMAT=32-bit_rle_xyze") != std::string::npos) {
                found_format = true;
            }
        }

        if (!found_format) {
            HUIRA_LOG_WARNING("read_hdr_raw_ - No FORMAT line found in HDR header, assuming RGBE: " + filepath.string());
        }

        // Parse resolution string: "-Y height +X width" is the most common
        if (!read_hdr_line_(fp, line)) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_hdr_raw_ - Failed to read resolution string: " + filepath.string());
        }
        int width = 0;
        int height = 0;

        auto parse_resolution = [&](const std::string& prefix_y, const std::string& prefix_x) -> bool {
            // Expected format: "<prefix_y> <height> <prefix_x> <width>"
            if (line.find(prefix_y) != 0) return false;
            std::istringstream iss(line.substr(prefix_y.size()));
            int h = 0;
            std::string px;
            int w = 0;
            if ((iss >> h >> px >> w) && px == prefix_x) {
                height = h;
                width = w;
                return true;
            }
            return false;
            };

        if (parse_resolution("-Y", "+X"))
        {
            /* Standard top-to-bottom, left-to-right */
        }
        else if (parse_resolution("+Y", "+X"))
        {
            /* Bottom-to-top variant */
        }
        else if (parse_resolution("-Y", "-X"))
        {
            /* Right-to-left variant */
        }
        else if (parse_resolution("+Y", "-X"))
        {
            /* Both flipped */
        }
        else {
            fclose(fp);
            HUIRA_THROW_ERROR("read_hdr_raw_ - Unsupported resolution format: \"" + line + "\": " + filepath.string());
        }

        if (width <= 0 || height <= 0) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_hdr_raw_ - Invalid dimensions (" +
                std::to_string(width) + " x " + std::to_string(height) + "): " + filepath.string());
        }

        // Decode scanlines
        std::size_t num_pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        std::vector<float> raw_data(num_pixels * 3);

        std::vector<unsigned char> scanline(static_cast<std::size_t>(width) * 4);

        for (int y = 0; y < height; ++y) {
            // Read first 4 bytes to determine encoding
            unsigned char header[4];
            if (fread(header, 1, 4, fp) != 4) {
                fclose(fp);
                HUIRA_THROW_ERROR("read_hdr_raw_ - Unexpected end of data at scanline " +
                    std::to_string(y) + ": " + filepath.string());
            }

            bool is_new_rle = (header[0] == 2 && header[1] == 2 &&
                ((static_cast<int>(header[2]) << 8) | header[3]) == width);

            if (is_new_rle) {
                // New-style adaptive RLE: each of the 4 channels is encoded separately
                for (int ch = 0; ch < 4; ++ch) {
                    int pos = 0;
                    while (pos < width) {
                        unsigned char byte;
                        if (fread(&byte, 1, 1, fp) != 1) {
                            fclose(fp);
                            HUIRA_THROW_ERROR("read_hdr_raw_ - Unexpected end of RLE data: " + filepath.string());
                        }

                        if (byte > 128) {
                            // Run: repeat next byte (byte - 128) times
                            int count = byte - 128;
                            unsigned char val;
                            if (fread(&val, 1, 1, fp) != 1) {
                                fclose(fp);
                                HUIRA_THROW_ERROR("read_hdr_raw_ - Unexpected end of RLE data: " + filepath.string());
                            }
                            for (int i = 0; i < count && pos < width; ++i) {
                                std::size_t index = static_cast<std::size_t>(pos) * 4 + static_cast<std::size_t>(ch);
                                scanline[index] = val;
                                ++pos;
                            }
                        }
                        else {
                            // Literal: read 'byte' values
                            int count = byte;
                            for (int i = 0; i < count && pos < width; ++i) {
                                unsigned char val;
                                if (fread(&val, 1, 1, fp) != 1) {
                                    fclose(fp);
                                    HUIRA_THROW_ERROR("read_hdr_raw_ - Unexpected end of RLE data: " + filepath.string());
                                }
                                std::size_t index = static_cast<std::size_t>(pos) * 4 + static_cast<std::size_t>(ch);
                                scanline[index] = val;
                                ++pos;
                            }
                        }
                    }
                }
            }
            else {
                // Old-style: first 4 bytes are already the first RGBE pixel
                scanline[0] = header[0];
                scanline[1] = header[1];
                scanline[2] = header[2];
                scanline[3] = header[3];

                // Read remaining pixels for this scanline
                if (width > 1) {
                    std::size_t remaining = static_cast<std::size_t>(width - 1) * 4;
                    if (fread(&scanline[4], 1, remaining, fp) != remaining) {
                        fclose(fp);
                        HUIRA_THROW_ERROR("read_hdr_raw_ - Unexpected end of data: " + filepath.string());
                    }
                }
            }

            // Convert RGBE scanline to float RGB
            for (int x = 0; x < width; ++x) {
                std::size_t src = static_cast<std::size_t>(x) * 4;
                std::size_t dst = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                    + static_cast<std::size_t>(x)) * 3;

                rgbe_to_float_(&scanline[src], raw_data[dst], raw_data[dst + 1], raw_data[dst + 2]);
            }
        }

        fclose(fp);

        HDRData hdr_data{};
        hdr_data.resolution = Resolution{ width, height };
        hdr_data.width = width;
        hdr_data.height = height;
        hdr_data.raw_data = std::move(raw_data);

        return hdr_data;
    }

    /**
     * @brief Reads a Radiance HDR (.hdr) image file and returns linear RGB data.
     *
     * Always returns an Image<RGB> in linear color space. HDR files natively
     * store linear radiance values using RGBE encoding, so no gamma conversion
     * is needed. HDR does not support alpha channels.
     *
     * Supports both uncompressed and new-style run-length encoded (RLE) HDR files.
     *
     * @param filepath Path to the HDR file to read
     * @return The linear RGB image.
     * @throws std::runtime_error if the file cannot be opened, is not a valid HDR,
     *         or if any error occurs during reading
     */
    inline Image<RGB> read_image_hdr(const fs::path& filepath)
    {
        auto hdr_data = read_hdr_raw_(filepath);

        Image<RGB> image(hdr_data.resolution);

        for (int y = 0; y < hdr_data.height; ++y) {
            for (int x = 0; x < hdr_data.width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(hdr_data.width)
                    + static_cast<std::size_t>(x)) * 3;

                image(x, y) = RGB{ hdr_data.raw_data[idx], hdr_data.raw_data[idx + 1], hdr_data.raw_data[idx + 2] };
            }
        }

        return image;
    }

    /**
     * @brief Reads a Radiance HDR (.hdr) image file and returns linear mono data.
     *
     * RGB channels are averaged to produce mono output. Since HDR files store
     * linear radiance values, no gamma conversion is applied.
     *
     * @param filepath Path to the HDR file to read
     * @return The linear mono image.
     * @throws std::runtime_error if the file cannot be opened, is not a valid HDR,
     *         or if any error occurs during reading
     */
    inline Image<float> read_image_hdr_mono(const fs::path& filepath)
    {
        auto hdr_data = read_hdr_raw_(filepath);

        Image<float> image(hdr_data.resolution);

        for (int y = 0; y < hdr_data.height; ++y) {
            for (int x = 0; x < hdr_data.width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(hdr_data.width)
                    + static_cast<std::size_t>(x)) * 3;

                image(x, y) = (hdr_data.raw_data[idx] + hdr_data.raw_data[idx + 1] + hdr_data.raw_data[idx + 2]) / 3.0f;
            }
        }

        return image;
    }

}
