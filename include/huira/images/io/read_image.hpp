#pragma once

#include <filesystem>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace fs = std::filesystem;

namespace huira {
    enum ImageFormat {
        IMAGE_FORMAT_PNG,
        IMAGE_FORMAT_JPEG,
        IMAGE_FORMAT_BMP,
        IMAGE_FORMAT_TGA,
        IMAGE_FORMAT_HDR,
        IMAGE_FORMAT_TIFF,
        IMAGE_FORMAT_FITS,
        IMAGE_FORMAT_UNKNOWN
    };

    ImageFormat detect_image_format(const fs::path& filepath);

    ImageBundle<RGB> read_image(const fs::path& filepath, bool read_alpha = true);
    ImageBundle<RGB> read_image(ImageFormat fmt, const unsigned char* data, std::size_t size, bool read_alpha = true);

    ImageBundle<float> read_image_mono(const fs::path& filepath, bool read_alpha = true);
    ImageBundle<float> read_image_mono(ImageFormat fmt, const unsigned char* data, std::size_t size, bool read_alpha = true);
}

#include "huira_impl/images/io/read_image.ipp"
