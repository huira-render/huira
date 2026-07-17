#pragma once

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
 * @brief Represents an unresolved asteroid with irradiance computed using the H-G magnitude system.
 *
 * UnresolvedAsteroid models small solar system bodies (asteroids) whose apparent
 * brightness is computed using the H-G photometric system, which accounts for the
 * phase angle between the Sun, asteroid, and observer. The absolute magnitude (H)
 * and slope parameter (G) characterize the asteroid's intrinsic brightness and
 * phase function.
 *
 * The H-G system is defined against solar illumination, so a light instance
 * (the sun) is required; other lights in the scene do not contribute.
 *
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
class UnresolvedAsteroid : public UnresolvedObject<TSpectral> {
  public:
    UnresolvedAsteroid(double H,
                       double G,
                       InstanceHandle<TSpectral> light_instance,
                       TSpectral albedo = TSpectral{1.f});
    UnresolvedAsteroid(double H, double G, InstanceHandle<TSpectral> light_instance, float albedo);

    void resolve_irradiance(const std::vector<Transform<float>>& self_transforms,
                            const std::vector<Time>& times,
                            const SceneView<TSpectral>& scene_view,
                            RandomSampler<float>& sampler) override;

    std::string type() const override { return "UnresolvedAsteroid"; }

  private:
    double H_;
    double G_;
    std::shared_ptr<Instance<TSpectral>> light_instance_;
    Light<TSpectral>* light_;
    TSpectral albedo_;
};
} // namespace huira

#include "huira_impl/assets/unresolved/unresolved_asteroid.ipp"
