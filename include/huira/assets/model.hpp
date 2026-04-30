#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "huira/geometry/mesh.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/handles/materials/material_handle.hpp"
#include "huira/handles/materials/bsdf_handle.hpp"
#include "huira/scene/frame_node.hpp"
#include "huira/scene/scene_object.hpp"

namespace fs = std::filesystem;

namespace huira {

    template <IsSpectral TSpectral>
    class SceneView;

    template <IsSpectral TSpectral>
    class ModelLoader;

    /**
     * @brief Represents a loaded 3D model with its own isolated detached graph.
     *
     * Model contains a root FrameNode representing the model's local coordinate system,
     * a collection of Meshes owned by this model, and the original file path for reference.
     * The Model's scene graph is disconnected from the main Scene graph. To place a Model
     * into the Scene, use Instance with a Model reference.
     * 
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class Model : public SceneObject<Model<TSpectral>> {
    public:
        Model() = default;
        ~Model() override { HUIRA_LOG_INFO("Model::~Model()"); }

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        Model(Model&&) = default;
        Model& operator=(Model&&) = default;

        const fs::path& source_path() const noexcept { return source_path_; }

        std::string type() const override { return "Model"; }

        void print_graph() const;

        MaterialHandle<TSpectral> get_material_by_id(std::uint64_t material_id) const;
        
        void set_all_bsdfs(const BSDFHandle<TSpectral>& bsdf_handle);

    private:
        
        fs::path source_path_;
        
        std::shared_ptr<FrameNode<TSpectral>> root_node_;

        void print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const;
        
        friend class ModelLoader<TSpectral>;
        friend class SceneView<TSpectral>;
    };

}

#include "huira_impl/assets/model.ipp"
