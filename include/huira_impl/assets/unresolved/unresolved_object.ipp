#include <vector>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/scene_view_types.hpp"
#include "huira/core/transform.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void UnresolvedObject<TSpectral>::resolve_irradiance(
            const Transform<float>& self_transform,
            const std::vector<LightInstance<TSpectral>>& lights
        ) {
            // Default: do nothing, irradiance stays as initialized
            (void)self_transform;
            (void)lights;
        }
}
