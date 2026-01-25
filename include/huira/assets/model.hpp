#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/scene_graph/frame_node.hpp"
#include "huira/assets/mesh.hpp"

namespace huira {

    namespace fs = std::filesystem;

    /**
     * @brief Represents a loaded 3D model with its own isolated scene graph.
     *
     * Model contains:
     * - A root FrameNode representing the model's local coordinate system
     * - A collection of Meshes owned by this model
     * - The original file path for reference
     *
     * The Model's scene graph is disconnected from the main Scene graph.
     * To place a Model into the Scene, use Instance<TSpectral> with a Model* reference.
     *
     * Usage:
     *   auto model_handle = scene.load_model("path/to/model.obj");
     *   auto instance = some_frame_handle.new_instance(model_handle.get());
     */
    template <IsSpectral TSpectral>
    class Model {
    public:
        Model() = default;
        ~Model() = default;

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        Model(Model&&) = default;
        Model& operator=(Model&&) = default;

        std::uint64_t id() const noexcept { return id_; }
        
        FrameNode<TSpectral>* root() const noexcept { return root_node_.get(); }
        const std::vector<std::shared_ptr<Mesh<TSpectral>>>& meshes() const noexcept { return meshes_; }
        const fs::path& source_path() const noexcept { return source_path_; }
        
        const std::string& name() const noexcept { return name_; }
        void set_name(const std::string& name) { name_ = name; }

        std::string get_info() const {
            return "Model[id=" + std::to_string(id_) + ", name=" + name_ + 
                   ", meshes=" + std::to_string(meshes_.size()) + "]";
        }
        
        void print_graph() const;

    private:
        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;

        std::string name_;
        fs::path source_path_;
        
        std::shared_ptr<FrameNode<TSpectral>> root_node_;

        // Meshes owned by this model (not shared with Scene's mesh collection)
        std::vector<std::shared_ptr<Mesh<TSpectral>>> meshes_;

        void print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const;

        // ModelLoader needs access to populate the model
        template <IsSpectral U>
        friend class ModelLoader;
    };

}

#include "huira_impl/assets/model.ipp"
