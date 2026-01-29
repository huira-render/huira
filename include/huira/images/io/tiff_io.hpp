#pragma once

#include <filesystem>
#include <utility>

#include "huira/core/concepts/pixel_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {
    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_tiff(const fs::path& filepath);
}

#include "huira_impl/images/io/tiff_io.ipp"
