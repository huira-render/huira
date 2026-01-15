#include <memory>
#include <string>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Node<TSpectral, TFloat>::Node(Scene<TSpectral, TFloat>* scene)
        : scene_(scene)
    {

    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::weak_ptr<Node<TSpectral, TFloat>> Node<TSpectral, TFloat>::new_child(std::string name)
    {
        if (scene_->is_locked()) {
            throw std::runtime_error("Attempted to add a Node to a locked scene");
        }

        auto child = std::make_shared<Node<TSpectral, TFloat>>(scene_);
        child->set_parent(this);

        scene_->add_node_name(name, child);

        children_.push_back(child);
        return child;
    }
}
