#pragma once

#include <cstddef>
#include <filesystem>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace huira {

ImageBundle<RGB> read_image_hdr(const unsigned char* data, std::size_t size);
ImageBundle<RGB> read_image_hdr(const fs::path& filepath);

ImageBundle<float> read_image_hdr_mono(const unsigned char* data, std::size_t size);
ImageBundle<float> read_image_hdr_mono(const fs::path& filepath);
} // namespace huira

#include "huira_impl/images/io/hdr_io.ipp"
