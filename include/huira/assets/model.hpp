#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/frame_node.hpp"
#include "huira/assets/mesh.hpp"
#include "huira/scene/scene_object.hpp"

namespace fs = std::filesystem;

namespace huira {

    template <IsSpectral TSpectral>
    class SceneView;

    template <IsSpectral TSpectral>
    class ModelLoader;

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
    class Model : public SceneObject<Model<TSpectral>, TSpectral> {
    public:
        Model() : id_(next_id_++) {}
        ~Model() = default;

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        Model(Model&&) = default;
        Model& operator=(Model&&) = default;

        const fs::path& source_path() const noexcept { return source_path_; }

        std::uint64_t id() const noexcept { return id_; }
        std::string type() const override { return "Model"; }

        void print_graph() const;

    private:
        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
        
        fs::path source_path_;
        
        std::shared_ptr<FrameNode<TSpectral>> root_node_;

        void print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const;

        // ModelLoader needs access to populate the model
        friend class ModelLoader<TSpectral>;
        friend class SceneView<TSpectral>;
    };

}

#include "huira_impl/assets/model.ipp"
