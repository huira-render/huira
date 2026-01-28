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
    std::pair<Image<T>, Image<float>> read_image_tiff(const fs::path& filepath)
    {
        HUIRA_THROW_ERROR("TIFF reading not yet implemented: " + filepath.string());
    }
}
