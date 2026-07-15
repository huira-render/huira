#pragma once

#include <string>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/render/sampler.hpp"
#include "huira/scene/node.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {

template <IsSpectral TSpectral>
class SceneView;

/**
 * @brief Represents an unresolved object to be rendered.
 *
 * UnresolvedObject serves as a base class for objects whose irradiance can be
 * computed or updated based on the state of the scene. Subclasses can override
 * resolve_irradiance() to implement custom irradiance computation logic. This base
 * class assumes that the object's spectral irradiance is constant and does not
 * depend on any observer or light positions.
 *
 * Irradiance is stored per temporal sample of the exposure interval: a single
 * entry represents a constant value, while N entries are linearly interpolated
 * across the resolved time range by get_irradiance().
 *
 * @tparam TSpectral The spectral representation type.
 */
template <IsSpectral TSpectral>
class UnresolvedObject : public SceneObject<UnresolvedObject<TSpectral>> {
  public:
    UnresolvedObject() = default;
    UnresolvedObject(const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance);
    UnresolvedObject(const units::WattsPerMeterSquared& irradiance);

    UnresolvedObject(const UnresolvedObject&) = delete;
    UnresolvedObject& operator=(const UnresolvedObject&) = delete;

    void set_irradiance(const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance);
    void set_irradiance(const units::WattsPerMeterSquared& irradiance);

    virtual TSpectral get_irradiance(Time time) const;

    virtual void resolve_irradiance(const std::vector<Transform<float>>& self_transforms,
                                    const std::vector<Time>& times,
                                    const SceneView<TSpectral>& scene_view,
                                    RandomSampler<float>& sampler);

    virtual std::string type() const override { return "UnresolvedObject"; }

  protected:
    void set_resolved_irradiance_(std::vector<TSpectral> irradiances,
                                  const std::vector<Time>& times);

    /// Spectral irradiance per temporal sample (size 1 == constant).
    std::vector<TSpectral> irradiance_{TSpectral{0}};

    /// Ephemeris-time endpoints of the resolved sample range (used for interpolation).
    double et_start_{0.0};
    double et_end_{0.0};
};
} // namespace huira

#include "huira_impl/assets/unresolved/unresolved_object.ipp"
