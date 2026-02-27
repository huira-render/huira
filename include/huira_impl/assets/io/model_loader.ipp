#include <algorithm>
#include <cmath>
#include <string>

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "huira/core/rotation.hpp"
#include "huira/core/types.hpp"
#include "huira/util/logger.hpp"

#include "huira/images/io/bmp_io.hpp"
#include "huira/images/io/hdr_io.hpp"
#include "huira/images/io/jpeg_io.hpp"
#include "huira/images/io/png_io.hpp"
#include "huira/images/io/tga_io.hpp"

namespace huira {
    /**
     * @brief Load a 3D model from file and add it to the scene.
     *
     * Loads the model using ASSIMP, converts meshes and node hierarchy, and attaches the model to the scene.
     *
     * @param scene Reference to the scene to add the model to
     * @param file_path Path to the model file
     * @param name Name for the model (optional)
     * @param post_process_flags ASSIMP post-processing flags (optional)
     * @return Shared pointer to the loaded Model
     * @throws std::runtime_error if loading fails
     */
    template <IsSpectral TSpectral>
    std::shared_ptr<Model<TSpectral>> ModelLoader<TSpectral>::load(
        Scene<TSpectral>& scene,
        const fs::path& file_path,
        std::string name,
        unsigned int post_process_flags,
        std::function<TSpectral(RGB)> spectral_conversion
    ) {
        // Validate file exists
        if (!fs::exists(file_path)) {
            HUIRA_THROW_ERROR("ModelLoader::load - File not found: " + file_path.string());
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
                "ModelLoader::load - ASSIMP error loading '" + file_path.string() + "': " +
                std::string(importer.GetErrorString())
            );
        }

        HUIRA_LOG_INFO("ModelLoader::load - Loading " + file_path.string());
        HUIRA_LOG_INFO("  - Meshes: " + std::to_string(ai_scene->mNumMeshes));
        HUIRA_LOG_INFO("  - Materials: " + std::to_string(ai_scene->mNumMaterials));
        HUIRA_LOG_INFO("  - Textures: " + std::to_string(ai_scene->mNumTextures));

        // Create the Model object
        auto shared_model = std::make_shared<Model<TSpectral>>();
        shared_model->source_path_ = file_path;
        if (name.empty()) {
            name = file_path.stem().string();
        }
        scene.models_.add(shared_model, name);

        // Create the root node for the model's scene graph
        // Note: We pass nullptr for Scene* since this is a disconnected graph
        // This means SPICE-based transforms won't work within models
        shared_model->root_node_ = std::make_shared<FrameNode<TSpectral>>(nullptr);

        // Setup loading context
        LoadContext ctx;
        ctx.ai_scene = ai_scene;
        ctx.model = shared_model.get();
        ctx.scene = &scene;
        ctx.spectral_conversion = std::move(spectral_conversion);
        ctx.base_directory = file_path.parent_path();
        
        // Process all materials:
        process_materials_(ctx);

        // Process all meshes:
        process_meshes_(ctx);

        // Process the node hierarchy (creates FrameNodes and Instances)
        // The root ASSIMP node's transform is applied to our root node
        Transform<double> root_transform = convert_transform_(ai_scene->mRootNode->mTransformation);
        shared_model->root_node_->set_position(root_transform.position);
        shared_model->root_node_->set_rotation(root_transform.rotation);
        shared_model->root_node_->set_scale(root_transform.scale);

        // Process children of the root node
        for (unsigned int i = 0; i < ai_scene->mRootNode->mNumChildren; ++i) {
            process_node_(ai_scene->mRootNode->mChildren[i], shared_model->root_node_.get(), ctx);
        }

        // Also handle any meshes directly attached to the root node
        for (unsigned int i = 0; i < ai_scene->mRootNode->mNumMeshes; ++i) {
            unsigned int mesh_index = ai_scene->mRootNode->mMeshes[i];
            auto it = ctx.mesh_map.find(mesh_index);
            if (it != ctx.mesh_map.end()) {
                MeshHandle<TSpectral> mesh_handle = it->second;
                shared_model->root_node_->new_instance(mesh_handle.get_shared().get());
            } else {
                HUIRA_LOG_ERROR("ModelLoader::load - Mesh index " + std::to_string(mesh_index) + " not found in mesh map");
            }
        }

        HUIRA_LOG_INFO("ModelLoader::load - Model loaded successfully: " + shared_model->get_info());

        return shared_model;
    }

    /**
     * @brief Process all meshes in the ASSIMP scene and create huira Mesh objects.
     *
     * Iterates through all meshes in the ASSIMP scene, converts them, and stores them in the loading context.
     *
     * @param ctx Loading context
     */
    template <IsSpectral TSpectral>
    void ModelLoader<TSpectral>::process_meshes_(LoadContext& ctx) {
        for (unsigned int i = 0; i < ctx.ai_scene->mNumMeshes; ++i) {
            const aiMesh* ai_mesh = ctx.ai_scene->mMeshes[i];

            auto mesh_handle = convert_mesh_(ai_mesh, ctx);
            ctx.mesh_map.emplace(i, mesh_handle);

            HUIRA_LOG_DEBUG("ModelLoader::process_meshes_ - Processed mesh " + std::to_string(i) + ": " +
                           std::string(ai_mesh->mName.C_Str()) +
                           " (" + std::to_string(ai_mesh->mNumVertices) + " vertices, " +
                           std::to_string(ai_mesh->mNumFaces) + " faces)");
        }
    }

    /**
     * @brief Convert a single ASSIMP mesh to a huira Mesh.
     *
     * Converts the given ASSIMP mesh to a huira Mesh, including vertex and index buffers.
     *
     * @param ai_mesh ASSIMP mesh pointer
     * @param ctx Loading context
     * @return Mesh handle for the converted mesh
     */
    template <IsSpectral TSpectral>
    MeshHandle<TSpectral> ModelLoader<TSpectral>::convert_mesh_(
        const aiMesh* ai_mesh,
        LoadContext& ctx)
    {
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
        TangentBuffer tangent_buffer;
        vertices.reserve(ai_mesh->mNumVertices);

        for (unsigned int i = 0; i < ai_mesh->mNumVertices; ++i) {
            Vertex<TSpectral> vertex;

            // Position (always present)
            vertex.position = convert_vec3_(ai_mesh->mVertices[i]);

            // Normal (should be present after aiProcess_GenNormals)
            if (ai_mesh->HasNormals()) {
                vertex.normal = convert_vec3_(ai_mesh->mNormals[i]);
            } else {
                vertex.normal = Vec3<float>{0.0, 1.0, 0.0}; // Default up
            }

            // Texture coordinates (first UV channel only for now)
            if (ai_mesh->HasTextureCoords(0)) {
                // ASSIMP stores UVs as 3D vectors; we only need the first two components
                vertex.uv = Vec2<float>{
                    static_cast<float>(ai_mesh->mTextureCoords[0][i].x),
                    1.f - static_cast<float>(ai_mesh->mTextureCoords[0][i].y)
                };
            } else {
                vertex.uv = Vec2<float>{0.0, 0.0};
            }

            // Load vertex albedo
            if (ai_mesh->HasVertexColors(0)) {
                const aiColor4D& color = ai_mesh->mColors[0][i];
                // NOTE: Vertex alpha is ignored
                RGB rgb{color.r, color.g, color.b};
                vertex.albedo = ctx.spectral_conversion(rgb);
            }
            
            // Tangent and bitangent (needed for normal mapping)
            if (ai_mesh->HasTangentsAndBitangents()) {
                Tangent vertex_tangent;
                vertex_tangent.tangent = convert_vec3_(ai_mesh->mTangents[i]);
                vertex_tangent.bitangent = convert_vec3_(ai_mesh->mBitangents[i]);
                tangent_buffer.push_back(vertex_tangent);
            }

            vertices.push_back(vertex);
        }

        auto mesh = Mesh<TSpectral>(std::move(indices), std::move(vertices), std::move(tangent_buffer));
        auto mesh_handle = ctx.scene->add_mesh(std::move(mesh), std::string(ai_mesh->mName.C_Str()));

        // Assign material
        unsigned int material_index = ai_mesh->mMaterialIndex;
        auto mat_it = ctx.material_map.find(material_index);
        if (mat_it != ctx.material_map.end()) {
            mesh_handle.get()->set_material(mat_it->second.get().get());
        }
        else {
            HUIRA_LOG_WARNING("ModelLoader::convert_mesh_ - No material found for mesh " +
                std::string(ai_mesh->mName.C_Str()) + " (material index " +
                std::to_string(material_index) + ")");
        }

        return mesh_handle;
    }

    /**
     * @brief Recursively process ASSIMP nodes and build the scene graph.
     *
     * Creates FrameNodes for each ASSIMP node, applies transforms, and attaches mesh instances.
     *
     * @param ai_node Current ASSIMP node
     * @param parent_frame Parent FrameNode in the scene graph
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
            HUIRA_THROW_ERROR("ModelLoader::process_node_ - Failed to create child FrameNode");
        }

        // Apply the node's transformation
        Transform<double> transform = convert_transform_(ai_node->mTransformation);
        child->set_position(transform.position);
        child->set_rotation(transform.rotation);
        child->set_scale(transform.scale);
        ctx.scene->register_node_name_(child, std::string(ai_node->mName.C_Str()));

        // Create instances for any meshes attached to this node
        for (unsigned int i = 0; i < ai_node->mNumMeshes; ++i) {
            unsigned int mesh_index = ai_node->mMeshes[i];
            auto it = ctx.mesh_map.find(mesh_index);
            if (it != ctx.mesh_map.end()) {
                MeshHandle<TSpectral> mesh_handle = it->second;
                Mesh<TSpectral>* mesh_ptr = mesh_handle.get_shared().get();
                if (mesh_ptr) {
                    child->new_instance(mesh_ptr);
                } else {
                    HUIRA_THROW_ERROR("ModelLoader::process_node_ - MeshHandle for mesh index " + std::to_string(mesh_index) + " is invalid");
                }
            } else {
                HUIRA_THROW_ERROR("ModelLoader::process_node_ - Mesh index " + std::to_string(mesh_index) + " not found in mesh map");
            }
        }

        // Recursively process child nodes
        for (unsigned int i = 0; i < ai_node->mNumChildren; ++i) {
            process_node_(ai_node->mChildren[i], child.get(), ctx);
        }
    }

    /**
     * @brief Convert ASSIMP's 4x4 matrix to huira Transform.
     *
     * Decomposes the ASSIMP matrix into position, rotation, and scale components.
     *
     * @param m ASSIMP 4x4 matrix
     * @return Transform object
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

        // Recall the column major ordering of GLM:
        Mat3<double> matrix{
            col0.x, col0.y, col0.z,
            col1.x, col1.y, col1.z,
            col2.x, col2.y, col2.z
        };
        
        transform.rotation = Rotation<double>::from_local_to_parent(matrix);

        return transform;
    }

    /**
     * @brief Convert ASSIMP's 3D vector to huira Vec3.
     *
     * Converts the ASSIMP vector to a huira Vec3.
     *
     * @param v ASSIMP 3D vector
     * @return Vec3 object
     */
    template <IsSpectral TSpectral>
    Vec3<float> ModelLoader<TSpectral>::convert_vec3_(const aiVector3D& v) {
        return Vec3<float>{
            static_cast<float>(v.x),
            static_cast<float>(v.y),
            static_cast<float>(v.z)
        };
    }

    template <IsSpectral TSpectral>
    void ModelLoader<TSpectral>::process_materials_(LoadContext& ctx) {
        for (unsigned int i = 0; i < ctx.ai_scene->mNumMaterials; ++i) {
            const aiMaterial* ai_mat = ctx.ai_scene->mMaterials[i];
            std::string name = std::string(ai_mat->GetName().C_Str());
            MaterialHandle<TSpectral> material = ctx.scene->new_cook_torrance_material(name);

            // Assign albedos:
            if (auto tex = load_material_texture_<TSpectral>(ai_mat, aiTextureType_BASE_COLOR, ctx)) {
                material.set_albedo(tex.value());
            }
            aiColor4D base_color;
            if (ai_mat->Get(AI_MATKEY_BASE_COLOR, base_color) == AI_SUCCESS) {
                material.set_albedo_factor(ctx.spectral_conversion(RGB{ base_color.r, base_color.g, base_color.b }));
            }

            // Replace the separate roughness/metallic loading with:
            if (ai_mat->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0 ||
                ai_mat->GetTextureCount(aiTextureType_METALNESS) > 0)
            {
                // Load as RGB to extract individual channels
                aiTextureType packed_type = ai_mat->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0
                    ? aiTextureType_DIFFUSE_ROUGHNESS
                    : aiTextureType_METALNESS;

                if (auto tex = load_material_texture_<Vec3<float>>(ai_mat, packed_type, ctx)) {
                    const Image<Vec3<float>>& packed = *tex.value().get()->image();

                    // Extract channels: green = roughness, blue = metallic
                    Image<float> roughness_img(packed.width(), packed.height());
                    Image<float> metallic_img(packed.width(), packed.height());
                    for (int y = 0; y < packed.height(); ++y) {
                        for (int x = 0; x < packed.width(); ++x) {
                            roughness_img(x, y) = packed(x, y).y; // green
                            metallic_img(x, y) = packed(x, y).z;  // blue
                        }
                    }

                    auto rough_handle = ctx.scene->add_texture(std::move(roughness_img), name + "_roughness");
                    auto metal_handle = ctx.scene->add_texture(std::move(metallic_img), name + "_metallic");
                    material.set_roughness_image(rough_handle);
                    material.set_metallic_image(metal_handle);
                }
            }

            // Assign normals:
            if (auto tex = load_material_texture_<Vec3<float>>(ai_mat, aiTextureType_NORMALS, ctx, true)) {
                material.set_normal_image(tex.value());
            }

            // Assign emissive:
            if (auto tex = load_material_texture_<TSpectral>(ai_mat, aiTextureType_EMISSION_COLOR, ctx)) {
                material.set_emissive_image(tex.value());
            }
            aiColor3D emissive;
            if (ai_mat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
                material.set_emissive_factor(ctx.spectral_conversion(RGB{ emissive.r, emissive.g, emissive.b }));
            }
            
            ctx.material_map.emplace(i, material);

            HUIRA_LOG_DEBUG("ModelLoader::process_materials_ - Processed material " +
                std::to_string(i) + ": " + name);
        }
    }


    enum class ImageFormat_ { PNG, JPEG, TGA, BMP, HDR, Unknown };
    inline ImageFormat_ detect_image_format_(const std::string& hint_or_extension) {
        std::string lower = hint_or_extension;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // Strip leading dot if present
        if (!lower.empty() && lower[0] == '.') {
            lower = lower.substr(1);
        }

        if (lower == "png")                    return ImageFormat_::PNG;
        if (lower == "jpg" || lower == "jpeg") return ImageFormat_::JPEG;
        if (lower == "tga")                    return ImageFormat_::TGA;
        if (lower == "bmp")                    return ImageFormat_::BMP;
        if (lower == "hdr")                    return ImageFormat_::HDR;
        return ImageFormat_::Unknown;
    }

    inline Image<RGB> load_rgb_from_buffer_(
        const unsigned char* data, std::size_t size, ImageFormat_ format)
    {
        switch (format) {
        case ImageFormat_::PNG:  return read_image_png(data, size).first;
        case ImageFormat_::JPEG: return read_image_jpeg(data, size);
        case ImageFormat_::TGA:  return read_image_tga(data, size).first;
        case ImageFormat_::BMP:  return read_image_bmp(data, size).first;
        case ImageFormat_::HDR:  return read_image_hdr(data, size);
        case ImageFormat_::Unknown:
        default:
            HUIRA_THROW_ERROR("load_rgb_from_buffer_ - Unsupported image format");
        }
    }

    inline Image<float> load_mono_from_buffer_(
        const unsigned char* data, std::size_t size, ImageFormat_ format)
    {
        switch (format) {
        case ImageFormat_::PNG:  return read_image_png_mono(data, size).first;
        case ImageFormat_::JPEG: return read_image_jpeg_mono(data, size);
        case ImageFormat_::TGA:  return read_image_tga_mono(data, size).first;
        case ImageFormat_::BMP:  return read_image_bmp_mono(data, size).first;
        case ImageFormat_::HDR:  return read_image_hdr_mono(data, size);
        case ImageFormat_::Unknown:
        default:
            HUIRA_THROW_ERROR("load_mono_from_buffer_ - Unsupported image format");
        }
    }

    inline Image<RGB> load_rgb_from_file_(const fs::path& filepath) {
        ImageFormat_ format = detect_image_format_(filepath.extension().string());
        switch (format) {
        case ImageFormat_::PNG:  return read_image_png(filepath).first;
        case ImageFormat_::JPEG: return read_image_jpeg(filepath);
        case ImageFormat_::TGA:  return read_image_tga(filepath).first;
        case ImageFormat_::BMP:  return read_image_bmp(filepath).first;
        case ImageFormat_::HDR:  return read_image_hdr(filepath);
        case ImageFormat_::Unknown:
        default:
            HUIRA_THROW_ERROR("load_rgb_from_file_ - Unsupported format: " +
                filepath.extension().string());
        }
    }

    inline Image<float> load_mono_from_file_(const fs::path& filepath) {
        ImageFormat_ format = detect_image_format_(filepath.extension().string());
        switch (format) {
        case ImageFormat_::PNG:  return read_image_png_mono(filepath).first;
        case ImageFormat_::JPEG: return read_image_jpeg_mono(filepath);
        case ImageFormat_::TGA:  return read_image_tga_mono(filepath).first;
        case ImageFormat_::BMP:  return read_image_bmp_mono(filepath).first;
        case ImageFormat_::HDR:  return read_image_hdr_mono(filepath);
        case ImageFormat_::Unknown:
        default:
            HUIRA_THROW_ERROR("load_mono_from_file_ - Unsupported format: " +
                filepath.extension().string());
        }
    }

    /// Convert Image<RGB> to Image<Vec3<float>> (direct channel copy, for normal maps)
    inline Image<Vec3<float>> rgb_to_vec3_(const Image<RGB>& rgb) {
        Image<Vec3<float>> result(rgb.width(), rgb.height());
        for (int y = 0; y < rgb.height(); ++y) {
            for (int x = 0; x < rgb.width(); ++x) {
                const RGB& p = rgb(x, y);
                result(x, y) = Vec3<float>{ p[0], p[1], p[2] };
            }
        }
        return result;
    }

    template <IsSpectral TSpectral, typename TPixel>
    Image<TPixel> convert_embedded_raw_(
        const aiTexture* embedded,
        const std::function<TSpectral(RGB)>& spectral_conversion)
    {
        const int w = static_cast<int>(embedded->mWidth);
        const int h = static_cast<int>(embedded->mHeight);
        const float inv255 = 1.0f / 255.0f;

        Image<TPixel> result(w, h);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const aiTexel& texel = embedded->pcData[y * w + x];
                const float r = static_cast<float>(texel.r) * inv255;
                const float g = static_cast<float>(texel.g) * inv255;
                const float b = static_cast<float>(texel.b) * inv255;

                if constexpr (std::is_same_v<TPixel, TSpectral>) {
                    result(x, y) = spectral_conversion(RGB{ r, g, b });
                }
                else if constexpr (std::is_same_v<TPixel, float>) {
                    // Luminance-weighted grayscale
                    result(x, y) = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                }
                else if constexpr (std::is_same_v<TPixel, Vec3<float>>) {
                    result(x, y) = Vec3<float>{ r, g, b };
                }
                else {
                    static_assert(sizeof(TPixel) == 0, "Unsupported pixel type");
                }
            }
        }

        return result;
    }

    /// Extract green channel from an RGB image (for GLTF roughness)
    inline Image<float> extract_green_channel_(const Image<RGB>& rgb) {
        Image<float> result(rgb.width(), rgb.height());
        for (int y = 0; y < rgb.height(); ++y) {
            for (int x = 0; x < rgb.width(); ++x) {
                result(x, y) = rgb(x, y)[1]; // green
            }
        }
        return result;
    }

    /// Extract blue channel from an RGB image (for GLTF metallic)
    inline Image<float> extract_blue_channel_(const Image<RGB>& rgb) {
        Image<float> result(rgb.width(), rgb.height());
        for (int y = 0; y < rgb.height(); ++y) {
            for (int x = 0; x < rgb.width(); ++x) {
                result(x, y) = rgb(x, y)[2]; // blue
            }
        }
        return result;
    }

    /**
     * @brief Load a texture from an aiMaterial for a given texture type.
     *
     * Checks whether the material has a texture for the given slot, resolves
     * the path (embedded or on-disk), loads the raw image data, converts it
     * to the target pixel type TPixel, registers it with the Scene, and returns
     * a TextureHandle. Uses the LoadContext caches for deduplication.
     *
     * @tparam TPixel Target pixel type (TSpectral, float, or Vec3<float>)
     * @param ai_mat The ASSIMP material
     * @param tex_type The ASSIMP texture type slot
     * @param ctx Loading context with scene reference and caches
     * @return TextureHandle if a texture was found and loaded, std::nullopt otherwise
     */
    template <IsSpectral TSpectral>
    template <typename TPixel>
    std::optional<TextureHandle<TPixel>> ModelLoader<TSpectral>::load_material_texture_(
        const aiMaterial* ai_mat,
        aiTextureType tex_type,
        LoadContext& ctx,
        bool is_normal_map)
    {
        // Check if this material has a texture for the requested slot
        if (ai_mat->GetTextureCount(tex_type) == 0) {
            return std::nullopt;
        }

        // Get the texture path from ASSIMP
        aiString ai_path;
        if (ai_mat->GetTexture(tex_type, 0, &ai_path) != AI_SUCCESS) {
            HUIRA_LOG_WARNING("ModelLoader::load_material_texture_ - "
                "Failed to get texture path for type " +
                std::to_string(static_cast<int>(tex_type)));
            return std::nullopt;
        }

        const std::string tex_path_str(ai_path.C_Str());

        // Check deduplication cache
        auto& cache = get_texture_cache_<TPixel>(ctx);
        auto cache_it = cache.find(tex_path_str);
        if (cache_it != cache.end()) {
            return cache_it->second;
        }

        // ------------------------------------------------------------------
        //  Load the image data
        // ------------------------------------------------------------------

        Image<TPixel> image;
        bool loaded = false;

        // Check for embedded texture
        const aiTexture* embedded = ctx.ai_scene->GetEmbeddedTexture(tex_path_str.c_str());

        if (embedded) {
            if (embedded->mHeight == 0) {
                // Compressed embedded texture: mWidth = byte count, achFormatHint = format
                const auto* data = reinterpret_cast<const unsigned char*>(embedded->pcData);
                const auto  size = static_cast<std::size_t>(embedded->mWidth);
                ImageFormat_ format = detect_image_format_(std::string(embedded->achFormatHint));

                if (format == ImageFormat_::Unknown) {
                    HUIRA_LOG_WARNING("ModelLoader::load_material_texture_ - "
                        "Unknown embedded format hint: " +
                        std::string(embedded->achFormatHint) +
                        " for texture: " + tex_path_str);
                    return std::nullopt;
                }

                try {
                    if constexpr (std::is_same_v<TPixel, TSpectral>) {
                        Image<RGB> rgb = load_rgb_from_buffer_(data, size, format);
                        image = Image<TSpectral>(rgb.width(), rgb.height());
                        for (int y = 0; y < rgb.height(); ++y) {
                            for (int x = 0; x < rgb.width(); ++x) {
                                image(x, y) = ctx.spectral_conversion(rgb(x, y));
                            }
                        }
                    }
                    else if constexpr (std::is_same_v<TPixel, float>) {
                        image = load_mono_from_buffer_(data, size, format);
                    }
                    else if constexpr (std::is_same_v<TPixel, Vec3<float>>) {
                        Image<RGB> rgb = load_rgb_from_buffer_(data, size, format);
                        image = rgb_to_vec3_(rgb);
                    }
                    loaded = true;
                }
                catch (const std::exception& e) {
                    HUIRA_LOG_WARNING("ModelLoader::load_material_texture_ - "
                        "Failed to decode embedded texture: " + tex_path_str +
                        " (" + e.what() + ")");
                    return std::nullopt;
                }

            }
            else {
                // Uncompressed embedded texture (raw ARGB8888 aiTexel data)
                try {
                    image = convert_embedded_raw_<TSpectral, TPixel>(
                        embedded, ctx.spectral_conversion);
                    loaded = true;
                }
                catch (const std::exception& e) {
                    HUIRA_LOG_WARNING("ModelLoader::load_material_texture_ - "
                        "Failed to convert raw embedded texture: " + tex_path_str +
                        " (" + e.what() + ")");
                    return std::nullopt;
                }
            }

        }
        else {
            // External texture file
            fs::path resolved_path = ctx.base_directory / tex_path_str;
            if (!fs::exists(resolved_path)) {
                // Try the path as-is (might be absolute)
                resolved_path = fs::path(tex_path_str);
            }

            if (!fs::exists(resolved_path)) {
                HUIRA_LOG_WARNING("ModelLoader::load_material_texture_ - "
                    "Texture file not found: " + tex_path_str +
                    " (searched: " + (ctx.base_directory / tex_path_str).string() + ")");
                return std::nullopt;
            }

            try {
                if constexpr (std::is_same_v<TPixel, TSpectral>) {
                    Image<RGB> rgb = load_rgb_from_file_(resolved_path);
                    image = Image<TSpectral>(rgb.width(), rgb.height());
                    for (int y = 0; y < rgb.height(); ++y) {
                        for (int x = 0; x < rgb.width(); ++x) {
                            image(x, y) = ctx.spectral_conversion(rgb(x, y));
                        }
                    }
                }
                else if constexpr (std::is_same_v<TPixel, float>) {
                    image = load_mono_from_file_(resolved_path);
                }
                else if constexpr (std::is_same_v<TPixel, Vec3<float>>) {
                    Image<RGB> rgb = load_rgb_from_file_(resolved_path);
                    image = rgb_to_vec3_(rgb);
                }
                loaded = true;
            }
            catch (const std::exception& e) {
                HUIRA_LOG_WARNING("ModelLoader::load_material_texture_ - "
                    "Failed to load texture file: " + resolved_path.string() +
                    " (" + e.what() + ")");
                return std::nullopt;
            }
        }

        if (!loaded) {
            return std::nullopt;
        }

        // Register with the Scene and cache
        std::string tex_name = fs::path(tex_path_str).stem().string();
        TextureHandle<TPixel> handle = [&]() {
            if constexpr (std::is_same_v<TPixel, Vec3<float>>) {
                if (is_normal_map) {
                    return ctx.scene->add_normal_texture(std::move(image), tex_name);
                }
            }
            return ctx.scene->add_texture(std::move(image), tex_name);
            }();
        cache.emplace(tex_path_str, handle);

        HUIRA_LOG_DEBUG("ModelLoader::load_material_texture_ - Loaded texture: " +
            tex_name + " (" + std::to_string(image.width()) + "x" +
            std::to_string(image.height()) + ")");

        return handle;
    }

    // =========================================================================
    //  Texture cache accessor
    // =========================================================================

    template <IsSpectral TSpectral>
    template <typename TPixel>
    auto& ModelLoader<TSpectral>::get_texture_cache_(LoadContext& ctx) {
        if constexpr (std::is_same_v<TPixel, TSpectral>) {
            return ctx.spectral_texture_cache;
        }
        else if constexpr (std::is_same_v<TPixel, float>) {
            return ctx.mono_texture_cache;
        }
        else if constexpr (std::is_same_v<TPixel, Vec3<float>>) {
            return ctx.vec3_texture_cache;
        }
        else {
            static_assert(sizeof(TPixel) == 0, "Unsupported texture pixel type");
        }
    }

}
