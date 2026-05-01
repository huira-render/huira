#pragma once

#include <cstddef>
#include <filesystem>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace huira {
ImageBundle<RGB> read_image_png(const fs::path& filepath, bool read_alpha = true);
ImageBundle<RGB>
read_image_png(const unsigned char* data, std::size_t size, bool read_alpha = true);

ImageBundle<float> read_image_png_mono(const fs::path& filepath, bool read_alpha = true);
ImageBundle<float>
read_image_png_mono(const unsigned char* data, std::size_t size, bool read_alpha = true);

void write_image_png(const fs::path& filepath, const ImageBundle<RGB>& output_image);
void write_image_png(const fs::path& filepath, const ImageBundle<float>& output_image);
} // namespace huira

#include "huira_impl/images/io/png_io.ipp"
