#include <cstddef>
#include <filesystem>
#include <utility>

#include <png.h>

#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/detail/logger.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/convert_pixel.hpp"

namespace fs = std::filesystem;

namespace huira {
    ImageFormat detect_image_format(const fs::path& filepath)
    {
        // TODO More robust format detection (e.g., magic numbers)
        auto ext = filepath.extension().string();
        if (ext == ".png" || ext == ".PNG") {
            return ImageFormat::IMAGE_FORMAT_PNG;
        }
        else if (ext == ".jpg" || ext == ".jpeg" || ext == ".JPG" || ext == ".JPEG") {
            return ImageFormat::IMAGE_FORMAT_JPEG;
        }
        else if (ext == ".tiff" || ext == ".tif" || ext == ".TIFF" || ext == ".TIF") {
            return ImageFormat::IMAGE_FORMAT_TIFF;
        }
        else if (ext == ".fits" || ext == ".FITS") {
            return ImageFormat::IMAGE_FORMAT_FITS;
        }
        else {
            return ImageFormat::IMAGE_FORMAT_UNKNOWN;
        }
    }

    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image(const fs::path& filepath)
    {
        ImageFormat fmt = detect_image_format(filepath);

        if (fmt == ImageFormat::IMAGE_FORMAT_PNG) {
            return read_image_png<T>(filepath);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_JPEG) {
            return read_image_jpeg<T>(filepath);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_TIFF) {
            return read_image_tiff<T>(filepath);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_FITS) {
            return read_image_fits<T>(filepath);
        }
        else {
            HUIRA_THROW_ERROR("Unsupported or unknown image format for file: " + filepath.string());
        }
    }

    struct FileCloser {
        void operator()(FILE* f) const noexcept {
            if (f) fclose(f);
        }
    };

    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_png(const fs::path& filepath)
    {
        // Open file
        std::unique_ptr<FILE, FileCloser> fp(fopen(filepath.string().c_str(), "rb"));
        if (!fp) {
            HUIRA_THROW_ERROR("Failed to open PNG file: " + filepath.string());
        }

        // Verify PNG signature
        std::array<png_byte, 8> sig{};
        if (fread(sig.data(), 1, 8, fp.get()) != 8 ||
            png_sig_cmp(sig.data(), 0, 8) != 0) {
            HUIRA_THROW_ERROR("File is not a valid PNG: " + filepath.string());
        }

        // Create read struct
        png_structp png_ptr = png_create_read_struct(
            PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

        if (!png_ptr) {
            HUIRA_THROW_ERROR("Failed to create PNG read struct");
        }

        // Create info struct
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            png_destroy_read_struct(&png_ptr, nullptr, nullptr);
            HUIRA_THROW_ERROR("Failed to create PNG info struct");
        }

        // Set up error handling (libpng uses setjmp/longjmp)
        if (setjmp(png_jmpbuf(png_ptr))) {
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            HUIRA_THROW_ERROR("Error during PNG read: " + filepath.string());
        }

        // Initialize I/O
        png_init_io(png_ptr, fp.get());
        png_set_sig_bytes(png_ptr, 8);  // We already read 8 bytes for signature

        // Read header info
        png_read_info(png_ptr, info_ptr);

        png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
        png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
        png_byte color_type = png_get_color_type(png_ptr, info_ptr);
        png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

        // Set up transformations to get consistent output format

        // Expand paletted images to RGB
        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            png_set_palette_to_rgb(png_ptr);
        }

        // Expand grayscale images with < 8 bits to 8 bits
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
            png_set_expand_gray_1_2_4_to_8(png_ptr);
        }

        // Expand tRNS chunk to full alpha channel
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_set_tRNS_to_alpha(png_ptr);
        }

        // Convert 16-bit to 8-bit
        if (bit_depth == 16) {
            png_set_strip_16(png_ptr);
        }

        // Convert grayscale to RGB
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
            png_set_gray_to_rgb(png_ptr);
        }

        // Update info after transformations
        png_read_update_info(png_ptr, info_ptr);

        // After transforms, determine if we have alpha
        png_byte final_color_type = png_get_color_type(png_ptr, info_ptr);
        bool has_alpha = (final_color_type & PNG_COLOR_MASK_ALPHA) != 0;
        unsigned int channels = has_alpha ? 4 : 3;

        // Allocate row pointers
        std::vector<png_bytep> row_pointers(height);
        std::vector<png_byte> raw_data(width * height * channels);

        for (png_uint_32 y = 0; y < height; ++y) {
            row_pointers[y] = raw_data.data() + y * width * channels;
        }

        // Read the image data
        png_read_image(png_ptr, row_pointers.data());
        png_read_end(png_ptr, nullptr);

        // Clean up libpng structures
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

        // Create output images
        Image<T> image(static_cast<std::size_t>(width),
            static_cast<std::size_t>(height));
        Image<float> alpha_image(0, 0);

        if (has_alpha) {
            alpha_image = Image<float>(static_cast<std::size_t>(width),
                static_cast<std::size_t>(height),
                1.0f);
        }

        // Convert raw data to your pixel format
        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                std::size_t idx = (y * width + x) * channels;

                float r = static_cast<float>(raw_data[idx + 0]) / 255.f;
                float g = static_cast<float>(raw_data[idx + 1]) / 255.f;
                float b = static_cast<float>(raw_data[idx + 2]) / 255.f;

                image(x, y) = convert_pixel<T>(Vec3<float>{r,g,b});

                if (has_alpha) {
                    uint8_t a = raw_data[idx + 3];
                    alpha_image(x, y) = static_cast<float>(a) / 255.0f;
                }
            }
        }

        return { std::move(image), std::move(alpha_image) };
    }

    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_jpeg(const fs::path& filepath)
    {
        (void)filepath;
        HUIRA_THROW_ERROR("NOT YET IMPLEMENTED: read_image_jpeg.");
    }

    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_tiff(const fs::path& filepath)
    {
        (void)filepath;
        HUIRA_THROW_ERROR("NOT YET IMPLEMENTED: read_image_tiff.");
    }

    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_fits(const fs::path& filepath)
    {
        (void)filepath;
        HUIRA_THROW_ERROR("NOT YET IMPLEMENTED: read_image_fits.");
    }
}
