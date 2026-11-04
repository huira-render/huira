#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/ephemeris/satrec.hpp"
#include "huira/scene/state_callbacks/tle_position.hpp"

namespace huira {
// Forward declare:
template <IsSpectral TSpectral>
class Node;

template <IsSpectral TSpectral>
class Constellation;

/**
 * @brief A scene's record of the satellites it places: for each NORAD catalog number, the element
 * set the scene holds for it, and what it places, a node or a satellite of a constellation.
 *
 * A satellite is placed once at most. Node::set_tle_position() claims a satellite for its node,
 * and throws if a node or a constellation holds it; Constellation::add() claims satellites for the
 * constellation, and leaves out those something else holds. A claim lapses on its own when its
 * holder lets the satellite go: a node deleted or given another position source, a satellite
 * removed from its constellation, or a constellation deleted. The manager holds the element set and
 * the holder only weakly, as the holder holds the element set, and checks whether a claim is still
 * held only when the same satellite is claimed again. Nothing in the scene has to tell it when a
 * satellite is let go.
 *
 * It is internal to Scene.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class TleManager {
  public:
    std::shared_ptr<TleEntry> claim(const SatRec& satrec, const Node<TSpectral>& node);

    std::shared_ptr<TleEntry>
    claim(const SatRec& satrec, const Constellation<TSpectral>& constellation, std::string& holder);

  private:
    struct Claim {
        std::weak_ptr<TleEntry> entry;

        /// What it places: a node, or a satellite of a constellation (the other is empty).
        std::weak_ptr<const Node<TSpectral>> node;
        std::weak_ptr<const Constellation<TSpectral>> constellation;
    };

    std::unordered_map<int, Claim> claims_;

    static std::string holder_of_(const Claim& claim);
};

} // namespace huira

#include "huira_impl/scene/tle_manager.ipp"
