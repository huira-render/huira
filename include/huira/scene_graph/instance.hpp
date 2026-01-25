#pragma once

#include <variant>

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/scene_graph/node.hpp"

#include "huira/assets/mesh.hpp"
#include "huira/assets/lights/light.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    using Instantiable = std::variant<Mesh<TSpectral>*, Light<TSpectral>*>;



    template <IsSpectral TSpectral>
    class Instance : public Node<TSpectral> {
    public:
        Instance(Scene<TSpectral>* scene, const Instantiable<TSpectral>& asset)
            : Node<TSpectral>(scene), asset_(asset) {}

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        const Instantiable<TSpectral>& asset() const { return asset_; }

        std::string get_asset_name() const;
        std::string get_info() const override;
        std::string get_type_name() const override { return "Instance"; }

    private:
        Instantiable<TSpectral> asset_;
    };

}

#include "huira_impl/scene_graph/instance.ipp"
