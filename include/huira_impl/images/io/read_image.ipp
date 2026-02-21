#include <cstddef>
#include <filesystem>
#include <utility>

#include "huira/core/concepts/pixel_concepts.hpp"
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

    inline std::pair<Image<RGB>, Image<float>> read_image(const fs::path& filepath, bool read_alpha)
    {
        ImageFormat fmt = detect_image_format(filepath);

        if (fmt == ImageFormat::IMAGE_FORMAT_BMP) {
            return read_image_bmp(filepath, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_FITS) {
            HUIRA_THROW_ERROR("read_image - FITS format does not support RGB images. Use read_image_mono instead: " + filepath.string());
            //auto image = read_image_fits_rgb(filepath);
            //return { std::move(image), Image<float>(0, 0) };
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_HDR) {
            auto image = read_image_hdr(filepath);
            return { std::move(image), Image<float>(0, 0) };
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_JPEG) {
            auto image = read_image_jpeg(filepath);
            return { std::move(image), Image<float>(0, 0) };
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_PNG) {
            return read_image_png(filepath, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_TGA) {
            return read_image_tga(filepath, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_TIFF) {
            return read_image_tiff_rgb(filepath, read_alpha);
        }
        else {
            HUIRA_THROW_ERROR("read_image - Unsupported or unknown image format for file: " + filepath.string());
        }
    }

    inline std::pair<Image<float>, Image<float>> read_image_mono(const fs::path& filepath, bool read_alpha)
    {
        ImageFormat fmt = detect_image_format(filepath);

        if (fmt == ImageFormat::IMAGE_FORMAT_BMP) {
            return read_image_bmp_mono(filepath, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_FITS) {
            HUIRA_THROW_ERROR("read_image_mono - FITS format is not currently supported for mono images: " + filepath.string());
            //auto image = read_image_fits_mono(filepath);
            //return { std::move(image), Image<float>(0, 0) };
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_HDR) {
            auto image = read_image_hdr_mono(filepath);
            return { std::move(image), Image<float>(0, 0) };
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_JPEG) {
            auto image = read_image_jpeg_mono(filepath);
            return { std::move(image), Image<float>(0, 0) };
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_PNG) {
            return read_image_png_mono(filepath, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_TGA) {
            return read_image_tga_mono(filepath, read_alpha);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_TIFF) {
            return read_image_tiff_mono(filepath, read_alpha);
        }
        else {
            HUIRA_THROW_ERROR("read_image_mono - Unsupported or unknown image format for file: " + filepath.string());
        }
    }
}
