#include <stdexcept>
#include <iostream>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/frame_handle.hpp"
#include "huira/detail/logger.hpp"

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
        , time_(Time::from_et(0))
    {
        root_node_->set_spice("SOLAR SYSTEM BARYCENTER", "J2000");
    };

#ifdef _MSC_VER
#pragma warning(pop)
#endif

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::set_time(const Time& time) {
        if (locked_) {
            throw std::runtime_error("Scene::set_time() was called on a locked scene");
        }
        time_ = time;

        HUIRA_LOG_INFO("Scene time set to " + time.to_utc_string() + " (et = " + std::to_string(time.et()) + ")");

        root_node_->update_all_spice_transforms_();
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_graph() const {
        std::cout << "root ";
        print_node_details_(root_node_.get());
        std::cout << "\n";

        const auto* children = root_node_->get_children_();
        if (children) {
            for (size_t i = 0; i < children->size(); ++i) {
                bool is_last = (i == children->size() - 1);
                print_node_((*children)[i].get(), "", is_last);
            }
        }
    }


    // ======================= //
    // === Private Members === //
    // ======================= //

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_node_(const Node<TSpectral>* node, const std::string& prefix, bool is_last) const {
        std::cout << prefix;
        std::cout << (is_last ? "+-- " : "|-- ");
        std::cout << node->get_type_name() + " ";
        print_node_details_(node);
        std::cout << "\n";

        const std::string child_prefix = prefix + (is_last ? "    " : "|   ");
        const auto* children = node->get_children_();  // Virtual dispatch handles it

        if (children) {
            for (size_t i = 0; i < children->size(); ++i) {
                bool child_is_last = (i == children->size() - 1);
                print_node_((*children)[i].get(), child_prefix, child_is_last);
            }
        }
    }

    template <IsSpectral TSpectral>
    void Scene<TSpectral>::print_node_details_(const Node<TSpectral>* node) const {
        // Print node ID
        std::cout << "[" << node->id() << "] ";

        if (node->spice_origin_ != "" || node->spice_frame_ != "") {
            std::cout << "(";
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
    }
}
