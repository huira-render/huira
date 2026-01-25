#include <stdexcept>
#include <iostream>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/assets/io/model_loader.hpp"
#include "huira/handles/model_handle.hpp"
#include "huira/handles/frame_handle.hpp"
#include "huira/detail/logger.hpp"
#include "huira/detail/text/colors.hpp"

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
    MeshHandle<TSpectral> Scene<TSpectral>::add_mesh(Mesh<TSpectral>&& mesh)
    {
        auto mesh_shared = std::make_shared<Mesh<TSpectral>>(std::move(mesh));
        meshes_.push_back(mesh_shared);
        HUIRA_LOG_INFO("Scene - Mesh added: Mesh[" + std::to_string(mesh_shared->id()) + "]");
        return MeshHandle<TSpectral>{ mesh_shared };
    };

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_mesh(const MeshHandle<TSpectral>& mesh_handle)
    {
        auto mesh_shared = mesh_handle.get();
        Mesh<TSpectral>* target_ptr = mesh_shared.get();

        auto it = std::find(meshes_.begin(), meshes_.end(), mesh_shared);
        if (it == meshes_.end()) {
            HUIRA_THROW_ERROR("Mesh does not exist in the scene");
        }

        HUIRA_LOG_INFO("Requested to delete Mesh[" + std::to_string(target_ptr->id()) + "]");

        prune_graph_references_(target_ptr);

        meshes_.erase(it);
    }

    template <IsSpectral TSpectral>
    PointLightHandle<TSpectral> Scene<TSpectral>::new_point_light(TSpectral intensity)
    {
        auto light_shared = std::make_shared<PointLight<TSpectral>>(intensity);
        lights_.push_back(light_shared);
        return PointLightHandle<TSpectral>{ light_shared };
    };

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_light(const PointLightHandle<TSpectral>& light_handle)
    {
        auto light_shared = light_handle.get();
        Light<TSpectral>* target_ptr = light_shared.get();

        auto it = std::find(lights_.begin(), lights_.end(), light_shared);
        if (it == lights_.end()) {
            HUIRA_THROW_ERROR("Light does not exist in the scene");
        }

        HUIRA_LOG_INFO("Requested to delete Light[" + std::to_string(target_ptr->id()) + "]");

        prune_graph_references_(target_ptr);

        lights_.erase(it);
    }

    template <IsSpectral TSpectral>
    ModelHandle<TSpectral> Scene<TSpectral>::load_model(const fs::path& file, unsigned int post_process_flags)
    {
        // Load the model using ModelLoader
        auto [model_shared, new_meshes] = ModelLoader<TSpectral>::load(file, post_process_flags);
        models_.push_back(model_shared);
        HUIRA_LOG_INFO("Scene - Model loaded: " + model_shared->get_info());
        // Add new meshes to the scene's mesh list
        for (const auto& mesh : new_meshes) {
            meshes_.push_back(mesh);
            HUIRA_LOG_INFO("Scene - Mesh added from Model: Mesh[" + std::to_string(mesh->id()) + "]");
        }
        return ModelHandle<TSpectral>{ model_shared };
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::delete_model(const ModelHandle<TSpectral>& model_handle)
    {
        auto model_shared = model_handle.get();
        Model<TSpectral>* target_ptr = model_shared.get();

        auto it = std::find(models_.begin(), models_.end(), model_shared);
        if (it == models_.end()) {
            HUIRA_THROW_ERROR("Model does not exist in the scene");
        }

        HUIRA_LOG_INFO("Requested to delete Model[" + std::to_string(target_ptr->id()) + "]");

        //prune_graph_references_(target_ptr);

        models_.erase(it);
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::prune_unreferenced_assets()
    {
        
    }



    template <IsSpectral TSpectral>
    template <typename TAssetPtr>
    void Scene<TSpectral>::prune_graph_references_(TAssetPtr target_ptr)
    {
        // 1. Define the recursive pruning lambda genericly
        std::function<void(Node<TSpectral>*)> prune_references =
            [&](Node<TSpectral>* parent)
            {
                // Snapshot children (Deep copy for safety)
                std::vector<std::shared_ptr<Node<TSpectral>>> children_snapshot(
                    parent->get_children().begin(),
                    parent->get_children().end()
                );

                for (const auto& child_node : children_snapshot) {
                    bool deleted = false;

                    // Is this node an Instance?
                    if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(child_node)) {
                        const auto& asset_var = instance->asset();

                        // --- GENERIC CHECK START ---
                        // We check if the variant holds our specific pointer type
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
                        // --- GENERIC CHECK END ---
                    }

                    // Recurse if the node itself wasn't deleted
                    if (!deleted) {
                        prune_references(child_node.get());
                    }
                }
            };

        // 2. Kick off the traversal from the root
        if (root_node_) {
            prune_references(root_node_.get());
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_meshes() const {
        if (meshes_.size() == 0) {
            std::cout << detail::red("No Meshes Loaded") << "\n";
        }
        else {
            std::cout << detail::green("Meshes: (" + std::to_string(meshes_.size()) + " loaded)") << "\n";
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_lights() const {
        if (lights_.size() == 0) {
            std::cout << detail::red("No Lights Loaded") << "\n";
        }
        else {
            std::cout << detail::yellow("Lights: (" + std::to_string(lights_.size()) + " loaded)") << "\n";
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_models() const {
        if (models_.size() == 0) {
            std::cout << detail::red("No Models Loaded") << "\n";
        }
        else {
            std::cout << detail::magenta("Models: (" + std::to_string(models_.size()) + " loaded)") << "\n";
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_graph() const {
        std::cout << detail::blue("root") << " ";
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
        print_meshes();
        print_lights();
        print_models();
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
            std::cout << detail::on_green(instance_node->get_info());
            
        }
        else if (const auto* unresolved_node = dynamic_cast<const UnresolvedObject<TSpectral>*>(node)) {
            std::cout << detail::on_cyan(unresolved_node->get_info());
        }
        else if (const auto* camera_node = dynamic_cast<const Camera<TSpectral>*>(node)) {
            std::cout << detail::on_magenta(camera_node->get_info());
        }
        else {
            std::cout << detail::blue(node->get_info());
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
            std::cout << "(";
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
}
