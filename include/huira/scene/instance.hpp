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

    /**
     * @brief Scene graph node representing an instantiable asset (mesh, light, unresolved object, camera model, or model).
     *
     * Instance nodes are leaf nodes in the scene graph and wrap a single asset pointer.
     *
     * @tparam TSpectral Spectral type (e.g., RGB, Spectral)
     */
    template <IsSpectral TSpectral>
    class Instance : public Node<TSpectral> {
    public:
        /**
         * @brief Construct an Instance node for a given asset.
         * @param scene Pointer to the owning Scene
         * @param asset Asset to wrap (mesh, light, etc.)
         */
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
