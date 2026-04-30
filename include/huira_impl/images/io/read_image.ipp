#include <cstddef>
#include <filesystem>

#include "huira/concepts/pixel_concepts.hpp"
#include "huira/util/logger.hpp"
#include "huira/images/image.hpp"

#include "huira/images/io/bmp_io.hpp"
#include "huira/images/io/fits_io.hpp"
#include "huira/images/io/hdr_io.hpp"
#include "huira/images/io/jpeg_io.hpp"
#include "huira/images/io/png_io.hpp"
#include "huira/images/io/tga_io.hpp"
#include "huira/images/io/tiff_io.hpp"


namespace fs = std::filesystem;

namespace huira {
    inline ImageFormat detect_image_format(const fs::path& filepath)
    {
        // TODO More robust format detection (e.g., magic numbers)
        auto ext = filepath.extension().string();
        if (ext == ".bmp" || ext == ".BMP") {
            return ImageFormat::IMAGE_FORMAT_BMP;
        }
        else if (ext == ".fits" || ext == ".FITS") {
            return ImageFormat::IMAGE_FORMAT_FITS;
        }
        else if (ext == ".hdr" || ext == ".HDR") {
            return ImageFormat::IMAGE_FORMAT_HDR;
        }
        else if (ext == ".jpg" || ext == ".jpeg" || ext == ".JPG" || ext == ".JPEG") {
            return ImageFormat::IMAGE_FORMAT_JPEG;
        }
        else if (ext == ".png" || ext == ".PNG") {
            return ImageFormat::IMAGE_FORMAT_PNG;
        } 
        else if (ext == ".tga" || ext == ".TGA") {
            return ImageFormat::IMAGE_FORMAT_TGA;
        }
        else if (ext == ".tiff" || ext == ".TIFF" || ext == ".tif" || ext == ".TIF") {
            return ImageFormat::IMAGE_FORMAT_TIFF;
        }
        
        else {
            return ImageFormat::IMAGE_FORMAT_UNKNOWN;
        }
    }

    inline ImageBundle<RGB> read_image(const fs::path& filepath, bool read_alpha)
    {
        ImageFormat fmt = detect_image_format(filepath);
        auto file_data = read_file_to_buffer(filepath);
        return read_image(fmt, file_data.data(), file_data.size(), read_alpha);
    }

    inline ImageBundle<RGB> read_image(ImageFormat fmt, const unsigned char* data, std::size_t size, bool read_alpha)
    {
        if (fmt == ImageFormat::IMAGE_FORMAT_BMP) {
            return read_image_bmp(data, size, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_FITS) {
            HUIRA_THROW_ERROR("read_image - FITS format is not currently supported.");
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_HDR) {
            return read_image_hdr(data, size);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_JPEG) {
            return read_image_jpeg(data, size);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_PNG) {
            return read_image_png(data, size, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_TGA) {
            return read_image_tga(data, size, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_TIFF) {
            return read_image_tiff_rgb(data, size, read_alpha);
        }
        else {
            HUIRA_THROW_ERROR("read_image - Unsupported or unknown image format.");
        }
    }

    inline ImageBundle<float> read_image_mono(const fs::path& filepath, bool read_alpha)
    {
        ImageFormat fmt = detect_image_format(filepath);
        auto file_data = read_file_to_buffer(filepath);
        return read_image_mono(fmt, file_data.data(), file_data.size(), read_alpha);
    }

    inline ImageBundle<float> read_image_mono(ImageFormat fmt, const unsigned char* data, std::size_t size, bool read_alpha)
    {
        if (fmt == ImageFormat::IMAGE_FORMAT_BMP) {
            return read_image_bmp_mono(data, size, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_FITS) {
            HUIRA_THROW_ERROR("read_image - FITS format is not currently supported.");
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_HDR) {
            return read_image_hdr_mono(data, size);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_JPEG) {
            return read_image_jpeg_mono(data, size);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_PNG) {
            return read_image_png_mono(data, size, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_TGA) {
            return read_image_tga_mono(data, size, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_TIFF) {
            return read_image_tiff_mono(data, size, read_alpha);
        }
        else {
            HUIRA_THROW_ERROR("read_image - Unsupported or unknown image format.");
        }
    }
}
