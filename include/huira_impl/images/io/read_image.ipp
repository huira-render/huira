#include <cstddef>
#include <filesystem>
#include <utility>

#include "huira/core/concepts/pixel_concepts.hpp"
#include "huira/util/logger.hpp"
#include "huira/images/image.hpp"

#include "huira/images/io/png_io.hpp"
#include "huira/images/io/jpeg_io.hpp"

namespace fs = std::filesystem;

namespace huira {
    inline ImageFormat detect_image_format(const fs::path& filepath)
    {
        // TODO More robust format detection (e.g., magic numbers)
        auto ext = filepath.extension().string();
        if (ext == ".png" || ext == ".PNG") {
            return ImageFormat::IMAGE_FORMAT_PNG;
        }
        else if (ext == ".jpg" || ext == ".jpeg" || ext == ".JPG" || ext == ".JPEG") {
            return ImageFormat::IMAGE_FORMAT_JPEG;
        }
        else {
            return ImageFormat::IMAGE_FORMAT_UNKNOWN;
        }
    }

    inline std::pair<Image<RGB>, Image<float>> read_image(const fs::path& filepath)
    {
        ImageFormat fmt = detect_image_format(filepath);

        if (fmt == ImageFormat::IMAGE_FORMAT_PNG) {
            return read_image_png(filepath);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_JPEG) {
            auto image = read_image_jpeg(filepath);
            return { std::move(image), Image<float>(0, 0) };
        }
        else {
            HUIRA_THROW_ERROR("Unsupported or unknown image format for file: " + filepath.string());
        }
    }

    inline std::pair<Image<float>, Image<float>> read_image_mono(const fs::path& filepath)
    {
        ImageFormat fmt = detect_image_format(filepath);
        if (fmt == ImageFormat::IMAGE_FORMAT_PNG) {
            return read_image_png_mono(filepath);
        }
        else if (fmt == ImageFormat::IMAGE_FORMAT_JPEG) {
            auto image = read_image_jpeg_mono(filepath);
            return { std::move(image), Image<float>(0, 0) };
        }
        else {
            HUIRA_THROW_ERROR("Unsupported or unknown image format for file: " + filepath.string());
        }
    }
}
