#include <string>

#include "huira/assets/lights/light.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/geometry/mesh.hpp"
#include "huira/scene/state_callbacks/look_at.hpp"
#include "huira/util/validate.hpp"

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
 *
 * The target is followed through the exposure. It is aimed at where it is seen from this
 * instance in an observation mode: by default the scene view's, so that a camera keeps the
 * target where the scene view renders it, or the one given, to aim by another model (where the
 * target truly is, say, while rendering where it appears).
 *
 * @param target Target node to look at
 * @param up Up vector for orientation (default: {0, 0, 1})
 * @param observation_mode The observation mode to aim in, or none (the default) for the scene
 *        view's
 * @throws std::runtime_error if the target is this instance, or up is not finite.
 */
template <IsSpectral TSpectral>
void Instance<TSpectral>::look_at(const Node<TSpectral>& target,
                                  const Vec3<double>& up,
                                  std::optional<ObservationMode> observation_mode)
{
    if (&target == this) {
        HUIRA_THROW_ERROR(this->get_info() + " - An instance cannot look at itself");
    }
    validate_real(up, this->get_info() + " up vector");
    bool is_blender = false;

    // Check if the wrapped asset is a camera to determine conventions
    if (std::holds_alternative<CameraModel<TSpectral>*>(asset_)) {
        auto* camera = std::get<CameraModel<TSpectral>*>(asset_);
        is_blender = camera->uses_blender_convention();
    }

    // Set the node to defer rotation evaluation to our new callback
    this->template set_custom_rotation_callback<LookAtCallback<TSpectral>>(
        this, target, up, is_blender, observation_mode);
}

/**
 * @brief Orient the instance to look at a fixed point, with an optional up vector.
 *
 * The point is in the instance's parent's frame by default, as a position set by set_position()
 * is, so it moves with the parent: an instance under a frame centered on the Earth looks at the
 * Earth with {0, 0, 0}. Given TargetFrame::ROOT, it is in the scene's root frame (the solar
 * system barycenter, with the J2000 axes).
 *
 * The point is aimed at where it is (TRUE_STATE, the default; a point has no light time, so
 * GEOMETRIC_STATE is the same), or where it is seen by the moving instance, shifted by the
 * aberration of its velocity (ABERRATED_STATE), or, given no mode, as the scene view observes.
 *
 * @param target_position The point to look at, in meters
 * @param up Up vector for orientation (default: {0, 0, 1})
 * @param frame The frame the point is in (default: the parent's)
 * @param observation_mode The observation mode to aim in (default: TRUE_STATE), or none for the
 *        scene view's
 * @throws std::runtime_error if the target or up is not finite.
 */
template <IsSpectral TSpectral>
void Instance<TSpectral>::look_at(const Vec3<double>& target_position,
                                  const Vec3<double>& up,
                                  TargetFrame frame,
                                  std::optional<ObservationMode> observation_mode)
{
    validate_real(target_position, this->get_info() + " target position");
    validate_real(up, this->get_info() + " up vector");
    bool is_blender = false;
    if (std::holds_alternative<CameraModel<TSpectral>*>(asset_)) {
        is_blender = std::get<CameraModel<TSpectral>*>(asset_)->uses_blender_convention();
    }

    this->template set_custom_rotation_callback<LookAtPositionCallback<TSpectral>>(
        this, target_position, up, is_blender, frame, observation_mode);
}

} // namespace huira
