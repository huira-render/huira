#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>

#include "huira/core/types.hpp"
#include "huira/images/image.hpp"
#include "huira/util/logger.hpp"

namespace fs = std::filesystem;

namespace huira {
    class DEMtoCubeMap {
    public:
        void add_dem(const fs::path& dem_path)
        {
            dem_paths_.push_back(dem_path);
        }

        void build(const fs::path& output, int resolution)
        {
            (void)output;
            (void)resolution;
            // TODO Skipping for now
        }

    private:
        std::vector<fs::path> dem_paths_;
    };

    enum CubeFace : std::uint8_t {
        POSITIVE_X = 0,
        NEGATIVE_X = 1,
        POSITIVE_Y = 2,
        NEGATIVE_Y = 3,
        POSITIVE_Z = 4,
        NEGATIVE_Z = 5
    };

    struct TileAddress {
        CubeFace face;
        int level;
        int x, y;

        TileAddress parent() const
        {

        }

        TileAddress neighbor(Direction dir) const
        {

        }

        std::pair<Vec2<float>, Vec2<float>> uv_bounds(int level_resolution) const
        {

        }

        bool operator==(const TileAddress& other) const
        {
            return x == other.x && y == other.y && level == other.level && face == other.face;
        }
    };

}

template<>
struct std::hash<huira::TileAddress> {
    std::size_t operator()(const huira::TileAddress& addr) const noexcept {
        // combine face, level, x, y
        std::size_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(addr.face));
        h ^= std::hash<int>{}(addr.level) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(addr.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(addr.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

namespace huira {

    template <IsImagePixel PixelT>
    class CubeMapTile {
    public:
        CubeMapTile(TileAddress address, Image<PixelT> data)
            : address_(address), data_(std::move(data)) {}

        const TileAddress& address() const { return address_; }
        const Image<PixelT>& data() const { return data_; }

    protected:
        TileAddress address_;
        Image<PixelT> data_;
    };

    template <IsSpectral TSpectral>
    class CubeMapHeightTile : public CubeMapTile<float> {
    public:
        std::shared_ptr<Mesh<TSpectral>> get_mesh()
        {
            if (!mesh_cache_) {
                // TODO Build mesh from height data
            }
            return mesh_cache_;
        }

    private:
        std::shared_ptr<Mesh<TSpectral>> mesh_cache_;
    };


    template <IsSpectral TSpectral>
    class CubeMapHeight {
    public:
        CubeMapHeight(const fs::path& cubemap_path)
        {
            // TODO Implement
            (void)cubemap_path;
        }

        std::vector<TileAddress> get_visible_tiles() const
        {
            // TODO Implement this for a camera.  For now, just return all
            std::vector<TileAddress> addresses;
            for (const auto& pair : tiles_) {
                addresses.push_back(pair.first);
            }
            return addresses;
        }

        std::shared_ptr<Mesh<TSpectral>> get_mesh(const TileAddress& addr) const
        {
            auto it = tiles_.find(addr);
            if (it != tiles_.end()) {
                return it->second->get_mesh();
            }
            else {
                HUIRA_THROW_ERROR("CubeMapHeight::tile - Tile not found: face=" + std::to_string(static_cast<uint8_t>(addr.face)) + ", level=" + std::to_string(addr.level) + ", x=" + std::to_string(addr.x) + ", y=" + std::to_string(addr.y));
            }
        }

    private:
        std::unordered_map<TileAddress, std::unique_ptr<CubeMapHeightTile<TSpectral>>> tiles_;
    };
}
