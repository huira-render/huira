#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "huira/assets/mesh.hpp"
#include "huira/assets/model.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/handles/mesh_handle.hpp"
#include "huira/scene/frame_node.hpp"
#include "huira/scene/instance.hpp"

namespace fs = std::filesystem;

namespace huira {
    /**
     * @brief Loader for 3D model files using ASSIMP.
     *
     * Provides static methods to load models from disk and convert them into huira scene objects.
     * Handles mesh conversion, node hierarchy, and basic transform extraction.
     *
     * @tparam TSpectral Spectral type (must satisfy IsSpectral concept)
     */
    template <IsSpectral TSpectral>
    class ModelLoader {
    public:
        static constexpr unsigned int DEFAULT_POST_PROCESS_FLAGS =
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_FlipUVs |
            aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType;
        
        static std::shared_ptr<Model<TSpectral>> load(
            Scene<TSpectral>& scene,
            const fs::path& file_path,
            std::string name,
            unsigned int post_process_flags = DEFAULT_POST_PROCESS_FLAGS
        );

    private:
        struct LoadContext {
            const aiScene* ai_scene;
            Model<TSpectral>* model;

            // Maps ASSIMP mesh index to our Mesh pointer
            std::unordered_map<unsigned int, MeshHandle<TSpectral>> mesh_map;

            Scene<TSpectral>* scene;

            // TODO: material/texture support
            // std::unordered_map<unsigned int, Material<TSpectral>*> material_map;
            // std::unordered_map<std::string, Texture*> texture_map;
        };

        
        static void process_meshes_(LoadContext& ctx);

        static MeshHandle<TSpectral> convert_mesh_(const aiMesh* ai_mesh, LoadContext& ctx);

        static void process_node_(const aiNode* ai_node, FrameNode<TSpectral>* parent_frame, LoadContext& ctx);

        static Transform<double> convert_transform_(const aiMatrix4x4& ai_matrix);

        static Vec3<double> convert_vec3_(const aiVector3D& ai_vec);
    };

}

#include "huira_impl/assets/io/model_loader.ipp"
