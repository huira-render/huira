#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <tiffio.h>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/color_space.hpp"
#include "huira/images/io/io_util.hpp"
#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {

    struct TIFFData {
        Resolution resolution{ 0, 0 };
        int width = 0;
        int height = 0;

        std::vector<std::vector<float>> channels;
        std::size_t num_channels = 0;

        uint16_t photometric = PHOTOMETRIC_MINISBLACK;
        bool has_alpha = false;
        int alpha_index = -1;
    };

    // =========================================================================
    // TIFFClientOpen memory I/O callbacks
    // =========================================================================

    /**
     * @brief State for libtiff custom memory I/O callbacks.
     *
     * Used with TIFFClientOpen to provide read/seek access over a memory buffer.
     */
    struct TiffMemState_ {
        const unsigned char* data;
        tsize_t size;
        toff_t pos;
    };

    /**
     * @brief libtiff read callback — reads bytes from the memory buffer.
     */
    inline tsize_t tiff_mem_read_(thandle_t handle, tdata_t buf, tsize_t n)
    {
        auto* state = reinterpret_cast<TiffMemState_*>(handle);
        tsize_t available = state->size - static_cast<tsize_t>(state->pos);
        tsize_t to_read = (n < available) ? n : available;
        if (to_read > 0) {
            std::memcpy(buf, state->data + state->pos, static_cast<std::size_t>(to_read));
            state->pos += static_cast<toff_t>(to_read);
        }
        return to_read;
    }

    /**
     * @brief libtiff write callback — not supported for read-only access.
     */
    inline tsize_t tiff_mem_write_(thandle_t, tdata_t, tsize_t)
    {
        return 0;
    }

    /**
     * @brief libtiff seek callback — repositions the read cursor.
     */
    inline toff_t tiff_mem_seek_(thandle_t handle, toff_t offset, int whence)
    {
        auto* state = reinterpret_cast<TiffMemState_*>(handle);
        toff_t new_pos;
        switch (whence) {
        case SEEK_SET: new_pos = offset; break;
        case SEEK_CUR: new_pos = state->pos + offset; break;
        case SEEK_END: new_pos = static_cast<toff_t>(state->size) + offset; break;
        default: return static_cast<toff_t>(-1);
        }
        if (new_pos > static_cast<toff_t>(state->size)) {
            return static_cast<toff_t>(-1);
        }
        state->pos = new_pos;
        return new_pos;
    }

    /**
     * @brief libtiff close callback — no-op for memory buffers.
     */
    inline int tiff_mem_close_(thandle_t)
    {
        return 0;
    }

    /**
     * @brief libtiff size callback — returns the total buffer size.
     */
    inline toff_t tiff_mem_size_(thandle_t handle)
    {
        auto* state = reinterpret_cast<TiffMemState_*>(handle);
        return static_cast<toff_t>(state->size);
    }

    // =========================================================================
    // Raw TIFF decoder
    // =========================================================================

    /**
     * @brief Decodes a TIFF from an in-memory buffer into raw per-channel float data.
     *
     * Uses libtiff via TIFFClientOpen with custom memory I/O callbacks. Extracts
     * all channels as separate floating-point arrays. Handles the following:
     *   - Sample formats: uint8, uint16, uint32, float32
     *   - Photometric interpretations: MinIsBlack, MinIsWhite, RGB, Palette, Separated (CMYK)
     *   - Storage: both stripped and tiled TIFFs
     *   - Planar configurations: chunky (interleaved) and separate (planar)
     *   - Extra samples: detects associated/unassociated alpha
     *
     * MinIsWhite images are inverted so that output is always in MinIsBlack convention.
     * Palette images are expanded to RGB(A) via TIFFReadRGBAImageOriented.
     * Signed integer TIFFs are rejected (not supported for texture loading).
     *
     * @param data Pointer to the TIFF data in memory
     * @param size Size of the data in bytes
     * @return Raw decoded data with per-channel float buffers
     * @throws std::runtime_error if the data is not a valid or supported TIFF
     */
    inline TIFFData read_tiff_raw_(const unsigned char* data, std::size_t size)
    {
        HUIRA_LOG_INFO("read_tiff_raw_ - Reading TIFF from memory (" + std::to_string(size) + " bytes)");

        TIFFSetWarningHandler(nullptr);

        TiffMemState_ mem_state{ data, static_cast<tsize_t>(size), 0 };

        TIFF* tif = TIFFClientOpen(
            "memory", "r",
            reinterpret_cast<thandle_t>(&mem_state),
            tiff_mem_read_,
            tiff_mem_write_,
            tiff_mem_seek_,
            tiff_mem_close_,
            tiff_mem_size_,
            nullptr, nullptr);

        if (!tif) {
            HUIRA_THROW_ERROR("read_tiff_raw_ - Failed to open TIFF from memory buffer");
        }

        uint32_t width = 0, height = 0;
        uint16_t samples_per_pixel = 1;
        uint16_t bits_per_sample = 8;
        uint16_t sample_format = SAMPLEFORMAT_UINT;
        uint16_t photometric = PHOTOMETRIC_MINISBLACK;
        uint16_t planar_config = PLANARCONFIG_CONTIG;

        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
        TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
        TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
        TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sample_format);
        TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);
        TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar_config);

        if (width == 0 || height == 0) {
            TIFFClose(tif);
            HUIRA_THROW_ERROR("read_tiff_raw_ - Invalid dimensions (" +
                std::to_string(width) + " x " + std::to_string(height) + ")");
        }

        // Validate sample format
        bool is_float = (sample_format == SAMPLEFORMAT_IEEEFP);

        if (sample_format == SAMPLEFORMAT_INT) {
            TIFFClose(tif);
            HUIRA_THROW_ERROR("read_tiff_raw_ - Unsupported signed integer sample format");
        }

        if (!is_float && sample_format != SAMPLEFORMAT_UINT) {
            TIFFClose(tif);
            HUIRA_THROW_ERROR("read_tiff_raw_ - Unsupported sample format (" +
                std::to_string(static_cast<int>(sample_format)) + ")");
        }

        if (!is_float && bits_per_sample != 8 && bits_per_sample != 16 && bits_per_sample != 32) {
            TIFFClose(tif);
            HUIRA_THROW_ERROR("read_tiff_raw_ - Unsupported bits per sample (" +
                std::to_string(static_cast<int>(bits_per_sample)) + ")");
        }

        if (is_float && bits_per_sample != 32) {
            TIFFClose(tif);
            HUIRA_THROW_ERROR("read_tiff_raw_ - Unsupported float bit depth (" +
                std::to_string(static_cast<int>(bits_per_sample)) + "), only 32-bit float supported");
        }

        // Handle palette images via RGBA fallback
        if (photometric == PHOTOMETRIC_PALETTE) {
            std::size_t num_pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
            std::vector<uint32_t> rgba_data(num_pixels);

            if (!TIFFReadRGBAImageOriented(tif, width, height, rgba_data.data(), ORIENTATION_TOPLEFT, 0)) {
                TIFFClose(tif);
                HUIRA_THROW_ERROR("read_tiff_raw_ - Failed to read palette TIFF via RGBA");
            }

            TIFFClose(tif);

            TIFFData tiff_data{};
            tiff_data.resolution = Resolution{ static_cast<int>(width), static_cast<int>(height) };
            tiff_data.width = static_cast<int>(width);
            tiff_data.height = static_cast<int>(height);
            tiff_data.num_channels = 4;
            tiff_data.photometric = PHOTOMETRIC_RGB;
            tiff_data.has_alpha = true;
            tiff_data.alpha_index = 3;

            tiff_data.channels.resize(4);
            for (std::size_t ch = 0; ch < 4; ++ch) {
                tiff_data.channels[ch].resize(num_pixels);
            }

            for (std::size_t i = 0; i < num_pixels; ++i) {
                uint32_t pixel = rgba_data[i];
                tiff_data.channels[0][i] = static_cast<float>(TIFFGetR(pixel)) / 255.0f;
                tiff_data.channels[1][i] = static_cast<float>(TIFFGetG(pixel)) / 255.0f;
                tiff_data.channels[2][i] = static_cast<float>(TIFFGetB(pixel)) / 255.0f;
                tiff_data.channels[3][i] = static_cast<float>(TIFFGetA(pixel)) / 255.0f;
            }

            return tiff_data;
        }

        // Detect alpha channel via extra samples
        uint16_t extra_samples_count = 0;
        uint16_t* extra_samples = nullptr;
        TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES, &extra_samples_count, &extra_samples);

        bool has_alpha = false;
        int alpha_index = -1;

        if (extra_samples_count > 0 && extra_samples != nullptr) {
            if (extra_samples[0] == EXTRASAMPLE_ASSOCALPHA ||
                extra_samples[0] == EXTRASAMPLE_UNASSALPHA) {
                has_alpha = true;
                alpha_index = static_cast<int>(samples_per_pixel) - 1;
            }
        }

        // Prepare output channels
        std::size_t num_channels = static_cast<std::size_t>(samples_per_pixel);
        std::size_t num_pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

        std::vector<std::vector<float>> channels(num_channels);
        for (std::size_t ch = 0; ch < num_channels; ++ch) {
            channels[ch].resize(num_pixels);
        }

        // Lambda to convert a raw sample value to float
        auto sample_to_float = [&](const unsigned char* ptr, int byte_offset) -> float {
            if (is_float) {
                float val;
                std::memcpy(&val, ptr + byte_offset, sizeof(float));
                return val;
            }
            else if (bits_per_sample == 8) {
                return static_cast<float>(ptr[byte_offset]) / 255.0f;
            }
            else if (bits_per_sample == 16) {
                uint16_t val;
                std::memcpy(&val, ptr + byte_offset, sizeof(uint16_t));
                return static_cast<float>(val) / 65535.0f;
            }
            else {  // 32-bit integer
                uint32_t val;
                std::memcpy(&val, ptr + byte_offset, sizeof(uint32_t));
                return static_cast<float>(static_cast<double>(val) / static_cast<double>(std::numeric_limits<uint32_t>::max()));
            }
        };

        std::size_t bytes_per_sample = static_cast<std::size_t>(bits_per_sample) / 8;

        bool is_tiled = TIFFIsTiled(tif) != 0;

        if (is_tiled) {
            uint32_t tile_width = 0, tile_height = 0;
            TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tile_width);
            TIFFGetField(tif, TIFFTAG_TILELENGTH, &tile_height);

            tsize_t tile_size = TIFFTileSize(tif);
            std::vector<unsigned char> tile_buf(static_cast<std::size_t>(tile_size));

            if (planar_config == PLANARCONFIG_CONTIG) {
                for (uint32_t ty = 0; ty < height; ty += tile_height) {
                    for (uint32_t tx = 0; tx < width; tx += tile_width) {
                        TIFFReadTile(tif, tile_buf.data(), tx, ty, 0, 0);

                        uint32_t eff_tw = std::min(tile_width, width - tx);
                        uint32_t eff_th = std::min(tile_height, height - ty);

                        for (uint32_t row = 0; row < eff_th; ++row) {
                            for (uint32_t col = 0; col < eff_tw; ++col) {
                                std::size_t tile_pixel_offset = (static_cast<std::size_t>(row) * tile_width
                                    + static_cast<std::size_t>(col)) * samples_per_pixel * bytes_per_sample;
                                std::size_t dst_pixel = static_cast<std::size_t>(ty + row) * width + (tx + col);

                                for (std::size_t ch = 0; ch < num_channels; ++ch) {
                                    channels[ch][dst_pixel] = sample_to_float(
                                        tile_buf.data(),
                                        static_cast<int>(tile_pixel_offset + ch * bytes_per_sample));
                                }
                            }
                        }
                    }
                }
            }
            else {
                // PLANARCONFIG_SEPARATE: one tile set per channel
                for (uint16_t ch = 0; ch < samples_per_pixel; ++ch) {
                    for (uint32_t ty = 0; ty < height; ty += tile_height) {
                        for (uint32_t tx = 0; tx < width; tx += tile_width) {
                            TIFFReadTile(tif, tile_buf.data(), tx, ty, 0, ch);

                            uint32_t eff_tw = std::min(tile_width, width - tx);
                            uint32_t eff_th = std::min(tile_height, height - ty);

                            for (uint32_t row = 0; row < eff_th; ++row) {
                                for (uint32_t col = 0; col < eff_tw; ++col) {
                                    std::size_t tile_offset = (static_cast<std::size_t>(row) * tile_width
                                        + static_cast<std::size_t>(col)) * bytes_per_sample;
                                    std::size_t dst_pixel = static_cast<std::size_t>(ty + row) * width + (tx + col);

                                    channels[ch][dst_pixel] = sample_to_float(
                                        tile_buf.data(), static_cast<int>(tile_offset));
                                }
                            }
                        }
                    }
                }
            }
        }
        else {
            // Stripped storage
            tsize_t scanline_size = TIFFScanlineSize(tif);
            std::vector<unsigned char> scanline_buf(static_cast<std::size_t>(scanline_size));

            if (planar_config == PLANARCONFIG_CONTIG) {
                for (uint32_t y = 0; y < height; ++y) {
                    if (TIFFReadScanline(tif, scanline_buf.data(), y) < 0) {
                        TIFFClose(tif);
                        HUIRA_THROW_ERROR("read_tiff_raw_ - Failed to read scanline " + std::to_string(y));
                    }

                    for (uint32_t x = 0; x < width; ++x) {
                        std::size_t pixel_offset = static_cast<std::size_t>(x) * samples_per_pixel * bytes_per_sample;
                        std::size_t dst_pixel = static_cast<std::size_t>(y) * width + x;

                        for (std::size_t ch = 0; ch < num_channels; ++ch) {
                            channels[ch][dst_pixel] = sample_to_float(
                                scanline_buf.data(),
                                static_cast<int>(pixel_offset + ch * bytes_per_sample));
                        }
                    }
                }
            }
            else {
                // PLANARCONFIG_SEPARATE: one set of scanlines per channel
                for (uint16_t ch = 0; ch < samples_per_pixel; ++ch) {
                    for (uint32_t y = 0; y < height; ++y) {
                        if (TIFFReadScanline(tif, scanline_buf.data(), y, ch) < 0) {
                            TIFFClose(tif);
                            HUIRA_THROW_ERROR("read_tiff_raw_ - Failed to read scanline " +
                                std::to_string(y) + " channel " + std::to_string(static_cast<int>(ch)));
                        }

                        for (uint32_t x = 0; x < width; ++x) {
                            std::size_t sample_offset = static_cast<std::size_t>(x) * bytes_per_sample;
                            std::size_t dst_pixel = static_cast<std::size_t>(y) * width + x;

                            channels[ch][dst_pixel] = sample_to_float(
                                scanline_buf.data(), static_cast<int>(sample_offset));
                        }
                    }
                }
            }
        }

        TIFFClose(tif);

        // Invert MinIsWhite so output is always MinIsBlack convention
        if (photometric == PHOTOMETRIC_MINISWHITE) {
            for (std::size_t ch = 0; ch < num_channels; ++ch) {
                if (ch == static_cast<std::size_t>(alpha_index)) {
                    continue;
                }
                for (std::size_t i = 0; i < num_pixels; ++i) {
                    channels[ch][i] = 1.0f - channels[ch][i];
                }
            }
        }

        TIFFData tiff_data{};
        tiff_data.resolution = Resolution{ static_cast<int>(width), static_cast<int>(height) };
        tiff_data.width = static_cast<int>(width);
        tiff_data.height = static_cast<int>(height);
        tiff_data.channels = std::move(channels);
        tiff_data.num_channels = num_channels;
        tiff_data.photometric = photometric;
        tiff_data.has_alpha = has_alpha;
        tiff_data.alpha_index = alpha_index;

        return tiff_data;
    }

    // =========================================================================
    // RGB readers
    // =========================================================================

    /**
     * @brief Reads a TIFF from an in-memory buffer and returns linear RGB + optional alpha data.
     *
     * Interprets the TIFF data as RGB color:
     *   - 1-channel: promoted to RGB (equal values in all channels)
     *   - 3-channel: interpreted as RGB directly
     *   - 4-channel with alpha: RGB + separate alpha
     *   - Other channel counts: throws an error
     *
     * @param data Pointer to the TIFF data in memory
     * @param size Size of the data in bytes
     * @param read_alpha If false, alpha channel is not extracted even if present
     * @return A pair containing the linear RGB image and an optional alpha image.
     */
    inline std::pair<Image<RGB>, Image<float>> read_image_tiff_rgb(const unsigned char* data, std::size_t size, bool read_alpha)
    {
        auto tiff_data = read_tiff_raw_(data, size);

        std::size_t color_channels = tiff_data.has_alpha ? tiff_data.num_channels - 1 : tiff_data.num_channels;

        if (color_channels != 1 && color_channels != 3) {
            HUIRA_THROW_ERROR("read_image_tiff_rgb - Cannot interpret " +
                std::to_string(color_channels) + "-channel TIFF as RGB");
        }

        bool extract_alpha = tiff_data.has_alpha && read_alpha;

        Image<RGB> image(tiff_data.resolution);
        Image<float> alpha_image(0, 0);

        if (extract_alpha) {
            alpha_image = Image<float>(tiff_data.resolution, 1.0f);
        }

        std::size_t num_pixels = static_cast<std::size_t>(tiff_data.width) * static_cast<std::size_t>(tiff_data.height);

        if (color_channels == 1) {
            for (std::size_t i = 0; i < num_pixels; ++i) {
                float v = tiff_data.channels[0][i];
                int y = static_cast<int>(i / static_cast<std::size_t>(tiff_data.width));
                int x = static_cast<int>(i % static_cast<std::size_t>(tiff_data.width));
                image(x, y) = RGB{ v, v, v };
            }
        }
        else {
            for (std::size_t i = 0; i < num_pixels; ++i) {
                int y = static_cast<int>(i / static_cast<std::size_t>(tiff_data.width));
                int x = static_cast<int>(i % static_cast<std::size_t>(tiff_data.width));
                image(x, y) = RGB{
                    tiff_data.channels[0][i],
                    tiff_data.channels[1][i],
                    tiff_data.channels[2][i]
                };
            }
        }

        if (extract_alpha) {
            for (std::size_t i = 0; i < num_pixels; ++i) {
                int y = static_cast<int>(i / static_cast<std::size_t>(tiff_data.width));
                int x = static_cast<int>(i % static_cast<std::size_t>(tiff_data.width));
                alpha_image(x, y) = tiff_data.channels[static_cast<std::size_t>(tiff_data.alpha_index)][i];
            }
        }

        return { std::move(image), std::move(alpha_image) };
    }

    /**
     * @brief Reads a TIFF file and returns linear RGB + optional alpha data.
     *
     * Convenience overload that reads the file into memory and forwards
     * to the buffer-based implementation.
     */
    inline std::pair<Image<RGB>, Image<float>> read_image_tiff_rgb(const fs::path& filepath, bool read_alpha)
    {
        auto file_data = read_file_to_buffer(filepath);
        return read_image_tiff_rgb(file_data.data(), file_data.size(), read_alpha);
    }

    // =========================================================================
    // Mono readers
    // =========================================================================

    /**
     * @brief Reads a TIFF from an in-memory buffer and returns linear mono + optional alpha data.
     *
     * Interprets the TIFF data as single-channel:
     *   - 1-channel: returned directly
     *   - 3-channel: averaged to mono
     *   - Other channel counts (excluding alpha): throws an error
     *
     * @param data Pointer to the TIFF data in memory
     * @param size Size of the data in bytes
     * @param read_alpha If false, alpha channel is not extracted even if present
     * @return A pair containing the linear mono image and an optional alpha image.
     */
    inline std::pair<Image<float>, Image<float>> read_image_tiff_mono(const unsigned char* data, std::size_t size, bool read_alpha)
    {
        auto tiff_data = read_tiff_raw_(data, size);

        std::size_t color_channels = tiff_data.has_alpha ? tiff_data.num_channels - 1 : tiff_data.num_channels;

        if (color_channels != 1 && color_channels != 3) {
            HUIRA_THROW_ERROR("read_image_tiff_mono - Cannot interpret " +
                std::to_string(color_channels) + "-channel TIFF as mono");
        }

        bool extract_alpha = tiff_data.has_alpha && read_alpha;

        Image<float> image(tiff_data.resolution);
        Image<float> alpha_image(0, 0);

        if (extract_alpha) {
            alpha_image = Image<float>(tiff_data.resolution, 1.0f);
        }

        std::size_t num_pixels = static_cast<std::size_t>(tiff_data.width) * static_cast<std::size_t>(tiff_data.height);

        if (color_channels == 1) {
            for (std::size_t i = 0; i < num_pixels; ++i) {
                int y = static_cast<int>(i / static_cast<std::size_t>(tiff_data.width));
                int x = static_cast<int>(i % static_cast<std::size_t>(tiff_data.width));
                image(x, y) = tiff_data.channels[0][i];
            }
        }
        else {
            for (std::size_t i = 0; i < num_pixels; ++i) {
                int y = static_cast<int>(i / static_cast<std::size_t>(tiff_data.width));
                int x = static_cast<int>(i % static_cast<std::size_t>(tiff_data.width));
                image(x, y) = (tiff_data.channels[0][i] + tiff_data.channels[1][i] + tiff_data.channels[2][i]) / 3.0f;
            }
        }

        if (extract_alpha) {
            for (std::size_t i = 0; i < num_pixels; ++i) {
                int y = static_cast<int>(i / static_cast<std::size_t>(tiff_data.width));
                int x = static_cast<int>(i % static_cast<std::size_t>(tiff_data.width));
                alpha_image(x, y) = tiff_data.channels[static_cast<std::size_t>(tiff_data.alpha_index)][i];
            }
        }

        return { std::move(image), std::move(alpha_image) };
    }

    /**
     * @brief Reads a TIFF file and returns linear mono + optional alpha data.
     *
     * Convenience overload that reads the file into memory and forwards
     * to the buffer-based implementation.
     */
    inline std::pair<Image<float>, Image<float>> read_image_tiff_mono(const fs::path& filepath, bool read_alpha)
    {
        auto file_data = read_file_to_buffer(filepath);
        return read_image_tiff_mono(file_data.data(), file_data.size(), read_alpha);
    }

}
