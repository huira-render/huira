#pragma once

#include <cstddef>
#include <filesystem>
#include <utility>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace huira {

    std::pair<Image<RGB>, Image<float>> read_image_bmp(const unsigned char* data, std::size_t size, bool read_alpha = true);
    std::pair<Image<RGB>, Image<float>> read_image_bmp(const fs::path& filepath, bool read_alpha = true);

    std::pair<Image<float>, Image<float>> read_image_bmp_mono(const unsigned char* data, std::size_t size, bool read_alpha = true);
    std::pair<Image<float>, Image<float>> read_image_bmp_mono(const fs::path& filepath, bool read_alpha = true);
}

#include "huira_impl/images/io/bmp_io.ipp"
