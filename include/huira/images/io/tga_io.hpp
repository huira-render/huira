#pragma once

#include <cstddef>
#include <filesystem>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace huira {

ImageBundle<RGB>
read_image_tga(const unsigned char* data, std::size_t size, bool read_alpha = true);
ImageBundle<RGB> read_image_tga(const fs::path& filepath, bool read_alpha = true);

ImageBundle<float>
read_image_tga_mono(const unsigned char* data, std::size_t size, bool read_alpha = true);
ImageBundle<float> read_image_tga_mono(const fs::path& filepath, bool read_alpha = true);
} // namespace huira

#include "huira_impl/images/io/tga_io.ipp"
