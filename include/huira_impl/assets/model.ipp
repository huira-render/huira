#include <iostream>
#include <typeinfo>

namespace huira {

    template <IsSpectral TSpectral>
    void Model<TSpectral>::print_graph() const {
        std::cout << "Model: " << name_ << "\n";
        std::cout << "Source: " << source_path_.string() << "\n";
        std::cout << "Model Graph:\n";

        if (root_node_) {
            print_node_(root_node_.get(), "", true);
        } else {
            std::cout << "  (empty)\n";
        }
    }

    template <IsSpectral TSpectral>
    void Model<TSpectral>::print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const {
        if (!node) return;

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
