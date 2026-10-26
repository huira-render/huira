#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "embree4/rtcore.h"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/render/shading_utils.hpp"
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

    /// The light's instance in the scene view's Embree scene, which holds it, if rays can hit the
    /// light (nullptr if not): Embree gives the transform it draws the light with at a time.
    RTCGeometry geometry = nullptr;

    /**
     * @brief The light's transform at a normalized time in the exposure, where rays find it.
     *
     * A moving light that rays can hit is drawn between its temporal samples with Embree's own
     * interpolation of its poses, which instance_transform_at() only approximates, turning it a
     * little differently: it is posed as Embree draws it, so that whatever its shape, it is
     * sampled where rays find it. Any other light is posed by instance_transform_at().
     *
     * @param time Normalized time in [0, 1] across the exposure.
     * @return The light's transform at that time.
     */
    [[nodiscard]] Transform<float> transform_at(float time) const
    {
        if (geometry == nullptr || transforms.size() < 2) {
            return instance_transform_at(transforms, time);
        }
        const EmbreeTransform drawn = embree_transform_at(geometry, std::clamp(time, 0.f, 1.f));
        Transform<float> transform;
        transform.position = drawn.translation;
        // Its rotation, scale and shear, as Embree interpolates them, with the scale's signs as
        // they are at the samples:
        detail::factor_linear(drawn.linear, detail::signs_of(transforms.front().scale), transform);
        return transform;
    }
};

/**
 * @brief Instance of an unresolved object in a scene view.
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
struct UnresolvedInstance {
    std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object;
    std::uint64_t instance_id = NO_OBJECT_ID; // The instance in the scene graph that placed it
    std::vector<Transform<float>> transforms; // Transform at N times
    std::vector<Time> times;                  // The N times, evenly spaced through the exposure
    std::vector<TSpectral> irradiances;       // Resolved: one for each time, or one for all
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
