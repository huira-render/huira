#pragma once

#include <vector>
#include <memory>
#include <string>
#include <stdexcept>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene;


    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Node {
    public:
        Node(Scene<TSpectral, TFloat>* scene);

        std::weak_ptr<Node<TSpectral, TFloat>> new_child(std::string name = "");

        const std::string& name() const { return scene_->name_of(this); }

    protected:
        void set_parent(Node<TSpectral, TFloat>* parent) { parent_ = parent; }

    private:
        Scene<TSpectral, TFloat>* scene_;
        Node<TSpectral, TFloat>* parent_ = nullptr;

        std::vector<std::shared_ptr<Node<TSpectral, TFloat>>> children_;
    };
}

#include "huira_impl/scene/nodes.ipp"
