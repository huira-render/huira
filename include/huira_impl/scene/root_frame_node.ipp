#include "huira/concepts/spectral_concepts.hpp"

namespace huira {

/**
 * @brief Construct a scene's root frame, placed by SPICE at the solar system barycenter with the
 * J2000 axes.
 *
 * Its pose is set here, directly: its setters refuse, as it is fixed.
 *
 * @param scene Pointer to the owning Scene
 */
template <IsSpectral TSpectral>
RootFrameNode<TSpectral>::RootFrameNode(Scene<TSpectral>* scene) : FrameNode<TSpectral>(scene)
{
    this->spice_origin_ = SPICE_ORIGIN;
    this->spice_frame_ = SPICE_FRAME;
    this->position_mode_ = TransformMode::SPICE_TRANSFORM;
    this->rotation_mode_ = TransformMode::SPICE_TRANSFORM;
}

} // namespace huira
