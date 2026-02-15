#include <cmath>
#include <memory>
#include <vector>

#include "glm/glm.hpp"

#include "huira/assets/lights/light.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/scene_view_types.hpp"
#include "huira/util/logger.hpp"

namespace huira {
    /**
     * @brief Computes the Lambert phase function for a spherical body.
     * 
     * Calculates the geometric phase function for a sphere with uniform Lambertian
     * scattering. The function accounts for the visible illuminated area and the
     * scattering properties at different phase angles.
     * 
     * The formula is: \f$\frac{\sin(\alpha) + (\pi - \alpha)\cos(\alpha)}{\pi}\f$,
     * where \f$\alpha\f$ is the phase angle.
     * 
     * @param phase The phase angle in radians.
     * @return The Lambert phase function value.
     */
    static inline float lambert_phase_function(float phase)
    {
        return (std::sin(phase) + (PI<float>() - phase) * std::cos(phase)) / PI<float>();
    };

    /**
     * @brief Constructs an UnresolvedLambertianSphere with specified properties.
     * 
     * Initializes a Lambertian sphere with a given radius, illuminating light source,
     * and spectral albedo. The constructor validates that the provided light instance
     * actually contains a Light object.
     * 
     * @param radius Physical radius of the sphere in meters.
     * @param light_instance Handle to the Instance containing the illuminating light source.
     * @param albedo Spectral albedo of the sphere (default: 1.0 for all wavelengths).
     * @throws std::runtime_error if the light_instance does not contain a Light.
     */
    template <IsSpectral TSpectral>
    UnresolvedLambertianSphere<TSpectral>::UnresolvedLambertianSphere(units::Meter radius, InstanceHandle<TSpectral> light_instance, TSpectral albedo) :
        radius_ { static_cast<float>(radius.to_si()) }, 
        light_instance_{ light_instance.get() }, 
        albedo_{ albedo }
    {
        const Instantiable<TSpectral>& asset = light_instance_->asset();
        auto* light_ptr = std::get_if<Light<TSpectral>*>(&asset);
        if (!light_ptr) {
            HUIRA_THROW_ERROR("UnresolvedLambertianSphere::UnresolvedLambertianSphere - Requires an Instance containing a Light");
        }
        light_ = *light_ptr;

        if (radius_ <= 0.f || std::isnan(radius_) || std::isinf(radius_)) {
            HUIRA_THROW_ERROR("UnresolvedLambertianSphere::UnresolvedLambertianSphere - Radius must be a positive finite value");
        }

        if (!albedo.valid_albedo()) {
            HUIRA_THROW_ERROR("UnresolvedLambertianSphere::UnresolvedLambertianSphere - Invalid spectral albedo: " + 
                albedo.to_string());
        }
    }

    /**
     * @brief Resolves the spectral irradiance based on Lambertian sphere scattering.
     * 
     * Computes the apparent brightness of the sphere as seen by an observer at the
     * origin. The calculation accounts for:
     * - The incident irradiance from the light source
     * - The sphere's cross-sectional area and albedo
     * - The Lambert phase function based on the phase angle
     * - Inverse square law falloff to the observer
     * 
     * @param self_transform The world-space transform of the sphere.
     * @param lights A vector of all light instances in the scene.
     * @throws std::runtime_error if the sphere's light source is not found in the scene.
     */
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

                if (!reflected_irradiance.valid()) {
                    HUIRA_THROW_ERROR("UnresolvedLambertianSphere::resolve_irradiance - Computed invalid reflected irradiance: " +
                        reflected_irradiance.to_string());
                }

                this->irradiance_ = reflected_irradiance;
                
                return;
            }
        }
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere::resolve_irradiance - Could not find its light source in SceneView");
    }
}
