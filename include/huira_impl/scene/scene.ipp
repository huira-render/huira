#include <stdexcept>
#include <iostream>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/frame_handle.hpp"
#include "huira/detail/logger.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Scene<TSpectral, TFloat>::Scene() : time_(Time::from_et(0))
    {
        root_node_ = std::make_unique<Node<TSpectral, TFloat>>(this);
        root_node_->set_spice("SOLAR SYSTEM BARYCENTER", "J2000");
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Scene<TSpectral, TFloat>::set_time(const Time& time) {
        if (locked_) {
            throw std::runtime_error("Scene::set_time() was called on a locked scene");
        }
        time_ = time;

        HUIRA_LOG_INFO("Scene time set to " + time.to_utc_string() + " (et = " + std::to_string(time.et()) + ")");
        
        root_node_->update_all_spice_transforms_();
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    FrameHandle<TSpectral, TFloat> Scene<TSpectral, TFloat>::new_subframe(std::string name)
    {
        return FrameHandle<TSpectral, TFloat>{ this->root_node_->new_child(name) };
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    FrameHandle<TSpectral, TFloat> Scene<TSpectral, TFloat>::new_spice_subframe(const std::string& spice_origin, const std::string& spice_frame, std::string name)
    {
        FrameHandle<TSpectral, TFloat> subframe = this->new_subframe(name);
        subframe.set_spice(spice_origin, spice_frame);
        return subframe;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    FrameHandle<TSpectral, TFloat> Scene<TSpectral, TFloat>::get_named_frame(const std::string& name)
    {
        if (auto it = node_names_.find(name); it != node_names_.end()) {
            return FrameHandle<TSpectral, TFloat>{ it->second };
        }
        throw std::runtime_error("No Node named: " + name);
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Scene<TSpectral, TFloat>::print_graph() const {
        std::cout << "root ";
        print_node_details_(root_node_.get());
        std::cout << "\n";

        const auto& children = root_node_->children_;
        for (size_t i = 0; i < children.size(); ++i) {
            bool is_last = (i == children.size() - 1);
            print_node_(children[i].get(), "", is_last);
        }
    }



    // ======================= //
    // === Private Members === //
    // ======================= //

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Scene<TSpectral, TFloat>::add_node_name_(const std::string& name, std::weak_ptr<Node<TSpectral, TFloat>> node)
    {
        if (name.empty()) {
            return;
        }

        if (node_names_.contains(name)) {
            HUIRA_THROW_ERROR("A Node already exists with the name: " + name);
        }
        node_names_[name] = node;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    const std::string& Scene<TSpectral, TFloat>::name_of_node_(const Node<TSpectral, TFloat>* node) const {
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
    void Scene<TSpectral, TFloat>::print_node_(const Node<TSpectral, TFloat>* node, const std::string& prefix, bool is_last) const {
        // Print current node
        std::cout << prefix;
        std::cout << (is_last ? "+-- " : "|-- ");
        std::cout << "Node ";
        print_node_details_(node);
        std::cout << "\n";

        // Recurse into children with updated prefix
        const std::string child_prefix = prefix + (is_last ? "    " : "|   ");
        const auto& children = node->children_;

        for (size_t i = 0; i < children.size(); ++i) {
            bool child_is_last = (i == children.size() - 1);
            print_node_(children[i].get(), child_prefix, child_is_last);
        }
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void Scene<TSpectral, TFloat>::print_node_details_(const Node<TSpectral, TFloat>* node) const {
        // Print node ID
        std::cout << "[" << node->id() << "] ";

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

        if (node->spice_origin_ != "" || node->spice_frame_ != "") {
            if (name_found) {
                std::cout << ", ";
            }
            else {
                std::cout << "(";
            }
            if (node->spice_origin_ != "") {
                std::cout << "origin: " << node->spice_origin_;
                if (node->spice_frame_ != "") {
                    std::cout << ", ";
                }
            }

            if (node->spice_frame_ != "") {
                std::cout << "frame: " << node->spice_frame_;
            }
            std::cout << ")";
        }
        else {
            if (name_found) {
                std::cout << ")";
            }
        }
    }
}

