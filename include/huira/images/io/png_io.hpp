#pragma once

#include <filesystem>
#include <utility>

#include <png.h>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/images/image.hpp"

namespace huira {
    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_png(const fs::path& filepath);

    enum class PngColorSpace {
        SRGB,        // sRGB chunk present, or assumed default
        LINEAR,      // gAMA = 1.0
        GAMMA,       // gAMA chunk with custom gamma
        ICC_PROFILE, // iCCP chunk present
        UNKNOWN      // Unknown or unsupported
    };

    struct PngColorInfo {
        PngColorSpace space = PngColorSpace::SRGB;
        double gamma = 2.2;  // Only used if space == GAMMA
        // TODO add ICC profile data
    };

    inline PngColorInfo detect_png_color_space(png_structp png_str, png_infop info_ptr);

    template <IsNumeric T>
    float linearize_png(T encoded, const PngColorInfo& color_info);

    template <IsNumeric T>
    Vec3<float> linearize_png(T encoded1, T encoded2, T encoded3, const PngColorInfo& color_info);
}

#include "huira_impl/images/io/png_io.ipp"
