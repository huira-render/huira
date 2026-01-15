#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/scene/handle.hpp"
#include "huira/scene/nodes.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class NodeHandle : public Handle<Node<TSpectral, TFloat>> {
    public:
        NodeHandle() = delete;
        using Handle<Node<TSpectral, TFloat>>::Handle;

        NodeHandle<TSpectral, TFloat> new_child(std::string name = "") const
        {
            return NodeHandle<TSpectral, TFloat>{ this->get()->new_child(name), this->scene_locked_ };
        }

        const std::string& name() const
        {
            return this->get()->name();
        }
    };
}
