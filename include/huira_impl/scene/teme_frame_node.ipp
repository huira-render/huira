#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/spice.hpp"

namespace huira {

/**
 * @brief Construct a scene's TEME frame, placed by SPICE at EARTH with the axes of
 * HUIRA_EARTH_TEME.
 *
 * Its pose is set here, directly: its setters refuse, as it is fixed. Scene::teme_frame() makes
 * it, attaches it to the root, and loads the frame kernel.
 *
 * @param scene Pointer to the owning Scene
 */
template <IsSpectral TSpectral>
TemeFrameNode<TSpectral>::TemeFrameNode(Scene<TSpectral>* scene) : FrameNode<TSpectral>(scene)
{
    this->spice_origin_ = SPICE_ORIGIN;
    this->spice_frame_ = spice::TEME_FRAME_NAME;
    this->position_mode_ = TransformMode::SPICE_TRANSFORM;
    this->rotation_mode_ = TransformMode::SPICE_TRANSFORM;
}

} // namespace huira
