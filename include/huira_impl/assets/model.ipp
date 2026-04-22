#include <iostream>
#include <typeinfo>

#include "huira/util/logger.hpp"

namespace huira {
    /**
     * @brief Prints the model's scene graph hierarchy to stdout.
     * 
     * Outputs the source file path and a tree-structured visualization of the
     * model's node hierarchy starting from the root node.
     */
    template <IsSpectral TSpectral>
    void Model<TSpectral>::print_graph() const {
        std::cout << "Source: " << source_path_.string() << "\n";
        std::cout << "Model Graph:\n";

        if (root_node_) {
            print_node_(root_node_.get(), "", true);
        } else {
            std::cout << "  (empty)\n";
        }
    }

    /**
     * @brief Recursively prints a node and its children in tree format.
     * 
     * Helper method that prints a single node with appropriate tree characters
     * and recursively processes all child nodes.
     * 
     * @param node The node to print.
     * @param prefix The string prefix for tree indentation.
     * @param is_last Whether this node is the last child of its parent.
     */
    template <IsSpectral TSpectral>
    void Model<TSpectral>::print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const {
        if (!node) {
            return;
        }

        std::cout << prefix;
        std::cout << (is_last ? "+-- " : "|-- ");
        std::cout << node->get_info();

        std::cout << "\n";

        // Recurse into children
        auto children = node->get_children();
        for (std::size_t i = 0; i < children.size(); ++i) {
            bool child_is_last = (i == children.size() - 1);
            std::string child_prefix = prefix + (is_last ? "    " : "|   ");
            print_node_(children[i].get(), child_prefix, child_is_last);
        }
    }

    /**
     * @brief Searches the model's scene graph for a material with the specified ID.
     * @param material_id The ID of the material to search for.
     * @return MaterialHandle<TSpectral> Handle to the found material.
     * @throws std::runtime_error if the material is not found.
     */
    template <IsSpectral TSpectral>
    MaterialHandle<TSpectral> Model<TSpectral>::get_material_by_id(std::uint64_t material_id) const
    {
        if (!root_node_) {
            HUIRA_THROW_ERROR("Model has no root node.");
        }

        // Iterative Depth-First Search stack
        std::vector<const Node<TSpectral>*> stack;
        stack.push_back(root_node_.get());

        while (!stack.empty()) {
            const Node<TSpectral>* current = stack.back();
            stack.pop_back();

            // Check if this node is an Instance
            if (auto instance = dynamic_cast<const Instance<TSpectral>*>(current)) {

                // Peek inside the variant
                const auto& asset = instance->asset();

                if (std::holds_alternative<Primitive<TSpectral>*>(asset)) {
                    Primitive<TSpectral>* prim = std::get<Primitive<TSpectral>*>(asset);

                    // Does it have a material, and does the ID match?
                    if (prim && prim->material && prim->material->id() == material_id) {
                        return MaterialHandle<TSpectral>{ prim->material };
                    }
                }
            }

            // Push all children onto the stack to continue the search
            for (const auto& child : current->get_children()) {
                if (child) {
                    stack.push_back(child.get());
                }
            }
        }

        HUIRA_THROW_ERROR("Material ID " + std::to_string(material_id) + " not found in model graph.");
    }

    /**
     * @brief Sets the given BSDF on all materials used by meshes in the model.
     *
     * @param bsdf The BSDF to set on all materials. The BSDF will be cloned for each material.
     * @throws std::runtime_error if the model has no root node.
     */
    template <IsSpectral TSpectral>
    void Model<TSpectral>::set_all_bsdfs(const BSDF<TSpectral>& bsdf)
    {
        if (!root_node_) {
            HUIRA_THROW_ERROR("Model has no root node.");
        }

        // Iterative Depth-First Search stack
        std::vector<Node<TSpectral>*> stack;
        stack.push_back(root_node_.get());
        while (!stack.empty()) {
            Node<TSpectral>* current = stack.back();
            stack.pop_back();
            // Check if this node is an Instance
            if (auto instance = dynamic_cast<Instance<TSpectral>*>(current)) {
                // Peek inside the variant
                const auto& asset = instance->asset();
                if (std::holds_alternative<Primitive<TSpectral>*>(asset)) {
                    Primitive<TSpectral>* prim = std::get<Primitive<TSpectral>*>(asset);

                    // Route the BSDF update through the Primitive's shared material
                    if (prim && prim->material) {
                        prim->material->set_bsdf(bsdf);
                    }
                }
            }
            // Push all children onto the stack to continue the search
            for (const auto& child : current->get_children()) {
                if (child) {
                    stack.push_back(child.get());
                }
            }
        }
    }
}
