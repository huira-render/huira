#include <cstddef>
#include <filesystem>
#include <utility>

#include "huira/detail/concepts/pixel_concepts.hpp"
#include "huira/detail/logger.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/convert_pixel.hpp"

namespace fs = std::filesystem;

namespace huira {
    template <IsImagePixel T>
    std::pair<Image<T>, Image<float>> read_image_fits(const fs::path& filepath)
    {
        HUIRA_THROW_ERROR("FITS reading not yet implemented: " + filepath.string());

        //bool has_alpha = false;
        //std::size_t width = 0;
        //std::size_t height = 0;
        //
        //// Create output images
        //Image<T> image(static_cast<std::size_t>(width),
        //    static_cast<std::size_t>(height));
        //Image<float> alpha_image(0, 0);
        //
        //if (has_alpha) {
        //    alpha_image = Image<float>(static_cast<std::size_t>(width),
        //        static_cast<std::size_t>(height),
        //        1.0f);
        //}
        //
        //return { std::move(image), std::move(alpha_image) };
    }
}
