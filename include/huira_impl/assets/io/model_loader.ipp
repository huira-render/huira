#include <cmath>
#include <iostream>
#include <tuple>

#include "huira/core/types.hpp"
#include "huira/core/rotation.hpp"
#include "huira/detail/logger.hpp"

namespace huira {
    /**
     * @brief Load a 3D model from file.
     *
     * @param file_path Path to the model file
     * @param post_process_flags ASSIMP post-processing flags (optional)
     * @return A unique_ptr to the loaded Model
     * @throws ModelLoadException if loading fails
     */
    template <IsSpectral TSpectral>
    std::tuple<
        std::shared_ptr<Model<TSpectral>>,
        std::vector<std::shared_ptr<Mesh<TSpectral>>>
        > ModelLoader<TSpectral>::load(
        const fs::path& file_path,
        unsigned int post_process_flags
    ) {
        // Validate file exists
        if (!fs::exists(file_path)) {
            HUIRA_THROW_ERROR("ModelLoader: File not found: " + file_path.string());
        }

        // Create ASSIMP importer
        Assimp::Importer importer;

        // Load the scene
        const aiScene* ai_scene = importer.ReadFile(
            file_path.string(),
            post_process_flags
        );

        // Check for errors
        if (!ai_scene || ai_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !ai_scene->mRootNode) {
            HUIRA_THROW_ERROR(
                "ASSIMP error loading '" + file_path.string() + "': " +
                std::string(importer.GetErrorString())
            );
        }

        HUIRA_LOG_INFO("Loading model: " + file_path.string());
        HUIRA_LOG_INFO("  - Meshes: " + std::to_string(ai_scene->mNumMeshes));
        HUIRA_LOG_INFO("  - Materials: " + std::to_string(ai_scene->mNumMaterials));
        HUIRA_LOG_INFO("  - Textures: " + std::to_string(ai_scene->mNumTextures));

        // Create the Model object
        auto model = std::make_shared<Model<TSpectral>>();
        model->id_ = Model<TSpectral>::next_id_++;
        model->source_path_ = file_path;
        model->name_ = file_path.stem().string();

        // Create the root node for the model's scene graph
        // Note: We pass nullptr for Scene* since this is a disconnected graph
        // This means SPICE-based transforms won't work within models
        model->root_node_ = std::make_shared<FrameNode<TSpectral>>(nullptr);

        // Setup loading context
        LoadContext ctx;
        ctx.ai_scene = ai_scene;
        ctx.model = model.get();

        // TODO: MATERIAL AND TEXTURE LOADING
        // // Load embedded textures
        // for (unsigned int i = 0; i < ai_scene->mNumTextures; ++i) {
        //     const aiTexture* ai_tex = ai_scene->mTextures[i];
        //     auto texture = convert_embedded_texture_(ai_tex);
        //     model->textures_.push_back(texture);
        //     ctx.texture_map[ai_tex->mFilename.C_Str()] = texture.get();
        // }
        //
        // // Load materials
        // for (unsigned int i = 0; i < ai_scene->mNumMaterials; ++i) {
        //     const aiMaterial* ai_mat = ai_scene->mMaterials[i];
        //     auto material = convert_material_(ai_mat, ctx);
        //     model->materials_.push_back(material);
        //     ctx.material_map[i] = material.get();
        // }

        // Process all meshes first (creates Mesh objects)
        process_meshes_(ctx);

        // Process the node hierarchy (creates FrameNodes and Instances)
        // The root ASSIMP node's transform is applied to our root node
        Transform<double> root_transform = convert_transform_(ai_scene->mRootNode->mTransformation);
        model->root_node_->set_position(root_transform.position);
        model->root_node_->set_rotation(root_transform.rotation);
        model->root_node_->set_scale(root_transform.scale);

        // Process children of the root node
        for (unsigned int i = 0; i < ai_scene->mRootNode->mNumChildren; ++i) {
            process_node_(ai_scene->mRootNode->mChildren[i], model->root_node_.get(), ctx);
        }

        // Also handle any meshes directly attached to the root node
        for (unsigned int i = 0; i < ai_scene->mRootNode->mNumMeshes; ++i) {
            unsigned int mesh_index = ai_scene->mRootNode->mMeshes[i];
            Mesh<TSpectral>* mesh_ptr = ctx.mesh_map[mesh_index];
            model->root_node_->new_instance(mesh_ptr);
        }

        HUIRA_LOG_INFO("Model loaded successfully: " + model->get_info());

        return  { model, ctx.meshes };
    }

    /**
     * @brief Process all meshes in the ASSIMP scene and create huira Mesh objects.
     */
    template <IsSpectral TSpectral>
    void ModelLoader<TSpectral>::process_meshes_(LoadContext& ctx) {
        for (unsigned int i = 0; i < ctx.ai_scene->mNumMeshes; ++i) {
            const aiMesh* ai_mesh = ctx.ai_scene->mMeshes[i];

            auto mesh = convert_mesh_(ai_mesh, ctx);
            ctx.mesh_map[i] = mesh.get();
            ctx.meshes.push_back(std::move(mesh));

            HUIRA_LOG_DEBUG("ModelLoader - Processed mesh " + std::to_string(i) + ": " +
                           std::string(ai_mesh->mName.C_Str()) +
                           " (" + std::to_string(ai_mesh->mNumVertices) + " vertices, " +
                           std::to_string(ai_mesh->mNumFaces) + " faces)");
        }
    }

    /**
     * @brief Convert a single ASSIMP mesh to a huira Mesh.
     */
    template <IsSpectral TSpectral>
    std::shared_ptr<Mesh<TSpectral>> ModelLoader<TSpectral>::convert_mesh_(
        const aiMesh* ai_mesh,
        LoadContext& ctx
    ) {
        (void)ctx; // Will be used for materials

        // Build index buffer
        IndexBuffer indices;
        indices.reserve(ai_mesh->mNumFaces * 3);

        for (unsigned int i = 0; i < ai_mesh->mNumFaces; ++i) {
            const aiFace& face = ai_mesh->mFaces[i];
            // After triangulation, all faces should have 3 indices
            if (face.mNumIndices == 3) {
                indices.push_back(face.mIndices[0]);
                indices.push_back(face.mIndices[1]);
                indices.push_back(face.mIndices[2]);
            }
            // Skip non-triangle primitives (points, lines) that might remain
        }

        // Build vertex buffer
        VertexBuffer<TSpectral> vertices;
        vertices.reserve(ai_mesh->mNumVertices);

        for (unsigned int i = 0; i < ai_mesh->mNumVertices; ++i) {
            Vertex<TSpectral> vertex;

            // Position (always present)
            vertex.position = convert_vec3_(ai_mesh->mVertices[i]);

            // Normal (should be present after aiProcess_GenNormals)
            if (ai_mesh->HasNormals()) {
                vertex.normal = convert_vec3_(ai_mesh->mNormals[i]);
            } else {
                vertex.normal = Vec3<double>{0.0, 1.0, 0.0}; // Default up
            }

            // Texture coordinates (first UV channel only for now)
            if (ai_mesh->HasTextureCoords(0)) {
                // ASSIMP stores UVs as 3D vectors; we only need the first two components
                vertex.uv = Vec2<double>{
                    static_cast<double>(ai_mesh->mTextureCoords[0][i].x),
                    static_cast<double>(ai_mesh->mTextureCoords[0][i].y)
                };
            } else {
                vertex.uv = Vec2<double>{0.0, 0.0};
            }

            // TODO: VERTEX COLOR AND ADDITIONAL ATTRIBUTES
            // if (ai_mesh->HasVertexColors(0)) {
            //     const aiColor4D& color = ai_mesh->mColors[0][i];
            //     vertex.color = Vec4<double>{color.r, color.g, color.b, color.a};
            // }
            //
            // // Tangent and bitangent (needed for normal mapping)
            // if (ai_mesh->HasTangentsAndBitangents()) {
            //     vertex.tangent = convert_vec3_(ai_mesh->mTangents[i]);
            //     vertex.bitangent = convert_vec3_(ai_mesh->mBitangents[i]);
            // }

            vertices.push_back(vertex);
        }

        // TODO: MATERIAL ASSIGNMENT
        // The mesh stores a material index that we'd look up:
        // unsigned int material_index = ai_mesh->mMaterialIndex;
        // Material<TSpectral>* material = ctx.material_map[material_index];
        // The Mesh constructor or a setter would then associate the material.
        auto mesh = std::make_shared<Mesh<TSpectral>>(std::move(indices), std::move(vertices));
        mesh->set_name(std::string(ai_mesh->mName.C_Str()));
        return mesh;
    }

    /**
     * @brief Recursively process ASSIMP nodes and build the scene graph.
     *
     * @param ai_node The current ASSIMP node
     * @param parent_frame The parent FrameNode in our scene graph
     * @param ctx Loading context
     */
    template <IsSpectral TSpectral>
    void ModelLoader<TSpectral>::process_node_(
        const aiNode* ai_node,
        FrameNode<TSpectral>* parent_frame,
        LoadContext& ctx
    ) {
        // Create a new FrameNode for this ASSIMP node
        auto child_weak = parent_frame->new_child();
        auto child = child_weak.lock();

        if (!child) {
            HUIRA_LOG_ERROR("Failed to create child FrameNode");
            return;
        }

        // Apply the node's transformation
        Transform<double> transform = convert_transform_(ai_node->mTransformation);
        child->set_position(transform.position);
        child->set_rotation(transform.rotation);
        child->set_scale(transform.scale);
        child->set_name(std::string(ai_node->mName.C_Str()));

        // Create instances for any meshes attached to this node
        for (unsigned int i = 0; i < ai_node->mNumMeshes; ++i) {
            unsigned int mesh_index = ai_node->mMeshes[i];
            Mesh<TSpectral>* mesh_ptr = ctx.mesh_map[mesh_index];

            if (mesh_ptr) {
                child->new_instance(mesh_ptr);
            } else {
                HUIRA_LOG_ERROR("Mesh index " + std::to_string(mesh_index) + " not found in mesh map");
            }
        }

        // Recursively process child nodes
        for (unsigned int i = 0; i < ai_node->mNumChildren; ++i) {
            process_node_(ai_node->mChildren[i], child.get(), ctx);
        }
    }

    /**
     * @brief Convert ASSIMP's 4x4 matrix to our Transform.
     */
    template <IsSpectral TSpectral>
    Transform<double> ModelLoader<TSpectral>::convert_transform_(const aiMatrix4x4& m) {
        Transform<double> transform;

        // ASSIMP matrices are row-major, so m[row][col]
        // We need to decompose the 4x4 matrix into position, rotation, and scale

        // Extract translation (4th column)
        transform.position = Vec3<double>{
            static_cast<double>(m.a4),
            static_cast<double>(m.b4),
            static_cast<double>(m.c4)
        };

        // Extract scale (length of each basis vector)
        Vec3<double> col0{static_cast<double>(m.a1), static_cast<double>(m.b1), static_cast<double>(m.c1)};
        Vec3<double> col1{static_cast<double>(m.a2), static_cast<double>(m.b2), static_cast<double>(m.c2)};
        Vec3<double> col2{static_cast<double>(m.a3), static_cast<double>(m.b3), static_cast<double>(m.c3)};

        double scale_x = std::sqrt(col0.x * col0.x + col0.y * col0.y + col0.z * col0.z);
        double scale_y = std::sqrt(col1.x * col1.x + col1.y * col1.y + col1.z * col1.z);
        double scale_z = std::sqrt(col2.x * col2.x + col2.y * col2.y + col2.z * col2.z);

        transform.scale = Vec3<double>{scale_x, scale_y, scale_z};

        // Normalize the basis vectors to get the rotation matrix
        if (scale_x > 1e-10) {
            col0.x /= scale_x; col0.y /= scale_x; col0.z /= scale_x;
        }
        if (scale_y > 1e-10) {
            col1.x /= scale_y; col1.y /= scale_y; col1.z /= scale_y;
        }
        if (scale_z > 1e-10) {
            col2.x /= scale_z; col2.y /= scale_z; col2.z /= scale_z;
        }

        Mat3<double> matrix{
            col0.x, col1.x, col2.x,
            col0.y, col1.y, col2.y,
            col0.z, col1.z, col2.z
        };
        transform.rotation = Rotation<double>(matrix);

        return transform;
    }

    /**
     * @brief Convert ASSIMP's 3D vector to our Vec3.
     */
    template <IsSpectral TSpectral>
    Vec3<double> ModelLoader<TSpectral>::convert_vec3_(const aiVector3D& v) {
        return Vec3<double>{
            static_cast<double>(v.x),
            static_cast<double>(v.y),
            static_cast<double>(v.z)
        };
    }
}
