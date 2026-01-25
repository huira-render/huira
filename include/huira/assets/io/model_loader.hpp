#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/assets/model.hpp"
#include "huira/scene_graph/frame_node.hpp"
#include "huira/scene_graph/instance.hpp"
#include "huira/assets/mesh.hpp"
#include "huira/core/types.hpp"

namespace fs = std::filesystem;

namespace huira {
    template <IsSpectral TSpectral>
    class ModelLoader {
    public:
        static constexpr unsigned int DEFAULT_POST_PROCESS_FLAGS =
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_FlipUVs |
            aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType;
        
        static std::tuple<
            std::shared_ptr<Model<TSpectral>>,
            std::vector<std::shared_ptr<Mesh<TSpectral>>>
        > load(
            const fs::path& file_path,
            unsigned int post_process_flags = DEFAULT_POST_PROCESS_FLAGS
        );

    private:
        struct LoadContext {
            const aiScene* ai_scene;
            Model<TSpectral>* model;
            std::vector<std::shared_ptr<Mesh<TSpectral>>> meshes;

            // Maps ASSIMP mesh index to our Mesh pointer
            std::unordered_map<unsigned int, Mesh<TSpectral>*> mesh_map;

            // TODO: material/texture support
            // std::unordered_map<unsigned int, Material<TSpectral>*> material_map;
            // std::unordered_map<std::string, Texture*> texture_map;
        };

        
        static void process_meshes_(LoadContext& ctx);

        static std::shared_ptr<Mesh<TSpectral>> convert_mesh_(const aiMesh* ai_mesh, LoadContext& ctx);

        static void process_node_(const aiNode* ai_node, FrameNode<TSpectral>* parent_frame, LoadContext& ctx);

        static Transform<double> convert_transform_(const aiMatrix4x4& ai_matrix);

        static Vec3<double> convert_vec3_(const aiVector3D& ai_vec);
    };

}

#include "huira_impl/assets/io/model_loader.ipp"
