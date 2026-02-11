#pragma once

#include <memory>
#include <vector>

#include "huira/core/transform.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward declarations
    template <IsSpectral TSpectral>
    class Light;

    template <IsSpectral TSpectral>
    class Mesh;

    template <IsSpectral TSpectral>
    class UnresolvedObject;

    template <IsSpectral TSpectral>
    struct LightInstance {
        std::shared_ptr<Light<TSpectral>> light;
        Transform<float> transform;
    };

    template <IsSpectral TSpectral>
    struct UnresolvedInstance {
        std::shared_ptr<UnresolvedObject<TSpectral>> unresolved_object;
        Transform<float> transform;
    };

    template <IsSpectral TSpectral>
    struct MeshBatch {
        std::shared_ptr<Mesh<TSpectral>> mesh;
        std::vector<Transform<float>> instances;
    };
}