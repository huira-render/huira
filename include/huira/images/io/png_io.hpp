#pragma once

#include <filesystem>
#include <utility>

#include "huira/core/types.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/pixel_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {
    template <IsNonSpectralPixel T>
    std::pair<Image<T>, Image<float>> read_image_png(const fs::path& filepath);


    template <IsNonSpectralPixel T>
    void write_image_png(const fs::path& filepath, const Image<T>& image, int bit_depth = 8);

    template <IsNonSpectralPixel T>
    void write_image_png(const fs::path& filepath, const Image<T>& image, const Image<float>& albedo, int bit_depth = 8);
}

#include "huira_impl/images/io/png_io.ipp"
