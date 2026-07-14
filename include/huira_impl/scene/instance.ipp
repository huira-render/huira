#include <string>

#include "huira/assets/lights/light.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/geometry/mesh.hpp"
#include "huira/scene/state_callbacks/look_at.hpp"

namespace huira {

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
