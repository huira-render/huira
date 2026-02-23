#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "huira/images/image.hpp"
#include "huira/scene/scene_object.hpp"

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

        explicit Texture(Image<TPixel>&& image)
            : image_{ std::move(image) }
            , id_{ next_id_++ }
        {
        }

        explicit Texture(const TPixel& constant_value)
            : image_{ 1, 1, constant_value }
            , id_{ next_id_++ }
        {
        }

        [[nodiscard]] Image<TPixel>* image() noexcept { return &image_; }
        [[nodiscard]] const Image<TPixel>* image() const noexcept { return &image_; }

        [[nodiscard]] Resolution resolution() const noexcept { return image_.resolution(); }
        
        [[nodiscard]] std::uint64_t id() const override { return id_; }
        [[nodiscard]] std::string type() const override { return "Texture"; }

    private:
        Image<TPixel> image_;

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
    };

}
