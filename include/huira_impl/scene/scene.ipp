#include <stdexcept>
#include <iostream>

#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/assets/io/model_loader.hpp"
#include "huira/handles/model_handle.hpp"
#include "huira/handles/frame_handle.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/colorful_text.hpp"
#include "huira/stars/io/star_catalog.hpp"

namespace huira {
    // Suppressing C4355: 'this' is passed to FrameNode constructor, but FrameNode only stores
    // the pointer without calling back into the incomplete Scene object.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4355)
#endif

    template <IsSpectral TSpectral>
    Scene<TSpectral>::Scene()
        : root_node_(std::make_shared<FrameNode<TSpectral>>(this))
        , root(root_node_)
    {
        root_node_->set_spice("SOLAR SYSTEM BARYCENTER", "J2000");
    };

#ifdef _MSC_VER
#pragma warning(pop)
#endif

    template <IsSpectral TSpectral>
    MeshHandle<TSpectral> Scene<TSpectral>::add_mesh(Mesh<TSpectral>&& mesh, std::string name)
    {
        auto mesh_shared = std::make_shared<Mesh<TSpectral>>(std::move(mesh));
        meshes_.add(mesh_shared, name);
        return MeshHandle<TSpectral>{ mesh_shared };
    };

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_name(const MeshHandle<TSpectral>& mesh_handle, const std::string& name)
    {
        meshes_.set_name(mesh_handle.get(), name);
    };

    template <IsSpectral TSpectral>
    MeshHandle<TSpectral> Scene<TSpectral>::get_mesh(const std::string& name)
    {
        return MeshHandle<TSpectral>{ meshes_.lookup(name) };
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_mesh(const MeshHandle<TSpectral>& mesh_handle)
    {
        auto mesh_shared = mesh_handle.get();
        prune_graph_references_(mesh_shared.get());
        meshes_.remove(mesh_shared);
    }




    template <IsSpectral TSpectral>
    PointLightHandle<TSpectral> Scene<TSpectral>::new_point_light(TSpectral intensity, std::string name)
    {
        auto light_shared = std::make_shared<PointLight<TSpectral>>(intensity);
        lights_.add(light_shared, name);
        return PointLightHandle<TSpectral>{ light_shared };
    };

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_name(const PointLightHandle<TSpectral>& light_handle, const std::string& name)
    {
        lights_.set_name(light_handle.get(), name);
    };

    template <IsSpectral TSpectral>
    PointLightHandle<TSpectral> Scene<TSpectral>::get_point_light(const std::string& name)
    {
        auto light_base = lights_.lookup(name);
        auto point_light = std::dynamic_pointer_cast<PointLight<TSpectral>>(light_base);
        if (!point_light) {
            HUIRA_THROW_ERROR("Light with name '" + name + "' is not a PointLight.");
        }
        return PointLightHandle<TSpectral>{ point_light };
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_light(const PointLightHandle<TSpectral>& light_handle)
    {
        auto light_shared = light_handle.get();
        prune_graph_references_(static_cast<Light<TSpectral>*>(light_shared.get()));
        lights_.remove(light_shared);
    }
    


    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::new_unresolved_object(TSpectral irradiance, std::string name)
    {
        auto unresolved_shared = std::make_shared<UnresolvedObject<TSpectral>>(irradiance);
        unresolved_objects_.add(unresolved_shared, name);
        return UnresolvedObjectHandle<TSpectral>{ unresolved_shared };
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_name(const UnresolvedObjectHandle<TSpectral>& unresolved, const std::string& name)
    {
        unresolved_objects_.set_name(unresolved.get(), name);
    }

    template <IsSpectral TSpectral>
    UnresolvedObjectHandle<TSpectral> Scene<TSpectral>::get_unresolved_object(const std::string& name)
    {
        return UnresolvedObjectHandle<TSpectral>{ unresolved_objects_.lookup(name) };
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_unresolved_object(const UnresolvedObjectHandle<TSpectral>& unresolved_object_handle)
    {
        auto unresolved_object_shared = unresolved_object_handle.get();
        prune_graph_references_(unresolved_object_shared.get());
        unresolved_objects_.remove(unresolved_object_shared);
    }



    template <IsSpectral TSpectral>
    CameraModelHandle<TSpectral> Scene<TSpectral>::new_camera_model(std::string name)
    {
        auto camera_shared = std::make_shared<CameraModel<TSpectral>>();
        camera_models_.add(camera_shared, name);
        return CameraModelHandle<TSpectral>{ camera_shared };
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_name(const CameraModelHandle<TSpectral>& camera_model_handle, const std::string& name)
    {
        camera_models_.set_name(camera_model_handle.get(), name);
    }

    template <IsSpectral TSpectral>
    CameraModelHandle<TSpectral> Scene<TSpectral>::get_camera_model(const std::string& name)
    {
        return CameraModelHandle<TSpectral>{ camera_models_.lookup(name) };
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_camera_model(const CameraModelHandle<TSpectral>& camera_model_handle)
    {
        auto camera_model_shared = camera_model_handle.get();
        prune_graph_references_(camera_model_shared.get());
        camera_models_.remove(camera_model_shared);
    }



    template <IsSpectral TSpectral>
    ModelHandle<TSpectral> Scene<TSpectral>::load_model(const fs::path& file, std::string name, unsigned int post_process_flags)
    {
        // Load the model using ModelLoader
        auto model_shared = ModelLoader<TSpectral>::load(*this, file, name, post_process_flags);
        return ModelHandle<TSpectral>{ model_shared };
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_name(const ModelHandle<TSpectral>& model_handle, const std::string& name)
    {
        models_.set_name(model_handle.get(), name);
    }

    template <IsSpectral TSpectral>
    ModelHandle<TSpectral> Scene<TSpectral>::get_model(const std::string& name)
    {
        return ModelHandle<TSpectral>{ models_.lookup(name) };
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_model(const ModelHandle<TSpectral>& model_handle)
    {
        auto model_shared = model_handle.get();
        prune_graph_references_(model_shared.get());
        models_.remove(model_shared);
    }



    template <IsSpectral TSpectral>
    void Scene<TSpectral>::add_star(const Star<TSpectral>& star)
    {
        stars_.push_back(star);
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_stars(const std::vector<Star<TSpectral>>& stars)
    {
        stars_ = stars;
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::load_stars(const fs::path& star_catalog_path, const Time& time, float min_magnitude)
    {
        // Read the catalog:
        StarCatalog star_catalog = StarCatalog::read_star_data(star_catalog_path, min_magnitude);
        const std::vector<StarData>& star_data = star_catalog.get_star_data();

        double tsince = time.julian_years_since_j2000(TimeScale::TT);

        // Create the stars:
        std::vector<Star<TSpectral>> stars(star_data.size());
        tbb::parallel_for(
            tbb::blocked_range<std::size_t>(0, star_data.size()),
            [&](const tbb::blocked_range<std::size_t>& r) {
                for (std::size_t i = r.begin(); i != r.end(); ++i) {
                    stars[i] = Star<TSpectral>(star_data[i], tsince);
                }
            }
        );

        // Add the stars to the scene:
        set_stars(stars);
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::prune_unreferenced_assets()
    {
        
    }



    template <IsSpectral TSpectral>
    template <typename TAssetPtr>
    void Scene<TSpectral>::prune_graph_references_(TAssetPtr target_ptr)
    {
        std::function<void(Node<TSpectral>*)> prune_references =
            [&](Node<TSpectral>* parent)
            {
                std::vector<std::shared_ptr<Node<TSpectral>>> children_snapshot(
                    parent->get_children().begin(),
                    parent->get_children().end()
                );

                for (const auto& child_node : children_snapshot) {
                    bool deleted = false;

                    if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(child_node)) {
                        const auto& asset_var = instance->asset();

                        // We check if the variant holds specific pointer type:
                        if (std::holds_alternative<TAssetPtr>(asset_var)) {
                            // Compare the pointers
                            if (std::get<TAssetPtr>(asset_var) == target_ptr) {

                                if (auto* frame_parent = dynamic_cast<FrameNode<TSpectral>*>(parent)) {
                                    frame_parent->delete_child(child_node);
                                    deleted = true;
                                }
                                else {
                                    HUIRA_THROW_ERROR("Attempted to delete child from a non-FrameNode!");
                                }
                            }
                        }
                    }

                    // Recurse if the node itself wasn't deleted
                    if (!deleted) {
                        prune_references(child_node.get());
                    }
                }
            };
        if (root_node_) {
            prune_references(root_node_.get());
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_meshes() const {
        std::cout << green("Meshes: " + std::to_string(meshes_.size()) + " loaded") << "\n";
        for (const auto& mesh : meshes_) {
            std::cout << " - " << mesh->get_info();
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_lights() const {
        std::cout << yellow("Lights: " + std::to_string(lights_.size()) + " loaded") << "\n";
        for (const auto& light : lights_) {
            std::cout << " - " << light->get_info();
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_unresolved_objects() const {
        std::cout << cyan("UnresolvedObjects: " + std::to_string(unresolved_objects_.size()) + " loaded") << "\n";
        for (const auto& unresolved_object : unresolved_objects_) {
            std::cout << " - " << unresolved_object->get_info();
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_camera_models() const {
        std::cout << blue("CameraModels: " + std::to_string(camera_models_.size()) + " loaded") << "\n";
        for (const auto& camera_model : camera_models_) {
            std::cout << " - " << camera_model->get_info();
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_models() const {
        std::cout << magenta("Models: " + std::to_string(models_.size()) + " loaded") << "\n";
        for (const auto& model : models_) {
            std::cout << " - " << model->get_info();
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_graph() const {
        std::cout << on_blue("root");
        print_node_details_(root_node_.get());
        std::cout << "\n";

        const auto children = root_node_->get_children();
        for (size_t i = 0; i < children.size(); ++i) {
            bool is_last = (i == children.size() - 1);
            print_node_((children)[i].get(), "", is_last);
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_contents() const {
        std::cout << "Scene Contents:\n";
        std::cout << " - " << blue("CameraModels: " + std::to_string(camera_models_.size()) + " loaded") << "\n";
        std::cout << " - " << green("Meshes: " + std::to_string(meshes_.size()) + " loaded") << "\n";
        std::cout << " - " << yellow("Lights: " + std::to_string(lights_.size()) + " loaded") << "\n";
        std::cout << " - " << cyan("UnresolvedObjects: " + std::to_string(unresolved_objects_.size()) + " loaded") << "\n";
        std::cout << " - " << magenta("Models: " + std::to_string(models_.size()) + " loaded") << "\n";
        std::cout << "Scene Graph:\n";
        print_graph();
    }


    // ======================= //
    // === Private Members === //
    // ======================= //

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const {
        std::cout << prefix;
        std::cout << (is_last ? "+-- " : "|-- ");
        
        // Check if the node is an Instance
        if (const auto* instance_node = dynamic_cast<const Instance<TSpectral>*>(node)) {
            std::string instance_str = "Instance[" + std::to_string(instance_node->id()) + "]";
            instance_str += instance_node->name_.empty() ? "" : " " + instance_node->name_;
            std::cout << on_green(instance_str) << " -> ";
            std::visit([&](auto* raw_ptr) noexcept {
                using AssetType = std::decay_t<decltype(*raw_ptr)>;
                if constexpr (std::is_same_v<AssetType, Mesh<TSpectral>>) {
                    std::cout << green(raw_ptr->get_info());
                }
                else if constexpr (std::is_same_v<AssetType, Light<TSpectral>>) {
                    std::cout << yellow(raw_ptr->get_info());
                }
                else if constexpr (std::is_same_v<AssetType, UnresolvedObject<TSpectral>>) {
                    std::cout << cyan(raw_ptr->get_info());
                }
                else if constexpr (std::is_same_v<AssetType, CameraModel<TSpectral>>) {
                    std::cout << blue(raw_ptr->get_info());
                }
                else if constexpr (std::is_same_v<AssetType, Model<TSpectral>>) {
                    std::cout << magenta(raw_ptr->get_info());
                }
                }, instance_node->asset_);
        }
        else {
            std::cout << on_blue(node->get_info());
        }
        print_node_details_(node);
        std::cout << "\n";

        const std::string child_prefix = prefix + (is_last ? "    " : "|   ");

        const auto children = node->get_children();
        for (size_t i = 0; i < children.size(); ++i) {
            bool child_is_last = (i == children.size() - 1);
            print_node_((children)[i].get(), child_prefix, child_is_last);
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_node_details_(const Node<TSpectral>* node) const {
        if (node->spice_origin_ != "" || node->spice_frame_ != "") {
            std::cout << " (";
            if (node->spice_origin_ != "") {
                std::cout << node->spice_origin_;
                if (node->spice_frame_ != "") {
                    std::cout << ", ";
                }
            }

            if (node->spice_frame_ != "") {
                std::cout << node->spice_frame_;
            }
            std::cout << ")";
        }
    }


    /**
     * @brief Finds the shared_ptr for a given raw Node pointer.
     *
     * Recursively searches the scene graph starting from the root to find
     * the shared_ptr corresponding to the given raw pointer. This is needed
     * for creating handles to existing nodes (e.g., parent nodes).
     *
     * @param target The raw pointer to search for
     * @return std::shared_ptr<Node<TSpectral>> The shared_ptr if found, nullptr otherwise
     */
    template <IsSpectral TSpectral>
    std::shared_ptr<Node<TSpectral>> Scene<TSpectral>::find_node_shared_ptr_(const Node<TSpectral>* target) const {
        // Check if target is the root
        if (root_node_.get() == target) {
            return root_node_;
        }

        // Otherwise recursively search the tree
        return find_node_in_tree_(root_node_, target);
    }


    /**
     * @brief Recursively searches for a node in the scene graph tree.
     *
     * Helper function for find_node_shared_ptr_ that traverses the scene graph.
     *
     * @param current The current node being examined
     * @param target The raw pointer to search for
     * @return std::shared_ptr<Node<TSpectral>> The shared_ptr if found, nullptr otherwise
     */
    template <IsSpectral TSpectral>
    std::shared_ptr<Node<TSpectral>> Scene<TSpectral>::find_node_in_tree_(
        const std::shared_ptr<Node<TSpectral>>& current, 
        const Node<TSpectral>* target) const 
    {
        // Check if current node is the target
        if (current.get() == target) {
            return current;
        }

        // Get children if this node has any
        const auto children = current->get_children();
            for (const auto& child : children) {
                // Check this child
                if (child.get() == target) {
                    return child;
                }

                // Recursively search this child's subtree
                auto result = find_node_in_tree_(child, target);
                if (result) {
                    return result;
                }
            }

        return nullptr;
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::register_node_name_(const std::shared_ptr<Node<TSpectral>>& node, const std::string& name)
    {
        node_registry_.add(node, name);
    }
}
