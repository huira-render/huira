#pragma once

#include <filesystem>
#include <utility>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace fs = std::filesystem;

namespace huira {
    enum ImageFormat {
        IMAGE_FORMAT_PNG,
        IMAGE_FORMAT_JPEG,
        IMAGE_FORMAT_UNKNOWN
    };

    ImageFormat detect_image_format(const fs::path& filepath);

    std::pair<Image<RGB>, Image<float>> read_image(const fs::path& filepath);

    std::pair<Image<float>, Image<float>> read_image_mono(const fs::path& filepath);
}

#include "huira_impl/images/io/read_image.ipp"
