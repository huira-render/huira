#include <vector>

#include "glm/glm.hpp"

#include "huira/core/constants.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void UnresolvedEmitter<TSpectral>::resolve_irradiance(
            const Transform<float>& self_transform,
            const std::vector<LightInstance<TSpectral>>& lights
        ) 
        {
            (void)lights; // Not needed for this implementation

            float distance = glm::length(self_transform.position);

            float distance_sq = distance * distance;
            this->irradiance_ = spectral_power_ / (4.f * PI<float>() * distance_sq);
        }
}
