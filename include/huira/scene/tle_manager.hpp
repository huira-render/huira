#pragma once

#include <memory>
#include <unordered_map>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/ephemeris/satrec.hpp"
#include "huira/scene/state_callbacks/tle_position.hpp"

namespace huira {
// Forward declare:
template <IsSpectral TSpectral>
class Node;

/**
 * @brief A scene's record of the satellites that place its nodes: for each NORAD catalog number,
 * the element set the scene holds for it, and the node it places.
 *
 * A satellite places one node at most. Node::set_tle_position() claims a satellite for its node,
 * and throws if another node holds it. A claim lapses on its own when its node is deleted or given
 * another position source: the manager holds the element set and the node only weakly, as the
 * node's position source holds the element set, and checks whether a claim is still held only
 * when a node claims the same satellite. Nothing in the scene graph has to tell it when a node
 * lets a satellite go.
 *
 * It is internal to Scene.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class TleManager {
  public:
    std::shared_ptr<TleEntry> claim(const SatRec& satrec, const Node<TSpectral>& node);

  private:
    struct Claim {
        std::weak_ptr<TleEntry> entry;
        std::weak_ptr<const Node<TSpectral>> node;
    };

    std::unordered_map<int, Claim> claims_;
};

} // namespace huira

#include "huira_impl/scene/tle_manager.ipp"
