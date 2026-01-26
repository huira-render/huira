#pragma once

#include <filesystem>
#include <utility>

#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/images/image.hpp"

namespace fs = std::filesystem;

namespace huira {
    enum ImageFormat {
        IMAGE_FORMAT_PNG,
        IMAGE_FORMAT_JPEG,
        IMAGE_FORMAT_TIFF,
        IMAGE_FORMAT_FITS,
        IMAGE_FORMAT_UNKNOWN
    };

    ImageFormat detect_image_format(const fs::path& filepath);

    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image(const fs::path& filepath);

    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_png(const fs::path& filepath);

    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_jpeg(const fs::path& filepath);

    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_tiff(const fs::path& filepath);

    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_fits(const fs::path& filepath);
}

#include "huira_impl/images/io/read_image.ipp"
