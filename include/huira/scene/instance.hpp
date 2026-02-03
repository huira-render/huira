#pragma once

#include <variant>
#include <string>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/node.hpp"

namespace huira {
    // Forward declarations
    template <IsSpectral TSpectral>
    class Scene;

    template <IsSpectral TSpectral>
    class SceneView;

    template <IsSpectral TSpectral>
    class Mesh;

    template <IsSpectral TSpectral>
    class Light;

    template <IsSpectral TSpectral>
    class UnresolvedObject;

    template <IsSpectral TSpectral>
    class CameraModel;

    template <IsSpectral TSpectral>
    class Model;

    template <IsSpectral TSpectral>
    using Instantiable = std::variant<
        Mesh<TSpectral>*,
        Light<TSpectral>*,
        UnresolvedObject<TSpectral>*,
        CameraModel<TSpectral>*,
        Model<TSpectral>*
    >;

    template <IsSpectral TSpectral>
    class Instance : public Node<TSpectral> {
    public:
        Instance(Scene<TSpectral>* scene, const Instantiable<TSpectral>& asset)
            : Node<TSpectral>(scene), asset_(asset) {}

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;

        const Instantiable<TSpectral>& asset() const { return asset_; }

        std::string type() const override { return "Instance"; }

        std::string get_info() const override;

    private:
        Instantiable<TSpectral> asset_;

        friend class Scene<TSpectral>;
        friend class SceneView<TSpectral>;
    };

}

#include "huira_impl/scene/instance.ipp"
