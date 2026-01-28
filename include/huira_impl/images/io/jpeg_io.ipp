#include <cstddef>
#include <filesystem>
#include <utility>

#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/detail/logger.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/convert_pixel.hpp"

namespace fs = std::filesystem;

namespace huira {
    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_jpeg(const fs::path& filepath)
    {
        (void)filepath;
        HUIRA_THROW_ERROR("JPEG reading not yet implemented: " + filepath.string());
    }

    template <IsImagePixel T>
    void write_image_jpeg(const fs::path& filepath, const Image<T>& image)
    {
        (void)filepath;
        (void)image;
        HUIRA_THROW_ERROR("JPEG writing not yet implemented: " + filepath.string());
    }
}
