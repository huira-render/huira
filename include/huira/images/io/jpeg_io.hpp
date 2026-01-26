#pragma once

#include <filesystem>
#include <utility>

#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {
    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_jpeg(const fs::path& filepath);
}

#include "huira_impl/images/io/jpeg_io.ipp"
