#include <memory>
#include <vector>
#include <cmath>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/util/logger.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
    static inline float asteroid_phi1(float alpha)
    {
        float A = 3.33f;
        float B = 0.64f;
        return std::exp(-A * std::pow(std::tan(alpha / 2), B));
    };

    static inline float asteroid_phi2(float alpha)
    {
        float A = 1.87f;
        float B = 1.22f;
        return std::exp(-A * std::pow(std::tan(alpha / 2), B));
    };

    template <IsSpectral TSpectral>
    UnresolvedAsteroid<TSpectral>::UnresolvedAsteroid(float H, float G, InstanceHandle<TSpectral> light_instance, TSpectral albedo) :
            H_ { H }, G_{ G }, 
            light_instance_{ light_instance.get() }, 
            albedo_{ albedo }
        {
            const Instantiable<TSpectral>& asset = light_instance_->asset();
            auto* light_ptr = std::get_if<Light<TSpectral>*>(&asset);
            if (!light_ptr) {
                HUIRA_THROW_ERROR("UnresolvedLambertianSphere requires an Instance containing a Light");
            }
            light_ = *light_ptr;
        }

    template <IsSpectral TSpectral>
    void UnresolvedAsteroid<TSpectral>::resolve_irradiance(
            const Transform<float>& self_transform,
            const std::vector<LightInstance<TSpectral>>& lights
        ) {
            for (const auto& light_inst : lights) {
                if (light_inst.light.get() == light_) {
                    Vec3<float> to_light = light_inst.transform.position - self_transform.position;
                    
                    // TODO Compute
                    
                    return;
                }
            }
            HUIRA_THROW_ERROR("UnresolvedAsteroid could not find its light source in SceneView");
        }
}
