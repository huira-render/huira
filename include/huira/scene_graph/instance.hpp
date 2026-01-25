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

        std::string get_asset_name() const
        {
            std::string asset_name;
            std::visit([&asset_name](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Mesh<TSpectral>*>) {
                    asset_name += "Mesh[" + std::to_string(arg->id()) + "]";
                }
                else if constexpr (std::is_same_v<T, Light<TSpectral>*>) {
                    asset_name += "Light[" + std::to_string(arg->id()) + "]";
                }
                }, asset_);
            return asset_name;
        }

        std::string get_info() const override
        {
            std::string info = "Instance[" + std::to_string(this->id()) + "] -> ";
            info += get_asset_name();
            return info;
        }
        std::string get_type_name() const override { return "Instance"; }

    private:
        Instantiable<TSpectral> asset_;
    };

}

#include "huira_impl/scene_graph/instance.ipp"
