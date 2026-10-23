#pragma once

#include <string>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/sampling/sampler.hpp"
#include "huira/scene/node.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {

template <IsSpectral TSpectral>
class SceneView;

/**
 * @brief Represents an unresolved object to be rendered.
 *
 * UnresolvedObject serves as a base class for objects whose irradiance at the camera is
 * computed from the state of the scene. A scene view resolves the irradiance of each of its
 * instances, at each of the instance's temporal samples, by resolve_irradiance(), which
 * subclasses override. This base class gives the irradiance it is given, wherever and however
 * often it is placed.
 *
 * The irradiance resolved for an instance belongs to the scene view (see
 * SceneView::unresolved_irradiance()), not to the object: one object can be placed several
 * times, and be seen by several views, each instance with its own.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
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

    /// The irradiance it was given, by its constructor or set_irradiance().
    TSpectral get_irradiance() const { return irradiance_; }

    virtual std::vector<TSpectral>
    resolve_irradiance(const std::vector<Transform<float>>& self_transforms,
                       const std::vector<Time>& times,
                       const SceneView<TSpectral>& scene_view,
                       RandomSampler<float>& sampler) const;

    virtual std::string type() const override { return "UnresolvedObject"; }

  protected:
    /// The irradiance it was given.
    TSpectral irradiance_{0};
};
} // namespace huira

#include "huira_impl/assets/unresolved/unresolved_object.ipp"
