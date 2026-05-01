#pragma once

#include <cstddef>
#include <filesystem>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace huira {

ImageBundle<RGB> read_image_jpeg(const unsigned char* data, std::size_t size);
ImageBundle<RGB> read_image_jpeg(const fs::path& filepath);

ImageBundle<float> read_image_jpeg_mono(const unsigned char* data, std::size_t size);
ImageBundle<float> read_image_jpeg_mono(const fs::path& filepath);

void write_image_jpeg(const fs::path& filepath,
                      const ImageBundle<RGB>& output_image,
                      int quality = 95);
void write_image_jpeg(const fs::path& filepath,
                      const ImageBundle<float>& output_image,
                      int quality = 95);
} // namespace huira

#include "huira_impl/images/io/jpeg_io.ipp"
