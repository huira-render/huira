#pragma once

#include <variant>
#include <string>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/scene_graph/node.hpp"

namespace huira {
    // Forward declarations
    template <IsSpectral TSpectral>
    class Mesh;

    template <IsSpectral TSpectral>
    class Light;

    template <IsSpectral TSpectral>
    class Model;

    template <IsSpectral TSpectral>
    using Instantiable = std::variant<Mesh<TSpectral>*, Light<TSpectral>*, Model<TSpectral>*>;

    template <IsSpectral TSpectral>
    class Instance : public Node<TSpectral> {
    public:
        Instance(Scene<TSpectral>* scene, const Instantiable<TSpectral>& asset)
            : Node<TSpectral>(scene), asset_(asset) {}

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        const Instantiable<TSpectral>& asset() const { return asset_; }

        std::string get_info() const override;

    private:
        Instantiable<TSpectral> asset_;
    };

}

#include "huira_impl/scene_graph/instance.ipp"
