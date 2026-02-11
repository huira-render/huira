#include <memory>
#include <vector>
#include <cmath>

#include "glm/glm.hpp"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/util/logger.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
    static inline float lambert_phase_function(float phase)
    {
        return (std::sin(phase) + (PI<float>() - phase) * std::cos(phase)) / PI<float>();
    };

    template <IsSpectral TSpectral>
    UnresolvedLambertianSphere<TSpectral>::UnresolvedLambertianSphere(units::Meter radius, InstanceHandle<TSpectral> light_instance, TSpectral albedo) :
        radius_ { static_cast<float>(radius.get_si_value()) }, 
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
    void UnresolvedLambertianSphere<TSpectral>::resolve_irradiance(
        const Transform<float>& self_transform,
        const std::vector<LightInstance<TSpectral>>& lights
    ) {
        for (const auto& light_inst : lights) {
            if (light_inst.light.get() == light_) {
                Vec3<float> L = glm::normalize(light_inst.transform.position - self_transform.position);
                
                float distance = glm::length(self_transform.position);
                Vec3<float> V = -self_transform.position / distance;

                TSpectral incident_irradiance = light_->irradiance_at(self_transform.position, light_inst.transform);

                float phase = std::acos(glm::dot(V, L));
                float A = PI<float>() * radius_ * radius_; // Cross-sectional area
                TSpectral reflectedPower = albedo_ * A * incident_irradiance * lambert_phase_function(phase);

                TSpectral reflected_irradiance = reflectedPower / (4 * PI<float>() * distance * distance);
                this->set_irradiance(reflected_irradiance);
                
                return;
            }
        }
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere could not find its light source in SceneView");
    }
}
