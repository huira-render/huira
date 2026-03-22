#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "huira/core/types.hpp"
#include "huira/images/image.hpp"
#include "huira/util/logger.hpp"

#include "huira/scene/scene_object.hpp"

namespace fs = std::filesystem;

namespace huira {
    template <IsFloatingPoint T>
    class OBB {
    public:
        OBB() = default;
        OBB(const Vec3<T>& center, const Vec3<T>& half_extents, const Mat3<T>& rotation)
            : center_(center), half_extents_(half_extents), rotation_(rotation) {}
        const Vec3<T>& center() const { return center_; }
        const Vec3<T>& half_extents() const { return half_extents_; }
        const Mat3<T>& rotation() const { return rotation_; }

    private:
        Vec3<T> center_;
        Vec3<T> half_extents_;
        Rotation<T> rotation_;
    };

    struct TileAddress {
        std::uint16_t level;
        std::uint16_t x, y;
        bool operator==(const TileAddress& other) const {
            return level == other.level && x == other.x && y == other.y;
        }
    };
}

namespace std {
    template <>
    struct hash<huira::TileAddress> {
        std::size_t operator()(const huira::TileAddress& addr) const {
            return (static_cast<std::size_t>(addr.level) << 32) | (static_cast<std::size_t>(addr.x) << 16) | addr.y;
        }
    };
}

namespace huira{

    struct QuadtreeNode {
        QuadtreeNode() = default;
        QuadtreeNode(const QuadtreeNode&) = delete;
        QuadtreeNode& operator=(const QuadtreeNode&) = delete;

        // Each node corresponds to a tile
        std::uint64_t file_id;
        std::uint64_t byte_offset;
        OBB<double> bounding_box;
        Vec3<double> normal;
        double normal_cone_angle;

        TileAddress tile_address;

        std::array<std::unique_ptr<QuadtreeNode>, 4> children;
    };

    template <IsSpectral TSpectral>
    class CubeMap : public SceneObject<CubeMap<TSpectral>> {
    public:
        CubeMap(const fs::path& cubemap_path) : id_(next_id_++)
        {
            // TODO Load the cubemap into QuadtreeNode
            // Mapping of file_path to file_id
            (void)cubemap_path; // Placeholder to suppress unused parameter warning
        }

        std::pair<std::vector<TileAddress>, std::vector<TileAddress>> get_visible_tile_updates(CameraModel<TSpectral> camera, Transform<double> camera_transform)
        {
            // Reset the lists of loaded/unloaded tiles for this frame
            this->unloaded_tiles_.clear();
            this->newly_loaded_tiles_.clear();

            for (std::size_t i = 0; i < face_trees_.size(); ++i) {
                // We use i as the face index
                update_meshes_(camera, camera_transform, face_trees_[i], i);
            }

            return { newly_loaded_tiles_, unloaded_tiles_ };
        }

        std::uint64_t id() const override { return id_; }
        std::string type() const override { return "CubeMap"; }

    private:

        std::array<QuadtreeNode, 6> face_trees_;
        std::unordered_map<std::uint64_t, fs::path> file_id_to_path_;

        // Cached loaded data
        std::unordered_map<TileAddress, std::shared_ptr<Mesh<TSpectral>>> loaded_meshes_;
        std::vector<TileAddress> newly_loaded_tiles_;
        std::vector<TileAddress> unloaded_tiles_;

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;

        void update_meshes_(const CameraModel<TSpectral>& camera, const Transform<double>& camera_transform, const QuadtreeNode& node, std::size_t face_index)
        {
            // TODO Determine if the node is visible
            bool visible = true; // Placeholder
            if (visible) {
                // TODO Determine if the node's LOD is suitable for rendering
                bool suitable_lod = true; // Placeholder
                if (suitable_lod) {
                    // Load it if we haven't already:
                    if (loaded_meshes_.find(node.tile_address) == loaded_meshes_.end()) {
                        loaded_meshes_[node.tile_address] = make_mesh_(node);
                        newly_loaded_tiles_.push_back(node.tile_address);
                    }

                    // Remove children meshes if they exist
                    for (const auto& child : node.children) {
                        if (child) {
                            unload_below_(*child);
                        }
                    }
                }
                else {
                    // Need to go deeper into the tree
                    for (const auto& child : node.children) {
                        if (child) {
                            update_meshes_(camera, camera_transform, *child, face_index);
                        }
                    }
                }
            }
            else {
                unload_below_(node);
            }
        }

        void unload_below_(const QuadtreeNode& node)
        {
            // Recursively remove all child meshes for this node
            for (const auto& child : node.children) {
                if (child) {
                    unload_below_(*child);
                    unloaded_tiles_.push_back(child->tile_address);
                }
            }
            // Remove the mesh for this node if it exists
            loaded_meshes_.erase(node.tile_address);
        }

        std::shared_ptr<Mesh<TSpectral>> make_mesh_(const QuadtreeNode& node)
        {
            if (file_id_to_path_.find(node.file_id) == file_id_to_path_.end()) {
                HUIRA_THROW_ERROR("CubeMap::make_mesh_ - File ID " + std::to_string(node.file_id) + "not found in cubemap file mapping");
            }
            const fs::path& path = file_id_to_path_[node.file_id];

            Image<float> height_image = load_heights(path, node.tile_address);

            // TODO create the mesh vertex data from the height image and the tile address

            VertexBuffer<TSpectral> vertex_buffer;
            IndexBuffer index_buffer;

            return std::make_shared<Mesh<TSpectral>>(vertex_buffer, index_buffer);
        }

        Image<float> load_heights(const fs::path& path, const TileAddress& tile_address)
        {
            // TODO Load the height data from the file at the byte offset.
            (void)path;
            (void)tile_address;
            return Image<float>();
        }
    };
}
