#pragma once

#include <filesystem>
#include <utility>

#include "huira/core/concepts/pixel_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {
    std::pair<Image<RGB>, Image<float>> read_image_tiff_rgb(const fs::path& filepath, bool read_alpha = true);
    std::pair<Image<float>, Image<float>> read_image_tiff_mono(const fs::path& filepath, bool read_alpha = true);
}

#include "huira_impl/images/io/tiff_io.ipp"
