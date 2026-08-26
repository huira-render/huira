#include <string>

#include "huira/assets/lights/light.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/geometry/mesh.hpp"
#include "huira/scene/state_callbacks/look_at.hpp"

namespace huira {

/**
 * @brief Designates this instance as an indirect illumination source (reflector).
 *
 * Designated instances are directly sampled during next event estimation:
 * they do not emit light, but their (sun)lit surfaces are importance-sampled
 * as if they were light sources, which is essential for capturing e.g.
 * earthshine or moonshine that undirected path sampling would rarely find.
 *
 * Instances containing a Primitive or a Model may be designated. A designated
 * Model acts as a single source: the sampling proxy bounds every Primitive in
 * its sub-graph, and a sample landing on any of them counts as a hit on this
 * source. Lights, camera models and unresolved objects cannot be designated.
 *
 * @param enabled Whether this instance acts as an indirect source (default true).
 * @throws std::runtime_error if enabled and the instance contains neither a
 *         Primitive nor a Model.
 */
template <IsSpectral TSpectral>
void Instance<TSpectral>::set_indirect_source(bool enabled)
{
    if (enabled && !std::holds_alternative<Primitive<TSpectral>*>(asset_) &&
        !std::holds_alternative<Model<TSpectral>*>(asset_)) {
        HUIRA_THROW_ERROR("Instance::set_indirect_source - Only Instances containing a "
                          "Primitive or a Model can be designated as indirect sources");
    }
    indirect_source_ = enabled;
}

/**
 * @brief Get a descriptive string for this instance, including asset info.
 * @return std::string Info string
 */
template <IsSpectral TSpectral>
std::string Instance<TSpectral>::get_info() const
{
    return "Instance[" + std::to_string(this->id()) + "]" +
           (this->name().empty() ? "" : " " + this->name()) + " -> " +
           std::visit(
               [](auto* ptr) -> std::string {
                   if constexpr (std::is_same_v<decltype(ptr), Mesh<TSpectral>*>) {
                       std::string info = ptr->get_info();
                       if (ptr->material()) {
                           info += " -> " + ptr->material()->get_info();
                       }
                       return info;
                   } else {
                       return ptr->get_info();
                   }
               },
               asset_);
}

/**
 * @brief Orient the instance to look at a target node, with an optional up vector.
 * @param target Target node to look at
 * @param up Up vector for orientation (default: {0, 0, 1})
 */
template <IsSpectral TSpectral>
void Instance<TSpectral>::look_at(const Node<TSpectral>& target, const Vec3<double>& up)
{
    bool is_blender = false;

    // Check if the wrapped asset is a camera to determine conventions
    if (std::holds_alternative<CameraModel<TSpectral>*>(asset_)) {
        auto* camera = std::get<CameraModel<TSpectral>*>(asset_);
        is_blender = camera->is_blender_convention();
    }

    // Set the node to defer rotation evaluation to our new callback
    this->template set_custom_rotation_callback<LookAtCallback<TSpectral>>(
        this, &target, up, is_blender);
}

/**
 * @brief Orient the instance to look at a target position, with an optional up vector.
 * @param target_position Target position to look at
 * @param up Up vector for orientation (default: {0, 0, 1})
 */
template <IsSpectral TSpectral>
void Instance<TSpectral>::look_at(const Vec3<double>& target_position, const Vec3<double>& up)
{
    bool is_blender = false;
    if (std::holds_alternative<CameraModel<TSpectral>*>(asset_)) {
        is_blender = std::get<CameraModel<TSpectral>*>(asset_)->is_blender_convention();
    }

    this->template set_custom_rotation_callback<LookAtPositionCallback<TSpectral>>(
        this, target_position, up, is_blender);
}

} // namespace huira
