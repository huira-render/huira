#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <vector>

#include <turbojpeg.h>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/color_space.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/paths.hpp"

namespace fs = std::filesystem;

namespace huira {

    /**
     * @brief Raw decoded JPEG data before pixel interpretation.
     */
    struct JPEGData {
        Resolution resolution{ 0, 0 };
        int width = 0;
        int height = 0;

        std::vector<unsigned char> raw_data;  ///< Decoded pixel data (always RGB, 3 bytes per pixel)
    };

    /**
     * @brief Decodes a JPEG file into raw RGB byte data.
     *
     * Uses the TurboJPEG API to decompress the JPEG into 8-bit RGB pixel data,
     * regardless of whether the source is grayscale or color. TurboJPEG handles
     * the grayscale-to-RGB promotion internally.
     *
     * @param filepath Path to the JPEG file to read
     * @return Raw decoded data with resolution and pixel buffer
     * @throws std::runtime_error if the file cannot be opened or decoded
     */
    inline JPEGData read_jpeg_raw_(const fs::path& filepath)
    {
        HUIRA_LOG_INFO("read_jpeg_raw_ - Reading image from: " + filepath.string());

        // Read entire file into memory
#ifdef _MSC_VER
        FILE* fp = nullptr;
        errno_t err = fopen_s(&fp, filepath.string().c_str(), "rb");
        if (err != 0 || !fp) {
            HUIRA_THROW_ERROR("read_jpeg_raw_ - Failed to open JPEG file: " + filepath.string());
        }
#else
        FILE* fp = fopen(filepath.string().c_str(), "rb");
        if (!fp) {
            HUIRA_THROW_ERROR("read_jpeg_raw_ - Failed to open JPEG file: " + filepath.string());
        }
#endif

        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        if (file_size <= 0) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_jpeg_raw_ - Failed to determine file size or file is empty: " + filepath.string());
        }
        fseek(fp, 0, SEEK_SET);

        std::vector<unsigned char> file_data(static_cast<std::size_t>(file_size));
        if (fread(file_data.data(), 1, static_cast<std::size_t>(file_size), fp) != static_cast<std::size_t>(file_size)) {
            fclose(fp);
            HUIRA_THROW_ERROR("read_jpeg_raw_ - Failed to read JPEG file: " + filepath.string());
        }
        fclose(fp);

        // Decompress using TurboJPEG
        tjhandle decompressor = tjInitDecompress();
        if (!decompressor) {
            HUIRA_THROW_ERROR("read_jpeg_raw_ - Failed to create TurboJPEG decompressor");
        }

        int width = 0;
        int height = 0;
        int subsamp = 0;
        int colorspace = 0;

        if (tjDecompressHeader3(decompressor, file_data.data(),
            static_cast<unsigned long>(file_size),
            &width, &height, &subsamp, &colorspace) != 0) {
            std::string tj_err = tjGetErrorStr2(decompressor);
            tjDestroy(decompressor);
            HUIRA_THROW_ERROR("read_jpeg_raw_ - Failed to read JPEG header: " + filepath.string() + " (" + tj_err + ")");
        }

        // Always decompress to RGB — TurboJPEG handles grayscale promotion
        constexpr int pixel_format = TJPF_RGB;
        constexpr int channels = 3;
        std::vector<unsigned char> raw_data(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * channels);

        if (tjDecompress2(decompressor, file_data.data(),
            static_cast<unsigned long>(file_size),
            raw_data.data(), width, 0, height, pixel_format, 0) != 0) {
            std::string tj_err = tjGetErrorStr2(decompressor);
            tjDestroy(decompressor);
            HUIRA_THROW_ERROR("read_jpeg_raw_ - Failed to decompress JPEG: " + filepath.string() + " (" + tj_err + ")");
        }

        tjDestroy(decompressor);

        JPEGData jpeg_data{};
        jpeg_data.resolution = Resolution{ width, height };
        jpeg_data.width = width;
        jpeg_data.height = height;
        jpeg_data.raw_data = std::move(raw_data);

        return jpeg_data;
    }

    /**
     * @brief Reads a JPEG image file and returns linear RGB data.
     *
     * Loads a JPEG image from disk and converts from sRGB to linear light.
     * Grayscale JPEGs are promoted to RGB by TurboJPEG during decompression.
     *
     * @param filepath Path to the JPEG file to read
     * @return The linear RGB image.
     * @throws std::runtime_error if the file cannot be opened, is not a valid JPEG,
     *         or if any error occurs during reading
     */
    inline Image<RGB> read_image_jpeg(const fs::path& filepath)
    {
        auto jpeg_data = read_jpeg_raw_(filepath);

        Image<RGB> image(jpeg_data.resolution);

        constexpr int channels = 3;

        for (int y = 0; y < jpeg_data.height; ++y) {
            for (int x = 0; x < jpeg_data.width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(jpeg_data.width)
                    + static_cast<std::size_t>(x)) * channels;

                float r = srgb_to_linear(static_cast<float>(jpeg_data.raw_data[idx + 0]) / 255.0f);
                float g = srgb_to_linear(static_cast<float>(jpeg_data.raw_data[idx + 1]) / 255.0f);
                float b = srgb_to_linear(static_cast<float>(jpeg_data.raw_data[idx + 2]) / 255.0f);

                image(x, y) = RGB{ r, g, b };
            }
        }

        return image;
    }

    /**
     * @brief Reads a JPEG image file and returns linear mono data.
     *
     * Loads a JPEG image from disk, converts from sRGB to linear light,
     * and averages the RGB channels to produce a single-channel result.
     *
     * @param filepath Path to the JPEG file to read
     * @return The linear mono image.
     * @throws std::runtime_error if the file cannot be opened, is not a valid JPEG,
     *         or if any error occurs during reading
     */
    inline Image<float> read_image_jpeg_mono(const fs::path& filepath)
    {
        auto jpeg_data = read_jpeg_raw_(filepath);

        Image<float> image(jpeg_data.resolution);

        constexpr int channels = 3;

        for (int y = 0; y < jpeg_data.height; ++y) {
            for (int x = 0; x < jpeg_data.width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(jpeg_data.width)
                    + static_cast<std::size_t>(x)) * channels;

                float r = srgb_to_linear(static_cast<float>(jpeg_data.raw_data[idx + 0]) / 255.0f);
                float g = srgb_to_linear(static_cast<float>(jpeg_data.raw_data[idx + 1]) / 255.0f);
                float b = srgb_to_linear(static_cast<float>(jpeg_data.raw_data[idx + 2]) / 255.0f);

                image(x, y) = (r + g + b) / 3.0f;
            }
        }

        return image;
    }

    /**
     * @brief Writes a linear RGB image to a JPEG file (sRGB encoded).
     *
     * Converts the image from linear color space to sRGB and compresses it
     * using TurboJPEG. The function automatically creates necessary directories.
     *
     * @param filepath Path where the JPEG file will be written
     * @param image The linear RGB image to write
     * @param quality JPEG compression quality (1-100, default 95)
     * @throws std::runtime_error if the file cannot be created, the image is empty,
     *         quality is invalid, or if any error occurs during writing
     */
    inline void write_image_jpeg(const fs::path& filepath, const Image<RGB>& image, int quality)
    {
        HUIRA_LOG_INFO("write_image_jpeg - Writing to: " + filepath.string());

        if (image.width() == 0 || image.height() == 0) {
            HUIRA_THROW_ERROR("write_image_jpeg - Cannot write empty image: " + filepath.string());
        }

        if (quality < 1 || quality > 100) {
            HUIRA_THROW_ERROR("write_image_jpeg - quality must be 1-100, got: " + std::to_string(quality));
        }

        make_path(filepath);

        int width = image.width();
        int height = image.height();

        // Convert linear RGB to sRGB 8-bit
        constexpr int channels = 3;
        std::vector<unsigned char> srgb_data(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * channels);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                    + static_cast<std::size_t>(x)) * channels;
                const RGB& pixel = image(x, y);

                srgb_data[idx + 0] = float_to_integer<uint8_t>(linear_to_srgb(pixel[0]));
                srgb_data[idx + 1] = float_to_integer<uint8_t>(linear_to_srgb(pixel[1]));
                srgb_data[idx + 2] = float_to_integer<uint8_t>(linear_to_srgb(pixel[2]));
            }
        }

        // Compress using TurboJPEG
        tjhandle compressor = tjInitCompress();
        if (!compressor) {
            HUIRA_THROW_ERROR("write_image_jpeg - Failed to create TurboJPEG compressor");
        }

        unsigned char* jpeg_buf = nullptr;
        unsigned long jpeg_size = 0;

        if (tjCompress2(compressor, srgb_data.data(), width, 0, height,
            TJPF_RGB, &jpeg_buf, &jpeg_size, TJSAMP_444, quality,
            TJFLAG_NOREALLOC) != 0) {
            // TJFLAG_NOREALLOC may fail if initial buffer is too small; retry without it
            if (tjCompress2(compressor, srgb_data.data(), width, 0, height,
                TJPF_RGB, &jpeg_buf, &jpeg_size, TJSAMP_444, quality, 0) != 0) {
                std::string tj_err = tjGetErrorStr2(compressor);
                tjDestroy(compressor);
                HUIRA_THROW_ERROR("write_image_jpeg - Failed to compress JPEG: " + filepath.string() + " (" + tj_err + ")");
            }
        }

        tjDestroy(compressor);

        // Write compressed data to file
#ifdef _MSC_VER
        FILE* fp = nullptr;
        errno_t err = fopen_s(&fp, filepath.string().c_str(), "wb");
        if (err != 0 || !fp) {
            tjFree(jpeg_buf);
            HUIRA_THROW_ERROR("write_image_jpeg - Failed to open file for writing: " + filepath.string());
        }
#else
        FILE* fp = fopen(filepath.string().c_str(), "wb");
        if (!fp) {
            tjFree(jpeg_buf);
            HUIRA_THROW_ERROR("write_image_jpeg - Failed to open file for writing: " + filepath.string());
        }
#endif

        std::size_t written = fwrite(jpeg_buf, 1, jpeg_size, fp);
        fclose(fp);
        tjFree(jpeg_buf);

        if (written != jpeg_size) {
            HUIRA_THROW_ERROR("write_image_jpeg - Failed to write complete JPEG data: " + filepath.string());
        }
    }

    /**
     * @brief Writes a linear mono image to a JPEG file (sRGB encoded).
     *
     * Convenience overload that promotes a mono image to RGB before writing.
     *
     * @param filepath Path where the JPEG file will be written
     * @param image The linear mono image to write
     * @param quality JPEG compression quality (1-100, default 95)
     */
    inline void write_image_jpeg(const fs::path& filepath, const Image<float>& image, int quality)
    {
        Image<RGB> image_rgb(image.width(), image.height());
        for (std::size_t i = 0; i < image.size(); ++i) {
            image_rgb[i] = RGB{ image[i], image[i], image[i] };
        }

        write_image_jpeg(filepath, image_rgb, quality);
    }

}
