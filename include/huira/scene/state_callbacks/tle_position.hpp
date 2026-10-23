#pragma once

#include <iomanip>
#include <ios>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "huira/core/time.hpp"
#include "huira/ephemeris/satrec.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"
#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief The element set a scene holds for one satellite.
 *
 * It is shared by the scene's TLE manager and the position source of the node the satellite
 * places, which reads it each time it is evaluated: an element set the manager replaces it with
 * reaches the node.
 */
struct TleEntry {
    explicit TleEntry(SatRec element_set) : satrec(std::move(element_set)) {}

    SatRec satrec;
};

/**
 * @brief Places a node by SGP4, from the element set its scene holds for a satellite (see
 * Node::set_tle_position()), in its parent's frame, the scene's TEME frame.
 */
struct TlePositionCallback : public PositionCallback {
    TlePositionCallback(std::shared_ptr<const TleEntry> entry, std::string node_info)
        : entry_(std::move(entry)), node_info_(std::move(node_info))
    {
    }

    /// The satellite's state at the context's time, in meters and meters per second.
    /// @throws std::runtime_error if SGP4 can't place it then: most often, it has decayed.
    [[nodiscard]] PositionState evaluate(const PoseContext& context,
                                         const RotationState& /*rotation*/) const override
    {
        const SatRec& satrec = entry_->satrec;
        const std::optional<PositionState> state = satrec.evaluate(context.time);
        if (!state.has_value()) {
            std::ostringstream days;
            days << std::fixed << std::setprecision(1)
                 << (context.time.et() - satrec.epoch().et()) / 86400.0;
            HUIRA_THROW_ERROR(
                node_info_ + " - SGP4 can't place satellite " +
                std::to_string(satrec.catalog_number()) + " at " + context.time.to_utc_string() +
                ", " + days.str() +
                " days from its element set's epoch: " + satrec.error_message_at(context.time));
        }
        return *state;
    }

    std::shared_ptr<const TleEntry> entry_;
    std::string node_info_;
};

} // namespace huira
