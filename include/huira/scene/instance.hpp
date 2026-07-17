#pragma once

#include <string>
#include <variant>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/scene/node.hpp"
#include "huira/util/logger.hpp"

namespace huira {
// Forward declarations
template <IsSpectral TSpectral>
class Scene;

template <IsSpectral TSpectral>
class SceneView;

template <IsSpectral TSpectral>
class Primitive;

template <IsSpectral TSpectral>
class Light;

template <IsSpectral TSpectral>
class UnresolvedObject;

template <IsSpectral TSpectral>
class CameraModel;

template <IsSpectral TSpectral>
class Model;

template <IsSpectral TSpectral>
using Instantiable = std::variant<Primitive<TSpectral>*,
                                  Light<TSpectral>*,
                                  UnresolvedObject<TSpectral>*,
                                  CameraModel<TSpectral>*,
                                  Model<TSpectral>*>;

/**
 * @brief Scene graph node representing an instantiable asset (mesh, light, unresolved object,
 * camera model, or model).
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
        : Node<TSpectral>(scene), asset_(asset)
    {
    }

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;

    const Instantiable<TSpectral>& asset() const { return asset_; }

    /**
     * @brief Designates this instance as an indirect illumination source (reflector).
     *
     * Designated instances are directly sampled during next event estimation:
     * they do not emit light, but their (sun)lit surfaces are importance-sampled
     * as if they were light sources, which is essential for capturing e.g.
     * earthshine or moonshine that undirected path sampling would rarely find.
     * Only Instances containing a Primitive may be designated.
     *
     * @param enabled Whether this instance acts as an indirect source (default true).
     * @throws std::runtime_error if enabled and the instance does not contain a Primitive.
     */
    void set_indirect_source(bool enabled = true)
    {
        if (enabled && !std::holds_alternative<Primitive<TSpectral>*>(asset_)) {
            HUIRA_THROW_ERROR("Instance::set_indirect_source - Only Instances containing a "
                              "Primitive can be designated as indirect sources");
        }
        indirect_source_ = enabled;
    }

    /// Whether this instance is designated as an indirect illumination source.
    bool is_indirect_source() const { return indirect_source_; }

    std::string type() const override { return "Instance"; }

    std::string get_info() const override;

    void look_at(const Node<TSpectral>& target, const Vec3<double>& up = {0.0, 0.0, 1.0});
    void look_at(const Vec3<double>& target_position, const Vec3<double>& up = {0.0, 0.0, 1.0});

  private:
    Instantiable<TSpectral> asset_;
    bool indirect_source_ = false;

    friend class Scene<TSpectral>;
    friend class SceneView<TSpectral>;
};
} // namespace huira

#include "huira_impl/scene/instance.ipp"
