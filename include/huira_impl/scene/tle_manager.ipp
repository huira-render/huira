#include <memory>
#include <string>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/scene/constellation.hpp"
#include "huira/scene/node.hpp"
#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief What holds a claim, if it is still held, or empty if it has lapsed.
 *
 * The entry is held by its holder: by a node's position source, so it is gone once the node is
 * destroyed or given another position callback, or by a constellation, so it is gone once the
 * satellite is removed from it. A position set by hand, or by SPICE, leaves the old callback in
 * place, unused, so a node's position mode is checked too. A node or a constellation deleted from
 * the scene can outlive its deletion while something else holds it, so it is told apart by the
 * scene no longer owning it.
 *
 * @param claim The claim.
 * @return The holder's description (see SceneObject::get_info()), or empty.
 */
template <IsSpectral TSpectral>
std::string TleManager<TSpectral>::holder_of_(const Claim& claim)
{
    if (claim.entry.expired()) {
        return {};
    }
    if (const std::shared_ptr<const Node<TSpectral>> node = claim.node.lock()) {
        const bool held =
            node->is_scene_owned() && node->get_position_mode() == TransformMode::POSITION_CALLBACK;
        return held ? node->get_info() : std::string();
    }
    if (const std::shared_ptr<const Constellation<TSpectral>> constellation =
            claim.constellation.lock()) {
        return constellation->is_scene_owned() ? constellation->get_info() : std::string();
    }
    return {};
}

/**
 * @brief Claim a satellite for a node, with the element set to place it by.
 *
 * If the node already holds the satellite, the element set it is placed by is replaced, as the
 * scene holds one for each satellite. If nothing holds it, it is the node's, with this element set.
 *
 * @param satrec The satellite's element set.
 * @param node The node it is to place.
 * @return The element set the scene now holds for the satellite, for the node's position source to
 *         hold: the claim lasts as long as it does.
 * @throws std::runtime_error if another node holds the satellite (one still in the scene, and
 *         still placed by it), or a constellation does.
 */
template <IsSpectral TSpectral>
std::shared_ptr<TleEntry> TleManager<TSpectral>::claim(const SatRec& satrec,
                                                       const Node<TSpectral>& node)
{
    Claim& claim = claims_[satrec.catalog_number()];
    const std::string holder = holder_of_(claim);
    const std::string satellite = "satellite " + std::to_string(satrec.catalog_number());

    if (!holder.empty() && !claim.node.expired() && claim.node.lock().get() != &node) {
        HUIRA_THROW_ERROR(node.get_info() + " - cannot be placed by " + satellite +
                          "'s TLE: " + holder +
                          " is. A satellite places one node: to place more with it, make them "
                          "children of that node");
    }
    if (!holder.empty() && !claim.constellation.expired()) {
        HUIRA_THROW_ERROR(node.get_info() + " - cannot be placed by " + satellite +
                          "'s TLE: it is in " + holder +
                          ". A satellite is placed once: remove it from the constellation first");
    }

    std::shared_ptr<TleEntry> entry = claim.entry.lock();
    if (!holder.empty()) {
        HUIRA_LOG_INFO(
            node.get_info() + " - " + satellite + "'s element set replaced, its epoch from " +
            entry->satrec.epoch().to_utc_string() + " to " + satrec.epoch().to_utc_string());
        entry->satrec = satrec;
        return entry;
    }

    entry = std::make_shared<TleEntry>(satrec);
    claim = {entry, node.weak_from_this(), {}};
    return entry;
}

/**
 * @brief Claim a satellite for a constellation, with the element set to place it by.
 *
 * The constellation replaces the element set of a satellite it already holds itself: it asks only
 * for satellites new to it.
 *
 * @param satrec The satellite's element set.
 * @param constellation The constellation it is to join.
 * @param holder Set to what holds the satellite, if it can't be claimed.
 * @return The element set the scene now holds for the satellite, for the constellation to hold: the
 *         claim lasts as long as it does. Or nothing, if a node or another constellation holds it.
 */
template <IsSpectral TSpectral>
std::shared_ptr<TleEntry> TleManager<TSpectral>::claim(
    const SatRec& satrec, const Constellation<TSpectral>& constellation, std::string& holder)
{
    Claim& claim = claims_[satrec.catalog_number()];
    holder = holder_of_(claim);
    if (!holder.empty()) {
        return nullptr;
    }

    auto entry = std::make_shared<TleEntry>(satrec);
    claim = {entry, {}, constellation.weak_from_this()};
    return entry;
}

} // namespace huira
