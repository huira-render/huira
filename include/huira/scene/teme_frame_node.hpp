#pragma once

#include <string>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/scene/frame_node.hpp"

namespace huira {
// Forward declare:
template <IsSpectral TSpectral>
class Scene;

/**
 * @brief A scene's TEME frame: centered on the Earth, with the axes of the True Equator, Mean
 * Equinox of date frame, in which SGP4 gives the states of objects from their NORAD element sets.
 *
 * SPICE places it, at EARTH, with the axes of HUIRA_EARTH_TEME, which Huira's frame kernel
 * (kernels/fk/huira_teme.tf in the data directory) defines from the IAU 1976 precession and
 * IAU 1980 nutation models. Those reproduce Vallado's teme2eci to within 0.2 mas. Rendering
 * anything in it needs an SPK with the Earth's ephemeris, such as de440s.bsp, as for any node
 * that SPICE places.
 *
 * Its pose is fixed: it cannot be moved, rotated or scaled, nor its SPICE origin or frame
 * changed. What it holds can be placed freely. Scene::teme_frame() makes it, keeping one per
 * scene, under the root.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class TemeFrameNode final : public FrameNode<TSpectral> {
  public:
    /// Its SPICE origin.
    static constexpr const char* SPICE_ORIGIN = "EARTH";

    TemeFrameNode(Scene<TSpectral>* scene);
    ~TemeFrameNode() override = default;

    std::string type() const override { return "TemeFrameNode"; }

  protected:
    bool pose_is_fixed_() const override { return true; }
};
} // namespace huira

#include "huira_impl/scene/teme_frame_node.ipp"
