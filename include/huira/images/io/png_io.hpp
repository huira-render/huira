#pragma once

#include <cstddef>
#include <filesystem>
#include <utility>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace huira {

    std::pair<Image<RGB>, Image<float>> read_image_png(const unsigned char* data, std::size_t size, bool read_alpha = true);
    std::pair<Image<RGB>, Image<float>> read_image_png(const fs::path& filepath, bool read_alpha = true);

    std::pair<Image<float>, Image<float>> read_image_png_mono(const unsigned char* data, std::size_t size, bool read_alpha = true);
    std::pair<Image<float>, Image<float>> read_image_png_mono(const fs::path& filepath, bool read_alpha = true);

    void write_image_png(const fs::path& filepath, const Image<RGB>& image, int bit_depth = 8);
    void write_image_png(const fs::path& filepath, const Image<float>& image, int bit_depth = 8);
    void write_image_png(const fs::path& filepath, const Image<RGB>& image, const Image<float>& alpha, int bit_depth = 8);
    void write_image_png(const fs::path& filepath, const Image<float>& image, const Image<float>& alpha, int bit_depth = 8);
}

#include "huira_impl/images/io/png_io.ipp"
