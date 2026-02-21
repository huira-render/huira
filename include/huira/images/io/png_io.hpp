#pragma once

#include <filesystem>
#include <utility>

#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

namespace huira {
    /**
     * @brief Reads a PNG image file and returns linear RGB + alpha data.
     *
     * Always returns an Image<Vec3<float>> in linear color space.
     * Grayscale PNGs are promoted to RGB (equal values in all channels).
     * If the PNG has an alpha channel, the second image contains it; otherwise
     * the second image is empty (0x0).
     *
     * @param filepath Path to the PNG file to read
     * @return A pair containing the linear RGB image and an optional alpha image.
     * @throws std::runtime_error if the file cannot be opened, is not a valid PNG,
     *         or if any error occurs during reading
     */
    std::pair<Image<RGB>, Image<float>> read_image_png(const fs::path& filepath, bool read_alpha = true);

    std::pair<Image<float>, Image<float>> read_image_png_mono(const fs::path& filepath, bool read_alpha = true);


    void write_image_png(const fs::path& filepath, const Image<RGB>& image, int bit_depth = 8);

    void write_image_png(const fs::path& filepath, const Image<float>& image, int bit_depth = 8);

    
    void write_image_png(const fs::path& filepath, const Image<RGB>& image,
        const Image<float>& alpha, int bit_depth = 8);


    void write_image_png(const fs::path& filepath, const Image<float>& image,
        const Image<float>& alpha, int bit_depth = 8);

}

#include "huira_impl/images/io/png_io.ipp"
