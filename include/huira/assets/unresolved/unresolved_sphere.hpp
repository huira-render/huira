#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "huira/assets/lights/light.hpp"
#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/handles/scene/instance_handle.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
/**
 * @brief Represents an unresolved sphere with Lambertian reflectance.
 *
 * UnresolvedLambertianSphere models a spherical body with uniform Lambertian scattering.
 * The apparent brightness depends on the phase angle between each light source, the
 * sphere, and the observer, calculated using Lambert's phase function. The reflected
 * light is computed based on the sphere's radius, albedo, and the incident irradiance,
 * summed over the illuminating light sources.
 *
 * By default the sphere is illuminated by every light in the scene view. The
 * light-linked constructors restrict illumination to a single specified light
 * instance (light linking).
 *
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
class UnresolvedLambertianSphere : public UnresolvedObject<TSpectral> {
  public:
    /// Constructs a sphere illuminated by all lights in the scene view.
    UnresolvedLambertianSphere(units::Meter radius, TSpectral albedo = TSpectral{1.f});
    /// Constructs a sphere illuminated by all lights in the scene view.
    UnresolvedLambertianSphere(units::Meter radius, float albedo);

    /// Constructs a sphere illuminated only by the given light instance (light linking).
    UnresolvedLambertianSphere(units::Meter radius,
                               InstanceHandle<TSpectral> light_instance,
                               TSpectral albedo = TSpectral{1.f});
    /// Constructs a sphere illuminated only by the given light instance (light linking).
    UnresolvedLambertianSphere(units::Meter radius,
                               InstanceHandle<TSpectral> light_instance,
                               float albedo);

    void resolve_irradiance(const std::vector<Transform<float>>& self_transforms,
                            const std::vector<Time>& times,
                            const SceneView<TSpectral>& scene_view,
                            RandomSampler<float>& sampler) override;

    void set_light_samples(std::size_t samples);

    void set_indirect_source_samples(std::size_t samples);

    std::string type() const override { return "UnresolvedLambertianSphere"; }

  private:
    /// Validates radius and albedo; throws on invalid values.
    void validate_shape_and_albedo_() const;

    float radius_;

    /// Optional light-linking filter; nullptr means "illuminated by all lights".
    std::shared_ptr<Instance<TSpectral>> light_instance_ = nullptr;
    Light<TSpectral>* light_ = nullptr;

    TSpectral albedo_;

    std::size_t light_samples_ = 64;
    std::size_t indirect_source_samples_ = 64;
};
} // namespace huira

#include "huira_impl/assets/unresolved/unresolved_sphere.ipp"
