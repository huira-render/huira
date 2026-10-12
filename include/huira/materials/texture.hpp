#pragma once

#include <cmath>
#include <filesystem>
#include <string>
#include <type_traits>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {

template <IsSpectral TSpectral>
class Scene;

/**
 * @brief Scene-managed wrapper around an Image.
 *
 * Texture is a SceneObject that provides scene ownership, naming, and
 * lifetime management for Image data. It exists in the scene management
 * layer and is accessed by users through TextureHandle.
 *
 * During rendering, Materials hold raw Image<TPixel>* pointers for
 * direct access without indirection through this wrapper. Texture is
 * not involved in the rendering hot path.
 *
 * @tparam TPixel The pixel type of the underlying Image (e.g., TSpectral,
 *                float, Vec3<float>)
 */
template <typename TPixel>
class Texture : public SceneObject<Texture<TPixel>> {
  public:
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    /// @throws std::runtime_error if the image has no pixels, or a pixel is not finite (or for a
    /// spectral one, negative).
    explicit Texture(Image<TPixel>&& image)
        : image_{std::make_shared<Image<TPixel>>(std::move(image))}
    {
        check_(*image_);
    }

    /// @throws std::runtime_error if the value is not finite (or for a spectral one, negative).
    explicit Texture(const TPixel& constant_value)
        : image_{std::make_shared<Image<TPixel>>(1, 1, constant_value)}
    {
        check_(*image_);
    }

    /// Throws unless an image has pixels, each finite, and for a spectrum, non-negative, as light
    /// and albedo are; textures are sampled without checking, so a bad pixel would spread NaN.
    static void check_(const Image<TPixel>& image)
    {
        if (image.width() < 1 || image.height() < 1) {
            HUIRA_THROW_ERROR("Texture - The image must have at least one pixel: " +
                              std::to_string(image.width()) + " x " +
                              std::to_string(image.height()));
        }
        for (std::size_t i = 0; i < image.size(); ++i) {
            bool good = true;
            if constexpr (IsSpectral<TPixel>) {
                good = image[i].valid();
            } else if constexpr (std::is_floating_point_v<TPixel>) {
                good = std::isfinite(image[i]);
            } else {
                for (int k = 0; k < TPixel::length(); ++k) {
                    good = good && std::isfinite(image[i][k]);
                }
            }
            if (!good) {
                HUIRA_THROW_ERROR(
                    "Texture - Pixel " +
                    std::to_string(i % static_cast<std::size_t>(image.width())) + ", " +
                    std::to_string(i / static_cast<std::size_t>(image.width())) + " is not finite" +
                    (IsSpectral<TPixel> ? " and non-negative" : ""));
            }
        }
    }

    [[nodiscard]] std::shared_ptr<Image<TPixel>> shared_image() const { return image_; }

    [[nodiscard]] Resolution resolution() const noexcept { return image_->resolution(); }

    [[nodiscard]] std::string type() const override { return "Texture"; }

  private:
    std::shared_ptr<Image<TPixel>> image_;
};

} // namespace huira
