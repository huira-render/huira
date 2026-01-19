#pragma once

#include <string>
#include <memory>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/scene/node.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene;

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class UnresolvedObject : public Node<TSpectral, TFloat> {
    public:
        UnresolvedObject(Scene<TSpectral, TFloat>* scene)
            : Node<TSpectral, TFloat>(scene)
        {
        }

        // Remove functionality from the base class that doesn't make sense for UnresolvedObject:
        std::weak_ptr<Node<TSpectral, TFloat>> new_child() = delete;
        void delete_child(std::weak_ptr<Node<TSpectral, TFloat>> child) = delete;
        void change_parent(std::weak_ptr<Node<TSpectral, TFloat>> self_weak, Node<TSpectral, TFloat>* new_parent) = delete;

        std::weak_ptr<UnresolvedObject<TSpectral, TFloat>> new_unresolved() = delete;


    protected:
        std::string get_type_name() const override { return "UnresolvedObject"; }

        friend class Scene<TSpectral, TFloat>;
    };
}

#include "huira_impl/scene/unresolved_object.ipp"
