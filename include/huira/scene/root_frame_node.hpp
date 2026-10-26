#pragma once

#include <string>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/scene/frame_node.hpp"

namespace huira {
// Forward declare:
template <IsSpectral TSpectral>
class Scene;

/**
 * @brief A scene's root frame: the solar system barycenter, with the J2000 axes, which SPICE
 * places every other node relative to.
 *
 * Its pose is fixed: it cannot be moved, rotated or scaled, nor its SPICE origin or frame
 * changed, however it is reached, whether through Scene::root or through a child's
 * get_parent(). What it holds can be placed freely. Scene makes it.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class RootFrameNode final : public FrameNode<TSpectral> {
  public:
    /// Its SPICE origin.
    static constexpr const char* SPICE_ORIGIN = "SOLAR SYSTEM BARYCENTER";

    /// Its SPICE frame.
    static constexpr const char* SPICE_FRAME = "J2000";

    RootFrameNode(Scene<TSpectral>* scene);
    ~RootFrameNode() override = default;

    std::string type() const override { return "RootFrameNode"; }

  protected:
    bool pose_is_fixed_() const override { return true; }
};
} // namespace huira

#include "huira_impl/scene/root_frame_node.ipp"
