#include <cmath>
#include <memory>
#include <vector>

#include "glm/glm.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/concepts/spectral_concepts.hpp"
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
    return std::max(0.f, (std::sin(phase) + (PI<float>() - phase) * std::cos(phase)) / PI<float>());
}

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
UnresolvedLambertianSphere<TSpectral>::UnresolvedLambertianSphere(
    units::Meter radius, InstanceHandle<TSpectral> light_instance, TSpectral albedo)
    : radius_{static_cast<float>(radius.to_si())}, light_instance_{light_instance.get()},
      albedo_{albedo}
{
    HUIRA_LOG_INFO(
        "UnresolvedLambertianSphere::UnresolvedLambertianSphere(radius, light_instance, albedo");
    const Instantiable<TSpectral>& asset = light_instance_->asset();
    auto* light_ptr = std::get_if<Light<TSpectral>*>(&asset);
    if (!light_ptr) {
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere::UnresolvedLambertianSphere - Requires an "
                          "Instance containing a Light");
    }
    light_ = *light_ptr;

    if (radius_ <= 0.f || std::isnan(radius_) || std::isinf(radius_)) {
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere::UnresolvedLambertianSphere - Radius must be "
                          "a positive finite value");
    }

    if (!albedo_.valid_ratio()) {
        HUIRA_THROW_ERROR(
            "UnresolvedLambertianSphere::UnresolvedLambertianSphere - Invalid spectral albedo: " +
            albedo_.to_string());
    }
}

/**
 * @brief Constructs an UnresolvedLambertianSphere with specified properties.
 *
 * Initializes a Lambertian sphere with a given radius, illuminating light source,
 * and spectral albedo. The constructor validates that the provided light instance
 * actually contains a Light object.
 *
 * @param radius Physical radius of the sphere in meters.
 * @param light_instance Handle to the Instance containing the illuminating light source.
 * @param albedo Constant spectral albedo of the sphere
 * @throws std::runtime_error if the light_instance does not contain a Light.
 */
template <IsSpectral TSpectral>
UnresolvedLambertianSphere<TSpectral>::UnresolvedLambertianSphere(
    units::Meter radius, InstanceHandle<TSpectral> light_instance, float albedo)
    : radius_{static_cast<float>(radius.to_si())}, light_instance_{light_instance.get()},
      albedo_{TSpectral{albedo}}
{
    HUIRA_LOG_INFO(
        "UnresolvedLambertianSphere::UnresolvedLambertianSphere(radius, light_instance, albedo");
    const Instantiable<TSpectral>& asset = light_instance_->asset();
    auto* light_ptr = std::get_if<Light<TSpectral>*>(&asset);
    if (!light_ptr) {
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere::UnresolvedLambertianSphere - Requires an "
                          "Instance containing a Light");
    }
    light_ = *light_ptr;

    if (radius_ <= 0.f || std::isnan(radius_) || std::isinf(radius_)) {
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere::UnresolvedLambertianSphere - Radius must be "
                          "a positive finite value");
    }

    if (!albedo_.valid_ratio()) {
        HUIRA_THROW_ERROR(
            "UnresolvedLambertianSphere::UnresolvedLambertianSphere - Invalid albedo: " +
            std::to_string(albedo));
    }
}

/**
 * @brief Resolves the spectral irradiance based on Lambertian sphere scattering.
 *
 * Computes the apparent brightness of the sphere as seen by an observer at the
 * origin, at each temporal sample of the exposure. The calculation accounts for:
 * - The incident irradiance from each illuminating light source
 * - The sphere's cross-sectional area and albedo
 * - The Lambert phase function based on the phase angle
 * - Inverse square law falloff to the observer
 *
 * If this sphere was constructed with a light-linking filter, only that light
 * contributes; otherwise all lights in the scene view contribute.
 *
 * @param self_transforms Camera-relative transforms of this object, one per temporal sample.
 * @param times Absolute times of the temporal samples.
 * @param scene_view The fully constructed scene view.
 * @param sampler Random sampler (reserved for the stochastic shadowing estimator).
 * @throws std::runtime_error if a light-linked sphere's light source is not found in the scene.
 */
template <IsSpectral TSpectral>
void UnresolvedLambertianSphere<TSpectral>::resolve_irradiance(
    const std::vector<Transform<float>>& self_transforms,
    const std::vector<Time>& times,
    const SceneView<TSpectral>& scene_view,
    RandomSampler<float>& sampler)
{
    (void)sampler;

    const std::vector<LightInstance<TSpectral>>& lights = scene_view.lights();

    bool filter_light_found = (light_ == nullptr);
    std::vector<TSpectral> irradiances(self_transforms.size(), TSpectral{0});

    for (std::size_t i = 0; i < self_transforms.size(); ++i) {
        const Transform<float>& self_transform = self_transforms[i];
        TSpectral total_irradiance{0};

        for (const auto& light_inst : lights) {
            if (light_ != nullptr && light_inst.light.get() != light_) {
                continue;
            }
            filter_light_found = true;

            // Use the light transform matching this temporal sample:
            const std::size_t li = std::min(i, light_inst.transforms.size() - 1);
            const Transform<float>& light_transform = light_inst.transforms[li];

            Vec3<float> L = glm::normalize(light_transform.position - self_transform.position);

            float distance = glm::length(self_transform.position);
            Vec3<float> V = -self_transform.position / distance;

            TSpectral incident_irradiance =
                light_inst.light->irradiance_at(self_transform.position, light_transform);

            float phase = std::acos(std::clamp(glm::dot(V, L), -1.0f, 1.0f));
            float A = PI<float>() * radius_ * radius_; // Cross-sectional area
            TSpectral reflected_power =
                albedo_ * A * incident_irradiance * lambert_phase_function(phase);
            
            TSpectral reflected_irradiance = (2.0f / 3.0f) * albedo_ * incident_irradiance *
                                             (radius_ * radius_) / (distance * distance) *
                                             lambert_phase_function(phase);

            if (!reflected_irradiance.valid()) {
                HUIRA_THROW_ERROR("UnresolvedLambertianSphere::resolve_irradiance - Computed "
                                  "invalid reflected irradiance: " +
                                  reflected_irradiance.to_string());
            }

            total_irradiance += reflected_irradiance;
        }

        irradiances[i] = total_irradiance;
    }

    if (!filter_light_found) {
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere::resolve_irradiance - Could not find its "
                          "light source in SceneView");
    }

    this->set_resolved_irradiance_(std::move(irradiances), times);
}

/**
 * @brief Validates the sphere's radius and albedo.
 *
 * @throws std::runtime_error if the radius is not a positive finite value or the
 *         albedo is not a valid ratio.
 */
template <IsSpectral TSpectral>
void UnresolvedLambertianSphere<TSpectral>::validate_shape_and_albedo_() const
{
    if (radius_ <= 0.f || std::isnan(radius_) || std::isinf(radius_)) {
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere::UnresolvedLambertianSphere - Radius must be "
                          "a positive finite value");
    }

    if (!albedo_.valid_ratio()) {
        HUIRA_THROW_ERROR(
            "UnresolvedLambertianSphere::UnresolvedLambertianSphere - Invalid spectral albedo: " +
            albedo_.to_string());
    }
}

} // namespace huira
