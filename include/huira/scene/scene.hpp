#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "huira/geometry/mesh.hpp"
#include "huira/assets/model.hpp"
#include "huira/assets/io/model_loader.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/assets/model_handle.hpp"
#include "huira/handles/assets/primitive_handle.hpp"
#include "huira/handles/geometry/geometry_handle.hpp"
#include "huira/handles/geometry/mesh_handle.hpp"
#include "huira/handles/geometry/ellipsoid_handle.hpp"
#include "huira/handles/materials/bsdf_handle.hpp"
#include "huira/handles/materials/material_handle.hpp"
#include "huira/handles/materials/texture_handle.hpp"
#include "huira/handles/scene/root_frame_handle.hpp"
#include "huira/handles/volumes/medium_handle.hpp"
#include "huira/images/image.hpp"
#include "huira/materials/material.hpp"
#include "huira/materials/texture.hpp"
#include "huira/stars/star.hpp"
#include "huira/stars/io/star_data.hpp"
#include "huira/scene/name_registry.hpp"

namespace fs = std::filesystem;

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral>
    class SceneView;

    /**
     * @brief Scene graph and asset manager for a rendered world.
     *
     * The Scene class manages all assets (geometries, materials, lights, unresolved objects, camera models, models, stars)
     * and the scene graph (root node, child nodes, instances, etc.). It provides methods for adding, removing,
     * and querying assets, as well as for loading star catalogs and models. The scene graph is rooted at a
     * FrameNode and supports hierarchical relationships between nodes and assets. Scene is non-copyable.
     *
     * @tparam TSpectral Spectral type used for asset spectral properties
     */
    template <IsSpectral TSpectral>
    class Scene {
    private:
        std::shared_ptr<FrameNode<TSpectral>> root_node_;

    public:
        Scene(const Scene&) = delete; // Delete the copy constructor
        Scene& operator=(const Scene&) = delete; // Delete the copy assignment operator

        Scene();
        ~Scene();

        RootFrameHandle<TSpectral> root;

        MeshHandle<TSpectral> add_mesh(const IndexBuffer& index_buffer,
                                       const VertexBuffer<TSpectral>& vertex_buffer,
                                       std::string name = "");
        MeshHandle<TSpectral> add_mesh(const IndexBuffer& index_buffer,
                                       const VertexBuffer<TSpectral>& vertex_buffer,
                                       const TangentBuffer& tangent_buffer,
                                       std::string name = "");
        EllipsoidHandle<TSpectral> add_ellipsoid(const units::Meter& x,
                                                 const units::Meter& y,
                                                 const units::Meter& z,
                                                 std::string name = "");
        GeometryHandle<TSpectral> add_geometry(std::shared_ptr<Geometry<TSpectral>> geom,
                                               std::string name = "");
        void set_name(const GeometryHandle<TSpectral>& geom_handle,
                      const std::string& name);
        GeometryHandle<TSpectral> get_geometry(const std::string& name) const;
        void delete_geometry(const GeometryHandle<TSpectral>& geom_handle);

        PrimitiveHandle<TSpectral> add_primitive(const GeometryHandle<TSpectral>& geom,
                                                 std::string name = "");
        PrimitiveHandle<TSpectral> add_primitive(const GeometryHandle<TSpectral>& geom, 
                                                 const MaterialHandle<TSpectral>& mat,
                                                 std::string name = "");
        PrimitiveHandle<TSpectral> add_primitive(const GeometryHandle<TSpectral>& geom,
                                                 const MediumHandle<TSpectral>& medium,
                                                 std::string name = "");
        PrimitiveHandle<TSpectral> add_primitive(const GeometryHandle<TSpectral>& geom, 
                                                 const MaterialHandle<TSpectral>& mat,
                                                  const MediumHandle<TSpectral>& medium,
                                                  std::string name = "");
        void set_name(const PrimitiveHandle<TSpectral>& primitive_handle, const std::string& name);
        PrimitiveHandle<TSpectral> get_primitive(const std::string& name) const;
        void delete_primitive(const PrimitiveHandle<TSpectral>& primitive_handle);

        LightHandle<TSpectral> new_sphere_light(
            const units::Meter& radius,
            const units::SpectralWattsPerMeterSquaredSteradian<TSpectral>& spectral_radiance,
            std::string name = "");
        LightHandle<TSpectral> new_sphere_light(
            const units::Meter& radius,
            const units::SpectralWatts<TSpectral>& spectral_power,
            std::string name = "");
        LightHandle<TSpectral> new_sphere_light(const units::Meter& radius,
                                                const units::Watt& total_power,
                                                std::string name = "");
        LightHandle<TSpectral> new_sun_light();
        LightHandle<TSpectral> add_light(std::shared_ptr<Light<TSpectral>> light,
                                         std::string name = "");

        void set_name(const LightHandle<TSpectral>& light_handle, const std::string& name);
        LightHandle<TSpectral> get_light(const std::string& name) const;
        void delete_light(const LightHandle<TSpectral>& light_handle);
        
        UnresolvedObjectHandle<TSpectral> new_unresolved_object(const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_object(const units::WattsPerMeterSquared& irradiance, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_object_from_magnitude(double visual_magnitude, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_object_from_magnitude(double visual_magnitude, TSpectral albedo, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_emitter(const units::SpectralWatts<TSpectral>& spectral_power, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_emitter(const units::Watt& power, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_sphere(units::Meter radius, InstanceHandle<TSpectral> sun, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_sphere(units::Meter radius, InstanceHandle<TSpectral> sun, TSpectral albedo, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_sphere(units::Meter radius, InstanceHandle<TSpectral> sun, float albedo, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_asteroid(double H, double G, InstanceHandle<TSpectral> sun, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_asteroid(double H, double G, InstanceHandle<TSpectral> sun, TSpectral albedo, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_asteroid(double H, double G, InstanceHandle<TSpectral> sun, float albedo, std::string name = "");
        UnresolvedObjectHandle<TSpectral> add_unresolved_object(std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object, std::string name = "");

        void set_name(const UnresolvedObjectHandle<TSpectral>& unresolved_object_handle, const std::string& name);
        UnresolvedObjectHandle<TSpectral> get_unresolved_object(const std::string& name) const;
        void delete_unresolved_object(const UnresolvedObjectHandle<TSpectral>& unresolved_object_handle);

        CameraModelHandle<TSpectral> new_camera_model(std::string name = "");
        void set_name(const CameraModelHandle<TSpectral>& camera_model_handle, const std::string& name);
        CameraModelHandle<TSpectral> get_camera_model(const std::string& name) const;
        void delete_camera_model(const CameraModelHandle<TSpectral>& camera_model_handle);


        ModelHandle<TSpectral> load_model(
            const fs::path& file,
            std::string name = "",
            unsigned int post_process_flags = ModelLoader<TSpectral>::DEFAULT_POST_PROCESS_FLAGS
        );
        void set_name(const ModelHandle<TSpectral>& model_handle, const std::string& name);
        ModelHandle<TSpectral> get_model(const std::string& name) const;
        void delete_model(const ModelHandle<TSpectral>& model_handle);

        BSDFHandle<TSpectral> new_bsdf_cook_torrance(std::string name = "");
        BSDFHandle<TSpectral> new_bsdf_hapke(float h, float B0, float b, float c,
                                             std::string name = "");
        BSDFHandle<TSpectral> new_bsdf_lambertian(std::string name = "");
        BSDFHandle<TSpectral> new_bsdf_lommel_seeliger(std::string name = "");
        BSDFHandle<TSpectral> new_bsdf_mcewen(std::string name = "");
        BSDFHandle<TSpectral> new_bsdf_null(std::string name = "");
        BSDFHandle<TSpectral> new_bsdf_oren_nayar(std::string name = "");
        BSDFHandle<TSpectral> add_bsdf(std::shared_ptr<BSDF<TSpectral>> bsdf,
                                       std::string name = "");

        MaterialHandle<TSpectral> new_material(const BSDFHandle<TSpectral>& bsdf, std::string name = "");
        MaterialHandle<TSpectral> add_material(std::shared_ptr<Material<TSpectral>> material, std::string name = "");

        void set_background_radiance(Image<TSpectral> background);
        void set_background_radiance(TSpectral background);
        void set_background_radiance(float background);

        TextureHandle<TSpectral> add_texture(Image<TSpectral>&& image, std::string name = "");
        TextureHandle<float> add_texture(Image<float>&& image, std::string name = "");
        TextureHandle<Vec3<float>> add_texture(Image<Vec3<float>>&& image, std::string name = "");
        TextureHandle<Vec3<float>> add_normal_texture(Image<Vec3<float>>&& image, std::string name = "");
        TextureHandle<Vec3<float>> add_normal_texture(Image<RGB>&& image, std::string name = "");

        void set_stars(const std::vector<Star<TSpectral>>& stars);
        void load_stars(const fs::path& star_catalog_path, const Time& time, float min_magnitude = 100.f);
        
        void load_dynamic_stars(const fs::path& star_catalog_path, const Time& time, float min_magnitude = 100.f);
        void update_star_epoch(const Time& time);
        
        void prune_unreferenced_assets();

        void print_meshes() const;
        void print_lights() const;
        void print_unresolved_objects() const;
        void print_camera_models() const;
        void print_models() const;
        void print_graph() const;
        void print_contents() const;

    private:
        // Assets:
        NameRegistry<Geometry<TSpectral>> geometries_;
        NameRegistry<Primitive<TSpectral>> primitives_;
        NameRegistry<Light<TSpectral>> lights_;
        NameRegistry<UnresolvedObject<TSpectral>> unresolved_objects_;
        NameRegistry<CameraModel<TSpectral>> camera_models_;
        NameRegistry<Model<TSpectral>> models_;

        // Material and Volume Assets:
        NameRegistry<BSDF<TSpectral>> bsdfs_;
        NameRegistry<Material<TSpectral>> materials_;
        NameRegistry<Texture<TSpectral>> spectral_textures_;
        NameRegistry<Texture<float>> mono_textures_;
        NameRegistry<Texture<Vec3<float>>> vec3_textures_;

        // Default textures:
        std::shared_ptr<Image<TSpectral>>   default_albedo_image_;
        std::shared_ptr<Image<float>>       default_alpha_image_;
        std::shared_ptr<Image<float>>       default_metallic_image_;
        std::shared_ptr<Image<float>>       default_roughness_image_;
        std::shared_ptr<Image<Vec3<float>>> default_normal_image_;
        std::shared_ptr<Image<TSpectral>>   default_transmission_image_;
        std::shared_ptr<Image<TSpectral>>   default_emission_image_;

        // Default materials and volumes:
        std::shared_ptr<Material<TSpectral>> default_material_;
        std::shared_ptr<Material<TSpectral>> default_null_material_;

        std::shared_ptr<Image<TSpectral>> background_;

        bool dynamic_stars_ = false;
        std::vector<Star<TSpectral>> stars_;
        std::vector<StarData> dynamic_star_data_;
        Time star_epoch_;

        void print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const;
        void print_node_details_(const Node<TSpectral>* node) const;

        template <typename TAssetPtr>
        void prune_graph_references_(TAssetPtr target_ptr);

        std::shared_ptr<Node<TSpectral>> find_node_shared_ptr_(const Node<TSpectral>* target) const;
        std::shared_ptr<Node<TSpectral>> find_node_in_tree_(const std::shared_ptr<Node<TSpectral>>& current, const Node<TSpectral>* target) const;

        NameRegistry<Node<TSpectral>> node_registry_;
        void register_node_name_(const std::shared_ptr<Node<TSpectral>>& node, const std::string& name);

        /// Embree RTC Device
        std::shared_ptr<EmbreeDevice> device_;

        friend class Node<TSpectral>;
        friend class FrameNode<TSpectral>;
        friend class SceneView<TSpectral>;
        friend class ModelLoader<TSpectral>;
    };
}

#include "huira_impl/scene/scene.ipp"
