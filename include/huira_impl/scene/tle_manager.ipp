#include <memory>
#include <string>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/scene/node.hpp"
#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Claim a satellite for a node, with the element set to place it by.
 *
 * If the node already holds the satellite, the element set it is placed by is replaced, as the
 * scene holds one for each satellite. If no node holds it, it is the node's, with this element
 * set.
 *
 * @param satrec The satellite's element set.
 * @param node The node it is to place.
 * @return The element set the scene now holds for the satellite, for the node's position source to
 *         hold: the claim lasts as long as it does.
 * @throws std::runtime_error if another node holds the satellite: one still in the scene, and
 *         still placed by it.
 */
template <IsSpectral TSpectral>
std::shared_ptr<TleEntry> TleManager<TSpectral>::claim(const SatRec& satrec,
                                                       const Node<TSpectral>& node)
{
    Claim& claim = claims_[satrec.catalog_number()];
    std::shared_ptr<TleEntry> entry = claim.entry.lock();
    const std::shared_ptr<const Node<TSpectral>> holder = claim.node.lock();

    // The entry is held by the node's position source, so it is gone once the node is destroyed
    // or given another position callback. A position set by hand, or by SPICE, leaves the old
    // callback in place, unused, so the node's position mode is checked too. A node deleted from
    // the scene can outlive its deletion while something else holds it, so it is told apart by
    // the scene no longer owning it.
    const bool held = entry && holder && holder->is_scene_owned() &&
                      holder->get_position_mode() == TransformMode::POSITION_CALLBACK;
    if (held && holder.get() != &node) {
        HUIRA_THROW_ERROR(node.get_info() + " - cannot be placed by satellite " +
                          std::to_string(satrec.catalog_number()) +
                          "'s TLE: " + holder->get_info() +
                          " is. A satellite places one node: to place more with it, make them "
                          "children of that node");
    }

    if (held) {
        HUIRA_LOG_INFO(node.get_info() + " - satellite " + std::to_string(satrec.catalog_number()) +
                       "'s element set replaced, its epoch from " +
                       entry->satrec.epoch().to_utc_string() + " to " +
                       satrec.epoch().to_utc_string());
        entry->satrec = satrec;
        return entry;
    }

    entry = std::make_shared<TleEntry>(satrec);
    claim = {entry, node.weak_from_this()};
    return entry;
}

} // namespace huira
