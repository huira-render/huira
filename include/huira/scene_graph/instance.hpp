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

        std::string get_info() const override {
            return "Instance[" + std::to_string(this->id()) + "] " + this->name_ + " -> " +
                std::visit([](auto* ptr) { return ptr->get_info(); }, asset_);
        }

    private:
        Instantiable<TSpectral> asset_;
    };

}

#include "huira_impl/scene_graph/instance.ipp"
