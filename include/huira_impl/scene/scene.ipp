#include <stdexcept>
#include <iostream>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Scene<TSpectral, TFloat>::Scene()
    {
        root_node_ = std::make_unique<Node<TSpectral, TFloat>>(this);

        // Manually configure as a SPICE frame:
        root_node_->spice_origin_ = "SOLAR SYSTEM BARYCENTER";
        root_node_->position_source_ = TransformSource::Spice;
        root_node_->spice_ref_ = "ICRF";
        root_node_->orientation_source_ = TransformSource::Spice;
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    NodeHandle<TSpectral, TFloat> Scene<TSpectral, TFloat>::new_node(std::string name)
    {
        std::weak_ptr<Node<TSpectral, TFloat>> node = this->root_node_->new_child(name);
        return NodeHandle<TSpectral, TFloat>{ node, & locked_ };
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Scene<TSpectral, TFloat>::add_node_name(const std::string& name, std::weak_ptr<Node<TSpectral, TFloat>> node)
    {
        if (name.empty()) {
            return;
        }

        if (node_names_.contains(name)) {
            throw std::runtime_error("A Node already exists with the name: " + name);
        }
        node_names_[name] = node;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    NodeHandle<TSpectral, TFloat> Scene<TSpectral, TFloat>::get_named_node(const std::string& name)
    {
        if (auto it = node_names_.find(name); it != node_names_.end()) {
            return NodeHandle<TSpectral, TFloat>{ it->second, & locked_ };
        }
        throw std::runtime_error("No Node named: " + name);
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    const std::string& Scene<TSpectral, TFloat>::name_of(const Node<TSpectral, TFloat>* node) const {
        for (const auto& [name, ptr] : node_names_) {
            if (auto sptr = ptr.lock()) {
                if (sptr.get() == node) {
                    return name;
                }
            }
        }
        static const std::string empty_string;
        return empty_string;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Scene<TSpectral, TFloat>::print_node_details(const Node<TSpectral, TFloat>* node) const {
        // Check for registered name
        bool name_found = false;
        for (const auto& [name, ptr] : node_names_) {
            if (auto sptr = ptr.lock()) {
                if (sptr.get() == node) {
                    std::cout << "(name: " << name;
                    name_found = true;
                    break;
                }
            }
        }

        if (node->spice_origin_ != "" || node->spice_ref_ != "") {
            if (name_found) {
                std::cout << ", ";
            }
            else {
                std::cout << "(";
            }
            if (node->spice_origin_ != "") {
                std::cout << "origin: " << node->spice_origin_;
                if (node->spice_ref_ != "") {
                    std::cout << ", ";
                }
            }

            if (node->spice_ref_ != "") {
                std::cout << "ref: " << node->spice_ref_;
            }
            std::cout << ")";
        }
        else {
            if (name_found) {
                std::cout << ")";
            }
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Scene<TSpectral, TFloat>::print_graph() const {
        std::cout << "root ";
        print_node_details(root_node_.get());
        std::cout << "\n";

        const auto& children = root_node_->children_;
        for (size_t i = 0; i < children.size(); ++i) {
            bool is_last = (i == children.size() - 1);
            print_node(children[i].get(), "", is_last);
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Scene<TSpectral, TFloat>::print_node(const Node<TSpectral, TFloat>* node, const std::string& prefix, bool is_last) const {
        // Print current node
        std::cout << prefix;
        std::cout << (is_last ? "+-- " : "+-- ");
        std::cout << "Node ";
        print_node_details(node);
        std::cout << "\n";

        // Recurse into children with updated prefix
        const std::string child_prefix = prefix + (is_last ? "    " : "|   ");
        const auto& children = node->children_;

        for (size_t i = 0; i < children.size(); ++i) {
            bool child_is_last = (i == children.size() - 1);
            print_node(children[i].get(), child_prefix, child_is_last);
        }
    }
}

