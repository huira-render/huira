#pragma once

#include <memory>
#include <vector>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/transform.hpp"

namespace huira {
    // Forward declarations
    template <IsSpectral TSpectral>
    class Light;

    template <IsSpectral TSpectral>
    class Mesh;

    template <IsSpectral TSpectral>
    class UnresolvedObject;

    /**
     * @brief Instance of a light in a scene view.
     * @tparam TSpectral Spectral type
     */
    template <IsSpectral TSpectral>
    struct LightInstance {
        std::shared_ptr<Light<TSpectral>> light;
        Transform<float> transform;
    };

    /**
     * @brief Instance of an unresolved object in a scene view.
     * @tparam TSpectral Spectral type
     */
    template <IsSpectral TSpectral>
    struct UnresolvedInstance {
        std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object;
        Transform<float> transform;
    };

    /**
     * @brief Batch of mesh instances in a scene view.
     * @tparam TSpectral Spectral type
     */
    template <IsSpectral TSpectral>
    struct MeshBatch {
        std::shared_ptr<Mesh<TSpectral>> mesh;
        std::vector<Transform<float>> instances;
    };
}
