#include <cstddef>
#include <filesystem>
#include <utility>

#include "huira/core/concepts/pixel_concepts.hpp"
#include "huira/util/logger.hpp"
#include "huira/images/image.hpp"

#include "huira/images/io/png_io.hpp"
#include "huira/images/io/jpeg_io.hpp"
#include "huira/images/io/tiff_io.hpp"
#include "huira/images/io/fits_io.hpp"

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
}
