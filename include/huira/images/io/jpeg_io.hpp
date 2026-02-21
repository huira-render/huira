#pragma once

#include <filesystem>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace huira {
    Image<RGB> read_image_jpeg(const fs::path& filepath);

    Image<float> read_image_jpeg_mono(const fs::path& filepath);

    void write_image_jpeg(const fs::path& filepath, const Image<RGB>& image, int quality = 95);

    void write_image_jpeg(const fs::path& filepath, const Image<float>& image, int quality = 95);
}

#include "huira_impl/images/io/jpeg_io.ipp"
