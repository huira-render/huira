#pragma once

#include <cstdint>
#include <limits>

namespace huira {
/// The ID recorded where there is no object: a ray that hit nothing, or a hit on a light, or a
/// primitive placed directly rather than by a model (for ObjectIds::model).
inline constexpr std::uint64_t NO_OBJECT_ID = std::numeric_limits<std::uint64_t>::max();

/**
 * @brief The IDs of the scene objects a ray hit.
 *
 * Each is the id() of a scene object, as print_graph() and print_contents() show them. A
 * model's internal graph is shared by every instance of the model, so its nodes alone cannot
 * tell two placements of one model apart; the instance does, and the part tells the pieces of
 * one placement apart.
 *
 * For a model loaded from a file with two copies of a solar panel, placed twice in the scene,
 * the four panels have the same geometry and model, two instances (one per placement) and two
 * parts (one per copy within the model). A primitive placed directly by an instance has no
 * model, and its part is that instance.
 */
struct ObjectIds {
    /// The geometry hit: the mesh or ellipsoid, shared by every primitive made from it.
    std::uint64_t geometry = NO_OBJECT_ID;

    /// The model placed by the instance, or NO_OBJECT_ID for a primitive placed directly.
    std::uint64_t model = NO_OBJECT_ID;

    /// The instance in the scene graph that placed the primitive or its model.
    std::uint64_t instance = NO_OBJECT_ID;

    /// The innermost instance that placed the primitive: the model's own node for a primitive
    /// in a model, or the instance itself for a primitive placed directly.
    std::uint64_t part = NO_OBJECT_ID;

    bool operator==(const ObjectIds&) const = default;
};
} // namespace huira
