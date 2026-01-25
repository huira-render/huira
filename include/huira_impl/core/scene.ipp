#include <stdexcept>
#include <iostream>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/frame_handle.hpp"
#include "huira/detail/logger.hpp"
#include "huira/detail/text/colors.hpp"

namespace huira {

    template <IsSpectral TSpectral>
    MeshHandle<TSpectral> Scene<TSpectral>::add_mesh(Mesh<TSpectral>&& mesh)
    {
        auto ptr = std::make_shared<Mesh<TSpectral>>(std::move(mesh));
        meshes_.push_back(ptr);
        return MeshHandle<TSpectral>{ ptr };
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

        // We use std::function to allow the lambda to call itself recursively
        std::function<void(Node<TSpectral>*)> prune_references =
            [&](Node<TSpectral>* parent)
            {
                std::vector<std::shared_ptr<Node<TSpectral>>> children_copy(
                    parent->get_children().begin(),
                    parent->get_children().end()
                );

                for (const auto& child_node : children_copy) {
                    bool deleted = false;

                    // Is this node an Instance?
                    if (auto instance = std::dynamic_pointer_cast<Instance<TSpectral>>(child_node)) {
                        const auto& asset_var = instance->asset();
                        if (std::holds_alternative<Mesh<TSpectral>*>(asset_var)) {
                            if (std::get<Mesh<TSpectral>*>(asset_var) == target_ptr) {
                                
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

                    if (!deleted) {
                        if (child_node.get() == nullptr) {
                            std::cout << "hello there";
                        }
                        prune_references(child_node.get());
                    }
                }
                
            };

        prune_references(root_node_.get());

        meshes_.erase(it);
    };


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
    void Scene<TSpectral>::print_meshes() const {
        if (meshes_.size() == 0) {
            std::cout << detail::red("No Meshes Loaded") << "\n";
        }
        else {
            std::cout << "Meshes: (" << std::to_string(meshes_.size()) << " loaded)\n";
            for (size_t i = 0; i < meshes_.size(); ++i) {
                std::cout << " - " << detail::green("Mesh") << "[" << std::to_string(meshes_[i]->id()) << "] ";
                std::cout << "(" << std::to_string(meshes_[i]->get_index_count()) << " vertices)\n";
            }
        }
        std::cout << "\n";
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_graph() const {
        std::cout << detail::blue("root ");
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
            std::cout << detail::on_green(node->get_type_name()) << "[" << node->id() << "] ";
            std::string asset_name = instance_node->get_asset_name();
            if (asset_name.starts_with("Mesh")) {
                std::cout << "-> " << detail::green(asset_name);
            }
            else {
                std::cout << "-> " << detail::yellow(asset_name);
            }
            
        }
        else if (const auto* unresolved_node = dynamic_cast<const UnresolvedObject<TSpectral>*>(node)) {
            std::cout << detail::on_cyan(node->get_type_name()) << "[" << node->id() << "] ";
        }
        else if (const auto* camera_node = dynamic_cast<const Camera<TSpectral>*>(node)) {
            std::cout << detail::on_magenta(node->get_type_name()) << "[" << node->id() << "]";
        }
        else {
            std::cout << detail::blue(node->get_type_name()) << "[" << node->id() << "] ";
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
