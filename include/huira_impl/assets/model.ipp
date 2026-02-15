#include <iostream>
#include <typeinfo>

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

}
