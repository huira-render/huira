#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/io_util.hpp"
#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {

    struct HDRData {
        Resolution resolution{ 0, 0 };
        int width = 0;
        int height = 0;

        std::vector<float> raw_data;
    };

    /**
     * @brief Simple cursor for reading sequentially from a memory buffer.
     *
     * Replaces FILE* operations (fread, fgetc) with equivalent buffer-based reads.
     */
    struct MemCursor_ {
        const unsigned char* data;
        std::size_t size;
        std::size_t pos = 0;

        bool has_remaining(std::size_t n) const { return pos + n <= size; }

        bool read(unsigned char* dst, std::size_t n)
        {
            if (!has_remaining(n)) return false;
            std::memcpy(dst, data + pos, n);
            pos += n;
            return true;
        }

        int getc()
        {
            if (pos >= size) return -1;
            return static_cast<int>(data[pos++]);
        }
    };

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
     * @brief Reads a single line of text from a memory cursor, handling \\n and \\r\\n.
     *
     * @param cur Memory cursor
     * @param line Output string (without line terminator)
     * @return true if a line was read, false on end of buffer
     */
    inline bool read_hdr_line_(MemCursor_& cur, std::string& line)
    {
        line.clear();
        int c;
        while ((c = cur.getc()) != -1) {
            if (c == '\n') return true;
            if (c == '\r') {
                // Consume optional \n after \r
                if (cur.pos < cur.size && cur.data[cur.pos] == '\n') {
                    cur.pos++;
                }
                return true;
            }
            line += static_cast<char>(c);
        }
        return !line.empty();
    }

    /**
     * @brief Decodes a Radiance HDR from an in-memory buffer into raw float RGB data.
     *
     * Parses the Radiance header to extract resolution, then decodes the RGBE
     * pixel data. Supports both uncompressed RGBE scanlines and new-style
     * adaptive run-length encoding (RLE) where each channel is encoded separately.
     *
     * @param data Pointer to the HDR data in memory
     * @param size Size of the data in bytes
     * @return Raw decoded data with resolution and float RGB buffer
     * @throws std::runtime_error if the data is not a valid or supported HDR
     */
    inline HDRData read_hdr_raw_(const unsigned char* data, std::size_t size)
    {
        HUIRA_LOG_INFO("read_hdr_raw_ - Reading HDR from memory (" + std::to_string(size) + " bytes)");

        MemCursor_ cur{ data, size };

        // Parse header: look for magic number and resolution string
        std::string line;

        // First line should be "#?RADIANCE" or "#?RGBE"
        if (!read_hdr_line_(cur, line)) {
            HUIRA_THROW_ERROR("read_hdr_raw_ - Failed to read HDR header");
        }

        if (line.substr(0, 2) != "#?") {
            HUIRA_THROW_ERROR("read_hdr_raw_ - Data is not a valid Radiance HDR (bad magic)");
        }

        // Read header lines until empty line
        bool found_format = false;
        while (read_hdr_line_(cur, line)) {
            if (line.empty()) break;

            if (line.find("FORMAT=32-bit_rle_rgbe") != std::string::npos ||
                line.find("FORMAT=32-bit_rle_xyze") != std::string::npos) {
                found_format = true;
            }
        }

        if (!found_format) {
            HUIRA_LOG_WARNING("read_hdr_raw_ - No FORMAT line found in HDR header, assuming RGBE");
        }

        // Parse resolution string: "-Y height +X width" is the most common
        if (!read_hdr_line_(cur, line)) {
            HUIRA_THROW_ERROR("read_hdr_raw_ - Failed to read resolution string");
        }

        int width = 0;
        int height = 0;

        auto parse_resolution = [&](const std::string& prefix_y, const std::string& prefix_x) -> bool {
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
            HUIRA_THROW_ERROR("read_hdr_raw_ - Unsupported resolution format: \"" + line + "\"");
        }

        if (width <= 0 || height <= 0) {
            HUIRA_THROW_ERROR("read_hdr_raw_ - Invalid dimensions (" +
                std::to_string(width) + " x " + std::to_string(height) + ")");
        }

        // Decode scanlines
        std::size_t num_pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        std::vector<float> raw_data(num_pixels * 3);

        std::vector<unsigned char> scanline(static_cast<std::size_t>(width) * 4);

        for (int y = 0; y < height; ++y) {
            // Read first 4 bytes to determine encoding
            unsigned char header[4];
            if (!cur.read(header, 4)) {
                HUIRA_THROW_ERROR("read_hdr_raw_ - Unexpected end of data at scanline " + std::to_string(y));
            }

            bool is_new_rle = (header[0] == 2 && header[1] == 2 &&
                ((static_cast<int>(header[2]) << 8) | header[3]) == width);

            if (is_new_rle) {
                // New-style adaptive RLE: each of the 4 channels is encoded separately
                for (int ch = 0; ch < 4; ++ch) {
                    int pos = 0;
                    while (pos < width) {
                        int byte = cur.getc();
                        if (byte == -1) {
                            HUIRA_THROW_ERROR("read_hdr_raw_ - Unexpected end of RLE data");
                        }

                        if (byte > 128) {
                            // Run: repeat next byte (byte - 128) times
                            int count = byte - 128;
                            int val = cur.getc();
                            if (val == -1) {
                                HUIRA_THROW_ERROR("read_hdr_raw_ - Unexpected end of RLE data");
                            }
                            for (int i = 0; i < count && pos < width; ++i) {
                                scanline[static_cast<std::size_t>(pos) * 4 + static_cast<std::size_t>(ch)] =
                                    static_cast<unsigned char>(val);
                                ++pos;
                            }
                        }
                        else {
                            // Literal: read 'byte' values
                            int count = byte;
                            for (int i = 0; i < count && pos < width; ++i) {
                                int val = cur.getc();
                                if (val == -1) {
                                    HUIRA_THROW_ERROR("read_hdr_raw_ - Unexpected end of RLE data");
                                }
                                scanline[static_cast<std::size_t>(pos) * 4 + static_cast<std::size_t>(ch)] =
                                    static_cast<unsigned char>(val);
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
                    if (!cur.read(&scanline[4], remaining)) {
                        HUIRA_THROW_ERROR("read_hdr_raw_ - Unexpected end of data");
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

        HDRData hdr_data{};
        hdr_data.resolution = Resolution{ width, height };
        hdr_data.width = width;
        hdr_data.height = height;
        hdr_data.raw_data = std::move(raw_data);

        return hdr_data;
    }

    // =========================================================================
    // RGB readers
    // =========================================================================

    /**
     * @brief Reads a Radiance HDR from an in-memory buffer and returns linear RGB data.
     *
     * HDR files natively store linear radiance values using RGBE encoding,
     * so no gamma conversion is needed. HDR does not support alpha channels.
     *
     * @param data Pointer to the HDR data in memory
     * @param size Size of the data in bytes
     * @return The linear RGB image.
     */
    inline Image<RGB> read_image_hdr(const unsigned char* data, std::size_t size)
    {
        auto hdr_data = read_hdr_raw_(data, size);

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
     * @brief Reads a Radiance HDR file and returns linear RGB data.
     *
     * Convenience overload that reads the file into memory and forwards
     * to the buffer-based implementation.
     *
     * @param filepath Path to the HDR file to read
     * @return The linear RGB image.
     */
    inline Image<RGB> read_image_hdr(const fs::path& filepath)
    {
        auto file_data = read_file_to_buffer(filepath);
        return read_image_hdr(file_data.data(), file_data.size());
    }

    // =========================================================================
    // Mono readers
    // =========================================================================

    /**
     * @brief Reads a Radiance HDR from an in-memory buffer and returns linear mono data.
     *
     * RGB channels are averaged to produce mono output.
     *
     * @param data Pointer to the HDR data in memory
     * @param size Size of the data in bytes
     * @return The linear mono image.
     */
    inline Image<float> read_image_hdr_mono(const unsigned char* data, std::size_t size)
    {
        auto hdr_data = read_hdr_raw_(data, size);

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

    /**
     * @brief Reads a Radiance HDR file and returns linear mono data.
     *
     * Convenience overload that reads the file into memory and forwards
     * to the buffer-based implementation.
     *
     * @param filepath Path to the HDR file to read
     * @return The linear mono image.
     */
    inline Image<float> read_image_hdr_mono(const fs::path& filepath)
    {
        auto file_data = read_file_to_buffer(filepath);
        return read_image_hdr_mono(file_data.data(), file_data.size());
    }

}
