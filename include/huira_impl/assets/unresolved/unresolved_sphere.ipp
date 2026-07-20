#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "glm/glm.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/geometry/ray.hpp"
#include "huira/render/interaction.hpp"
#include "huira/scene/scene_view.hpp"
#include "huira/scene/scene_view_types.hpp"
#include "huira/util/logger.hpp"
#include "huira/volumes/medium_stack.hpp"

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

/**
 * @brief Constructs an UnresolvedLambertianSphere illuminated by all scene lights.
 *
 * @param radius Physical radius of the sphere in meters.
 * @param albedo Spectral albedo of the sphere (default: 1.0 for all wavelengths).
 * @throws std::runtime_error if the radius or albedo is invalid.
 */
template <IsSpectral TSpectral>
UnresolvedLambertianSphere<TSpectral>::UnresolvedLambertianSphere(units::Meter radius,
                                                                  TSpectral albedo)
    : radius_{static_cast<float>(radius.to_si())}, albedo_{albedo}
{
    HUIRA_LOG_INFO("UnresolvedLambertianSphere::UnresolvedLambertianSphere(radius, albedo)");
    validate_shape_and_albedo_();
}

/**
 * @brief Constructs an UnresolvedLambertianSphere illuminated by all scene lights.
 *
 * @param radius Physical radius of the sphere in meters.
 * @param albedo Constant albedo of the sphere.
 * @throws std::runtime_error if the radius or albedo is invalid.
 */
template <IsSpectral TSpectral>
UnresolvedLambertianSphere<TSpectral>::UnresolvedLambertianSphere(units::Meter radius, float albedo)
    : UnresolvedLambertianSphere{radius, TSpectral{albedo}}
{
}

/**
 * @brief Constructs a light-linked UnresolvedLambertianSphere.
 *
 * Initializes a Lambertian sphere with a given radius, illuminating light source,
 * and spectral albedo. The constructor validates that the provided light instance
 * actually contains a Light object. The sphere is illuminated only by that light.
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
        "UnresolvedLambertianSphere::UnresolvedLambertianSphere(radius, light_instance, albedo)");
    const Instantiable<TSpectral>& asset = light_instance_->asset();
    auto* light_ptr = std::get_if<Light<TSpectral>*>(&asset);
    if (!light_ptr) {
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere::UnresolvedLambertianSphere - Requires an "
                          "Instance containing a Light");
    }
    light_ = *light_ptr;

    validate_shape_and_albedo_();
}

/**
 * @brief Constructs a light-linked UnresolvedLambertianSphere.
 *
 * Initializes a Lambertian sphere with a given radius, illuminating light source,
 * and constant albedo. The constructor validates that the provided light instance
 * actually contains a Light object. The sphere is illuminated only by that light.
 *
 * @param radius Physical radius of the sphere in meters.
 * @param light_instance Handle to the Instance containing the illuminating light source.
 * @param albedo Constant spectral albedo of the sphere.
 * @throws std::runtime_error if the light_instance does not contain a Light.
 */
template <IsSpectral TSpectral>
UnresolvedLambertianSphere<TSpectral>::UnresolvedLambertianSphere(
    units::Meter radius, InstanceHandle<TSpectral> light_instance, float albedo)
    : UnresolvedLambertianSphere{radius, light_instance, TSpectral{albedo}}
{
}

/**
 * @brief Resolves the spectral irradiance via stochastic illumination sampling.
 *
 * Computes the apparent brightness of the sphere as seen by an observer at the
 * origin, at each temporal sample of the exposure. The estimator is, for each
 * illumination source (emitting light or designated indirect source):
 *
 *   F = (2/3) * albedo * (R / d_obs)^2 * E[ (L_i * T_i / pdf_i) * Phi(alpha_i) ]
 *
 * where each sample i draws a direction toward the source, L_i is the incident
 * radiance from that direction (the light's radiance, or the traced-and-shaded
 * radiance of an indirect source's surface), T_i is the visibility /
 * transmittance along the sample ray, pdf_i is the solid-angle sampling
 * density, and alpha_i is that sample's own phase angle against the observer
 * direction. Phi is the Lambert sphere phase function (Phi(0) = 1); the 2/3
 * prefactor is the Lambertian sphere's geometric albedo factor, which
 * integrates to exact energy conservation (total scattered power =
 * albedo * pi * R^2 * E).
 *
 * This single formulation captures: multiple light sources, penumbra (finite
 * lights partially occluded across their disk), extended non-uniform indirect
 * sources such as earthshine/moonshine (phase variation across the source's
 * solid angle included), and occlusion of the indirect sources themselves.
 *
 * If this sphere was constructed with a light-linking filter, only that light
 * contributes to the direct term; all designated indirect sources always
 * contribute.
 *
 * @param self_transforms Camera-relative transforms of this object, one per temporal sample.
 * @param times Absolute times of the temporal samples.
 * @param scene_view The fully constructed scene view (TLAS built).
 * @param sampler Random sampler for the stochastic estimate.
 * @throws std::runtime_error if a light-linked sphere's light source is not found in the scene.
 */
template <IsSpectral TSpectral>
void UnresolvedLambertianSphere<TSpectral>::resolve_irradiance(
    const std::vector<Transform<float>>& self_transforms,
    const std::vector<Time>& times,
    const SceneView<TSpectral>& scene_view,
    RandomSampler<float>& sampler)
{
    const std::vector<LightInstance<TSpectral>>& lights = scene_view.lights();
    const std::vector<IndirectSourceInstance<TSpectral>>& indirect_sources =
        scene_view.indirect_sources();

    bool filter_light_found = (light_ == nullptr);
    const std::size_t num_samples = self_transforms.size();
    std::vector<TSpectral> irradiances(num_samples, TSpectral{0});

    for (std::size_t i = 0; i < num_samples; ++i) {
        const Vec3<float> self_position = self_transforms[i].position;

        const float observer_distance = glm::length(self_position);
        const Vec3<float> V = -self_position / observer_distance; // Toward the observer

        // Normalized exposure time for motion-blurred ray queries:
        const float trace_time =
            (num_samples > 1) ? static_cast<float>(i) / static_cast<float>(num_samples - 1) : 0.5f;

        // Minimal shading reference for light sampling (only position is used):
        Interaction<TSpectral> reference{};
        reference.position = self_position;

        // Estimate of the directional gather integral: Int L(w) * Phi(alpha(w)) dw
        TSpectral gathered{0};

        // --- Emitting lights (with shadowing / penumbra) ---
        for (const auto& light_inst : lights) {
            if (light_ != nullptr && light_inst.light.get() != light_) {
                continue;
            }
            filter_light_found = true;

            // Use the light transform matching this temporal sample:
            const std::size_t li = std::min(i, light_inst.transforms.size() - 1);
            const Transform<float>& light_transform = light_inst.transforms[li];

            TSpectral light_sum{0};
            for (std::size_t s = 0; s < light_samples_; ++s) {
                auto sample = light_inst.light->sample_li(reference, light_transform, sampler);
                if (!sample) {
                    continue; // e.g. inside the light sphere: no contribution
                }

                // Visibility / transmittance along the sample ray. The sphere
                // itself is not in the TLAS, so no self-intersection offset is
                // needed. The object is assumed to be in vacuum (empty medium
                // stack at the ray origin).
                Ray<TSpectral> shadow_ray(self_position, sample->wi);
                MediumStack<TSpectral> medium_stack;
                TSpectral transmittance = scene_view.evaluate_transmittance(
                    shadow_ray, sample->distance, medium_stack, sampler, trace_time);
                if (transmittance.max() <= 0.0f) {
                    continue;
                }

                const float cos_alpha = std::clamp(glm::dot(sample->wi, V), -1.0f, 1.0f);
                const float phase = std::acos(cos_alpha);

                light_sum +=
                    (sample->Li / sample->pdf) * transmittance * lambert_phase_function(phase);
            }
            gathered += light_sum / static_cast<float>(light_samples_);
        }

        // --- Designated indirect sources (earthshine, moonshine, ...) ---
        for (std::size_t src_idx = 0; src_idx < indirect_sources.size(); ++src_idx) {
            const auto& source = indirect_sources[src_idx];
            const std::size_t ti = std::min(i, source.transforms.size() - 1);

            TSpectral source_sum{0};
            for (std::size_t s = 0; s < indirect_source_samples_; ++s) {
                SphereConeSample sample = source.sample_toward(self_position, ti, sampler);

                Ray<TSpectral> ray(self_position, sample.wi);
                HitRecord hit = scene_view.intersect(ray, trace_time);

                // Contributions are counted only when the ray lands on the
                // intended source: misses are known zeros of the bounding-cone
                // estimator, and hits on anything else are occlusions (other
                // sources are accounted for by their own sampling rounds).
                if (scene_view.indirect_source_index(hit) != src_idx) {
                    continue;
                }

                TSpectral radiance = scene_view.direct_lit_radiance(ray, hit, sampler, trace_time);
                if (radiance.max() <= 0.0f) {
                    continue;
                }

                const float cos_alpha = std::clamp(glm::dot(sample.wi, V), -1.0f, 1.0f);
                const float phase = std::acos(cos_alpha);

                source_sum += (radiance / sample.pdf) * lambert_phase_function(phase);
            }
            gathered += source_sum / static_cast<float>(indirect_source_samples_);
        }

        // Lambertian sphere response: F = (2/3) * a * (R/d)^2 * Int L Phi dw
        TSpectral reflected_irradiance = (2.0f / 3.0f) * albedo_ * (radius_ * radius_) /
                                         (observer_distance * observer_distance) * gathered;

        if (!reflected_irradiance.valid()) {
            HUIRA_THROW_ERROR("UnresolvedLambertianSphere::resolve_irradiance - Computed "
                              "invalid reflected irradiance: " +
                              reflected_irradiance.to_string());
        }

        irradiances[i] = reflected_irradiance;
    }

    if (!filter_light_found) {
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere::resolve_irradiance - Could not find its "
                          "light source in SceneView");
    }

    this->set_resolved_irradiance_(std::move(irradiances), times);
}

/**
 * @brief Sets the number of stochastic samples per light source.
 *
 * Each light is sampled this many times per temporal sample; each sample
 * casts one shadow/transmittance ray, so occlusion (including penumbra
 * across a finite light's disk) is captured. Defaults to 64.
 *
 * @param samples Sample count (must be at least 1).
 * @throws std::runtime_error if samples is zero.
 */
template <IsSpectral TSpectral>
void UnresolvedLambertianSphere<TSpectral>::set_light_samples(std::size_t samples)
{
    if (samples == 0) {
        HUIRA_THROW_ERROR("UnresolvedLambertianSphere::set_light_samples - Must be at least 1");
    }
    light_samples_ = samples;
}

/**
 * @brief Sets the number of stochastic samples per indirect source.
 *
 * Each designated indirect source (reflector) is sampled this many times
 * per temporal sample; each sample traces one ray to the source and shades
 * the hit against the scene's lights. Defaults to 64.
 *
 * @param samples Sample count (must be at least 1).
 * @throws std::runtime_error if samples is zero.
 */
template <IsSpectral TSpectral>
void UnresolvedLambertianSphere<TSpectral>::set_indirect_source_samples(std::size_t samples)
{
    if (samples == 0) {
        HUIRA_THROW_ERROR(
            "UnresolvedLambertianSphere::set_indirect_source_samples - Must be at least 1");
    }
    indirect_source_samples_ = samples;
}

} // namespace huira
