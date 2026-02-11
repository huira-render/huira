#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <filesystem>

#include "huira/core/concepts/spectral_concepts.hpp"

#include "huira/assets/mesh.hpp"
#include "huira/assets/model.hpp"
#include "huira/assets/io/model_loader.hpp"
#include "huira/handles/model_handle.hpp"
#include "huira/handles/root_frame_handle.hpp"
#include "huira/handles/mesh_handle.hpp"
#include "huira/stars/star.hpp"
#include "huira/scene/name_registry.hpp"
#include "huira/assets/io/model_loader.hpp"

namespace fs = std::filesystem;

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral>
    class SceneView;


    template <IsSpectral TSpectral>
    class Scene {
    private:
        std::shared_ptr<FrameNode<TSpectral>> root_node_;

    public:
        Scene(const Scene&) = delete; // Delete the copy constructor
        Scene& operator=(const Scene&) = delete; // Delete the copy assignment operator

        Scene();

        RootFrameHandle<TSpectral> root;
        
        MeshHandle<TSpectral> add_mesh(Mesh<TSpectral>&& mesh, std::string name = "");
        void set_name(const MeshHandle<TSpectral>& mesh_handle, const std::string& name);
        MeshHandle<TSpectral> get_mesh(const std::string& name);
        void delete_mesh(const MeshHandle<TSpectral>& mesh_handle);


        LightHandle<TSpectral> new_point_light(TSpectral intensity, std::string name = "");
        LightHandle<TSpectral> new_sun_light();
        LightHandle<TSpectral> add_light(std::shared_ptr<Light<TSpectral>> light, std::string name = "");

        void set_name(const LightHandle<TSpectral>& light_handle, const std::string& name);
        LightHandle<TSpectral> get_light(const std::string& name);
        void delete_light(const LightHandle<TSpectral>& light_handle);
        
        UnresolvedObjectHandle<TSpectral> new_unresolved_object(TSpectral irradiance = TSpectral{ 0 }, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_object_from_power(TSpectral power, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_sphere(units::Meter radius, InstanceHandle<TSpectral> sun, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_sphere(units::Meter radius, InstanceHandle<TSpectral> sun, TSpectral albedo, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_asteroid(float H, float G, InstanceHandle<TSpectral> sun, std::string name = "");
        UnresolvedObjectHandle<TSpectral> new_unresolved_asteroid(float H, float G, InstanceHandle<TSpectral> sun, TSpectral albedo, std::string name = "");
        UnresolvedObjectHandle<TSpectral> add_unresolved_object(std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object, std::string name = "");

        void set_name(const UnresolvedObjectHandle<TSpectral>& unresolved_object_handle, const std::string& name);
        UnresolvedObjectHandle<TSpectral> get_unresolved_object(const std::string& name);
        void delete_unresolved_object(const UnresolvedObjectHandle<TSpectral>& unresolved_object_handle);

        CameraModelHandle<TSpectral> new_camera_model(std::string name = "");
        void set_name(const CameraModelHandle<TSpectral>& camera_model_handle, const std::string& name);
        CameraModelHandle<TSpectral> get_camera_model(const std::string& name);
        void delete_camera_model(const CameraModelHandle<TSpectral>& camera_model_handle);


        ModelHandle<TSpectral> load_model(
            const fs::path& file,
            std::string name = "",
            unsigned int post_process_flags = ModelLoader<TSpectral>::DEFAULT_POST_PROCESS_FLAGS
        );
        void set_name(const ModelHandle<TSpectral>& model_handle, const std::string& name);
        ModelHandle<TSpectral> get_model(const std::string& name);
        void delete_model(const ModelHandle<TSpectral>& model_handle);


        void add_star(const Star<TSpectral>& star);
        void set_stars(const std::vector<Star<TSpectral>>& stars);
        void load_stars(const fs::path& star_catalog_path, const Time& time, float min_magnitude = 100.f);
        

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
        NameRegistry<Mesh<TSpectral>> meshes_;
        NameRegistry<Light<TSpectral>> lights_;
        NameRegistry<UnresolvedObject<TSpectral>> unresolved_objects_;
        NameRegistry<CameraModel<TSpectral>> camera_models_;
        NameRegistry<Model<TSpectral>> models_;

        std::vector<Star<TSpectral>> stars_;

        void print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const;
        void print_node_details_(const Node<TSpectral>* node) const;

        template <typename TAssetPtr>
        void prune_graph_references_(TAssetPtr target_ptr);

        std::shared_ptr<Node<TSpectral>> find_node_shared_ptr_(const Node<TSpectral>* target) const;
        std::shared_ptr<Node<TSpectral>> find_node_in_tree_(const std::shared_ptr<Node<TSpectral>>& current, const Node<TSpectral>* target) const;

        NameRegistry<Node<TSpectral>> node_registry_;
        void register_node_name_(const std::shared_ptr<Node<TSpectral>>& node, const std::string& name);

        friend class Node<TSpectral>;
        friend class FrameNode<TSpectral>;
        friend class SceneView<TSpectral>;
        friend class ModelLoader<TSpectral>;
    };
}

#include "huira_impl/scene/scene.ipp"
