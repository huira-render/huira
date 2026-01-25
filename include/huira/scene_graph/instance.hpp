#pragma once

#include <variant>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/scene_graph/node.hpp"

namespace huira {
    // Forward Declarations
    template <IsSpectral TSpectral>
    class Mesh;

    template <IsSpectral TSpectral>
    class Light;

    template <IsSpectral TSpectral>
    class UnresolvedObject;

    template <IsSpectral TSpectral>
    using Instantiable = std::variant<Mesh<TSpectral>*, Light<TSpectral>*, UnresolvedObject<TSpectral>*>;



    template <IsSpectral TSpectral>
    class Instance : public Node<TSpectral> {
    public:
        Instance(const Instantiable<TSpectral>& asset) : asset_(asset) {}

    private:
        Instantiable<TSpectral> asset_;
    };

}

#include "huira_impl/scene_graph/instance.ipp"
