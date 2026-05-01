#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <vector>

#include <turbojpeg.h>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/color_space.hpp"
#include "huira/images/io/convert_pixel.hpp"
#include "huira/images/io/io_util.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/paths.hpp"

namespace fs = std::filesystem;

namespace huira {

struct JPEGData {
    Resolution resolution{0, 0};
    int width = 0;
    int height = 0;

    std::vector<unsigned char> raw_data;
};

/**
 * @brief Decodes a JPEG from an in-memory buffer into raw RGB byte data.
 *
 * Uses the TurboJPEG API to decompress the JPEG into 8-bit RGB pixel data,
 * regardless of whether the source is grayscale or color. TurboJPEG handles
 * the grayscale-to-RGB promotion internally.
 *
 * @param data Pointer to the JPEG data in memory
 * @param size Size of the data in bytes
 * @return Raw decoded data with resolution and pixel buffer
 * @throws std::runtime_error if the data cannot be decoded
 */
inline JPEGData read_jpeg_raw_(const unsigned char* data, std::size_t size, bool force_gray = false)
{
    HUIRA_LOG_INFO("read_jpeg_raw_ - Reading JPEG from memory (" + std::to_string(size) +
                   " bytes)");

    tjhandle decompressor = tjInitDecompress();
    if (!decompressor) {
        HUIRA_THROW_ERROR("read_jpeg_raw_ - Failed to create TurboJPEG decompressor");
    }

    int width = 0;
    int height = 0;
    int subsamp = 0;
    int colorspace = 0;

    if (tjDecompressHeader3(decompressor,
                            data,
                            static_cast<unsigned long>(size),
                            &width,
                            &height,
                            &subsamp,
                            &colorspace) != 0) {
        std::string tj_err = tjGetErrorStr2(decompressor);
        tjDestroy(decompressor);
        HUIRA_THROW_ERROR("read_jpeg_raw_ - Failed to read JPEG header (" + tj_err + ")");
    }

    int pixel_format = force_gray ? TJPF_GRAY : TJPF_RGB;
    int channels = force_gray ? 1 : 3;
    std::vector<unsigned char> raw_data(static_cast<std::size_t>(width * height * channels));

    if (tjDecompress2(decompressor,
                      data,
                      static_cast<unsigned long>(size),
                      raw_data.data(),
                      width,
                      0,
                      height,
                      pixel_format,
                      0) != 0) {
        std::string tj_err = tjGetErrorStr2(decompressor);
        tjDestroy(decompressor);
        HUIRA_THROW_ERROR("read_jpeg_raw_ - Failed to decompress JPEG (" + tj_err + ")");
    }

    tjDestroy(decompressor);

    JPEGData jpeg_data{};
    jpeg_data.resolution = Resolution{width, height};
    jpeg_data.width = width;
    jpeg_data.height = height;
    jpeg_data.raw_data = std::move(raw_data);

    return jpeg_data;
}

// =========================================================================
// RGB readers
// =========================================================================

/**
 * @brief Reads a JPEG from an in-memory buffer and returns linear RGB data.
 *
 * Converts from sRGB to linear light. Grayscale JPEGs are promoted to RGB
 * by TurboJPEG during decompression.
 *
 * @param data Pointer to the JPEG data in memory
 * @param size Size of the data in bytes
 * @return An ImageBundle<RGB> containing the linear RGB image.
 */
inline ImageBundle<RGB> read_image_jpeg(const unsigned char* data, std::size_t size)
{
    auto jpeg_data = read_jpeg_raw_(data, size);

    ImageBundle<RGB> bundle{Image<RGB>(jpeg_data.resolution)};

    constexpr int channels = 3;

    for (int y = 0; y < jpeg_data.height; ++y) {
        for (int x = 0; x < jpeg_data.width; ++x) {
            std::size_t idx =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(jpeg_data.width) +
                 static_cast<std::size_t>(x)) *
                channels;

            float r = integer_to_float<std::uint8_t>(jpeg_data.raw_data[idx + 0]);
            float g = integer_to_float<std::uint8_t>(jpeg_data.raw_data[idx + 1]);
            float b = integer_to_float<std::uint8_t>(jpeg_data.raw_data[idx + 2]);

            bundle.image(x, y) = RGB{r, g, b};
        }
    }

    return bundle;
}

/**
 * @brief Reads a JPEG file and returns linear RGB data.
 *
 * Convenience overload that reads the file into memory and forwards
 * to the buffer-based implementation.
 *
 * @param filepath Path to the JPEG file to read
 * @return An ImageBundle<RGB> containing the linear RGB image.
 */
inline ImageBundle<RGB> read_image_jpeg(const fs::path& filepath)
{
    auto file_data = read_file_to_buffer(filepath);
    return read_image_jpeg(file_data.data(), file_data.size());
}

// =========================================================================
// Mono readers
// =========================================================================

/**
 * @brief Reads a JPEG from an in-memory buffer and returns linear mono data.
 *
 * Converts from sRGB to linear light, then averages the RGB channels.
 *
 * @param data Pointer to the JPEG data in memory
 * @param size Size of the data in bytes
 * @return An ImageBundle<float> containing the linear mono image.
 */
inline ImageBundle<float> read_image_jpeg_mono(const unsigned char* data, std::size_t size)
{
    auto jpeg_data = read_jpeg_raw_(data, size, true);

    ImageBundle<float> bundle{Image<float>(jpeg_data.resolution)};

    for (std::size_t i = 0; i < static_cast<std::size_t>(jpeg_data.width * jpeg_data.height); ++i) {
        bundle.image[i] = integer_to_float<std::uint8_t>(jpeg_data.raw_data[i]);
    }

    return bundle;
}

/**
 * @brief Reads a JPEG file and returns linear mono data.
 *
 * Convenience overload that reads the file into memory and forwards
 * to the buffer-based implementation.
 *
 * @param filepath Path to the JPEG file to read
 * @return An ImageBundle<float> containing the linear mono image.
 */
inline ImageBundle<float> read_image_jpeg_mono(const fs::path& filepath)
{
    auto file_data = read_file_to_buffer(filepath);
    return read_image_jpeg_mono(file_data.data(), file_data.size());
}

// =========================================================================
// Writers (file-only, unchanged)
// =========================================================================

/**
 * @brief Writes a linear RGB image to a JPEG file (sRGB encoded).
 *
 * Converts the image from linear color space to sRGB and compresses it
 * using TurboJPEG. The function automatically creates necessary directories.
 *
 * @param filepath Path where the JPEG file will be written
 * @param image An ImageBundle<RGB> with the linear RGB image to write
 * @param quality JPEG compression quality (1-100, default 95)
 * @throws std::runtime_error if the file cannot be created, the image is empty,
 *         quality is invalid, or if any error occurs during writing
 */
inline void
write_image_jpeg(const fs::path& filepath, const ImageBundle<RGB>& output_image, int quality)
{
    HUIRA_LOG_INFO("write_image_jpeg - Writing to: " + filepath.string());

    if (output_image.image.width() == 0 || output_image.image.height() == 0) {
        HUIRA_THROW_ERROR("write_image_jpeg - Cannot write empty image: " + filepath.string());
    }

    if (quality < 1 || quality > 100) {
        HUIRA_THROW_ERROR("write_image_jpeg - quality must be 1-100, got: " +
                          std::to_string(quality));
    }

    make_path(filepath);

    int width = output_image.image.width();
    int height = output_image.image.height();

    // Convert linear RGB to sRGB 8-bit
    constexpr int channels = 3;
    std::vector<unsigned char> srgb_data(static_cast<std::size_t>(width) *
                                         static_cast<std::size_t>(height) * channels);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                               static_cast<std::size_t>(x)) *
                              channels;
            const RGB& pixel = output_image.image(x, y);

            srgb_data[idx + 0] = float_to_integer<uint8_t>(pixel[0]);
            srgb_data[idx + 1] = float_to_integer<uint8_t>(pixel[1]);
            srgb_data[idx + 2] = float_to_integer<uint8_t>(pixel[2]);
        }
    }

    // Compress using TurboJPEG
    tjhandle compressor = tjInitCompress();
    if (!compressor) {
        HUIRA_THROW_ERROR("write_image_jpeg - Failed to create TurboJPEG compressor");
    }

    unsigned char* jpeg_buf = nullptr;
    unsigned long jpeg_size = 0;

    if (tjCompress2(compressor,
                    srgb_data.data(),
                    width,
                    0,
                    height,
                    TJPF_RGB,
                    &jpeg_buf,
                    &jpeg_size,
                    TJSAMP_444,
                    quality,
                    0) != 0) {
        std::string tj_err = tjGetErrorStr2(compressor);
        tjDestroy(compressor);
        HUIRA_THROW_ERROR("write_image_jpeg - Failed to compress JPEG: " + filepath.string() +
                          " (" + tj_err + ")");
    }

    tjDestroy(compressor);

    // Write compressed data to file
#ifdef _MSC_VER
    FILE* fp = nullptr;
    errno_t err = fopen_s(&fp, filepath.string().c_str(), "wb");
    if (err != 0 || !fp) {
        tjFree(jpeg_buf);
        HUIRA_THROW_ERROR("write_image_jpeg - Failed to open file for writing: " +
                          filepath.string());
    }
#else
    FILE* fp = fopen(filepath.string().c_str(), "wb");
    if (!fp) {
        tjFree(jpeg_buf);
        HUIRA_THROW_ERROR("write_image_jpeg - Failed to open file for writing: " +
                          filepath.string());
    }
#endif

    std::size_t written = fwrite(jpeg_buf, 1, jpeg_size, fp);
    fclose(fp);
    tjFree(jpeg_buf);

    if (written != jpeg_size) {
        HUIRA_THROW_ERROR("write_image_jpeg - Failed to write complete JPEG data: " +
                          filepath.string());
    }
}

/**
 * @brief Writes a linear mono image to a JPEG file (sRGB encoded).
 *
 * Convenience overload that promotes a mono image to RGB before writing.
 *
 * @param filepath Path where the JPEG file will be written
 * @param image An ImageBundle<float> with the linear mono image to write
 * @param quality JPEG compression quality (1-100, default 95)
 */
inline void
write_image_jpeg(const fs::path& filepath, const ImageBundle<float>& output_image, int quality)
{
    Image<RGB> image_rgb(output_image.image.width(), output_image.image.height());
    for (std::size_t i = 0; i < output_image.image.size(); ++i) {
        float val = output_image.image[i];
        image_rgb[i] = RGB{val, val, val};
    }
    ImageBundle<RGB> output_bundle(output_image, std::move(image_rgb));
    write_image_jpeg(filepath, output_bundle, quality);
}

} // namespace huira
