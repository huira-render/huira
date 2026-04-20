#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "huira/core/concepts/pixel_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {

    ImageBundle<RGB> read_image_tiff_rgb(const unsigned char* data, std::size_t size, bool read_alpha = true);
    ImageBundle<RGB> read_image_tiff_rgb(const fs::path& filepath, bool read_alpha = true);

    ImageBundle<float> read_image_tiff_mono(const unsigned char* data, std::size_t size, bool read_alpha = true);
    ImageBundle<float> read_image_tiff_mono(const fs::path& filepath, bool read_alpha = true);

    void write_image_tiff(const fs::path& filepath, 
                          const Image<RGB>& image, 
                          int bit_depth = 8,
                          const std::string& description = "",
                          const std::string& artist = "");

    void write_image_tiff(const fs::path& filepath, 
                          const Image<float>& image, 
                          int bit_depth = 8,
                          const std::string& description = "",
                          const std::string& artist = "");

    template <IsSpectral TSpectral>
    void write_image_tiff(const fs::path& filepath,
                          const Image<TSpectral>& image,
                          int bit_depth = 8,
                          const std::string& description = "",
                          const std::string& artist = "");
}

#include "huira_impl/images/io/tiff_io.ipp"
