#pragma once

#include <memory>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/scene/object_ids.hpp"

namespace huira {
// Forward declarations
template <IsSpectral TSpectral>
class Light;

template <IsSpectral TSpectral>
class Primitive;

template <IsSpectral TSpectral>
class UnresolvedObject;

/**
 * @brief Instance of a light in a scene view.
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
struct LightInstance {
    std::shared_ptr<Light<TSpectral>> light;
    std::vector<Transform<float>> transforms; // Transform at N times
};

/**
 * @brief Instance of an unresolved object in a scene view.
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
struct UnresolvedInstance {
    std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object;
    std::vector<Transform<float>> transforms; // Transform at N times
    std::vector<Time> times;                  // The N times, evenly spaced through the exposure
};

/**
 * @brief Batch of mesh instances in a scene view.
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
struct PrimitiveBatch {
    std::shared_ptr<Primitive<TSpectral>> primitive;
    std::vector<std::vector<Transform<float>>> instances; // Instances Transforms at N times
    std::vector<ObjectIds> ids;                           // What placed each instance
};
} // namespace huira
