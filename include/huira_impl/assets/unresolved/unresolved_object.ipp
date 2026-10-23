#include <cmath>
#include <vector>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/transform.hpp"
#include "huira/scene/scene_view_types.hpp"
#include "huira/util/logger.hpp"

namespace huira {
/**
 * @brief Constructs an UnresolvedObject with specified spectral irradiance.
 *
 * Creates an unresolved object and initializes its spectral irradiance value.
 * The provided irradiance is validated to ensure all components are non-negative
 * and finite.
 *
 * @param spectral_irradiance The initial spectral irradiance in \f$W \cdot m^{-2}\f$.
 * @throws std::runtime_error if the irradiance contains invalid values.
 */
template <IsSpectral TSpectral>
UnresolvedObject<TSpectral>::UnresolvedObject(
    const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance)
{
    HUIRA_LOG_INFO("UnresolvedObject::UnresolvedObject(spectral_irradiance)");
    this->set_irradiance(spectral_irradiance);
}

/**
 * @brief Constructs an UnresolvedObject from total irradiance.
 *
 * Creates an unresolved object with a total irradiance value that is distributed across
 * spectral bins proportionally to their wavelength widths.
 *
 * @param irradiance The total irradiance in \f$W \cdot m^{-2}\f$.
 * @throws std::runtime_error if the irradiance is negative, NaN, or infinite.
 */
template <IsSpectral TSpectral>
UnresolvedObject<TSpectral>::UnresolvedObject(const units::WattsPerMeterSquared& irradiance)
{
    HUIRA_LOG_INFO("UnresolvedObject::UnresolvedObject(irradiance)");
    this->set_irradiance(irradiance);
}

/**
 * @brief Sets the spectral irradiance of the unresolved object.
 *
 * Updates the object's irradiance. All spectral components must be non-negative, as negative
 * irradiance is physically meaningless.
 *
 * @param spectral_irradiance The new spectral irradiance value in \f$W \cdot m^{-2}\f$.
 * @throws std::runtime_error if any irradiance component is negative.
 */
template <IsSpectral TSpectral>
void UnresolvedObject<TSpectral>::set_irradiance(
    const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance)
{
    TSpectral irradiance_si = spectral_irradiance.to_si();
    if (!irradiance_si.valid()) {
        HUIRA_THROW_ERROR("UnresolvedObject::set_irradiance - Invalid spectral irradiance: " +
                          irradiance_si.to_string());
    }
    this->irradiance_ = irradiance_si;
}

/**
 * @brief Sets the total irradiance of the unresolved object.
 *
 * Updates the object's irradiance by converting a total irradiance value
 * (in watts per square meter) to the spectral representation. The total irradiance must be
 * non-negative, as negative values are physically meaningless.
 *
 * @param irradiance The new total irradiance value in \f$W \cdot m^{-2}\f$.
 * @throws std::runtime_error if the total irradiance is negative, NaN, or infinite.
 */
template <IsSpectral TSpectral>
void UnresolvedObject<TSpectral>::set_irradiance(const units::WattsPerMeterSquared& irradiance)
{
    float irradiance_si = static_cast<float>(irradiance.to_si());
    if (irradiance_si < 0.0f || std::isnan(irradiance_si) || std::isinf(irradiance_si)) {
        HUIRA_THROW_ERROR("UnresolvedObject::set_irradiance - Invalid irradiance: " +
                          std::to_string(irradiance_si) + " W/m^2");
    }
    this->irradiance_ = TSpectral::from_total(irradiance_si);
}

/**
 * @brief Resolves the spectral irradiance of an instance from the scene's state.
 *
 * This method provides a hook for subclasses to compute an instance's irradiance at the
 * camera at each of its temporal samples, from its transforms and the contents of the scene
 * view (lights, occluders, indirect sources). It is called by SceneView for each instance,
 * after the acceleration structure has been built, so implementations may cast rays through
 * the view; the view keeps what it returns, for that instance (see
 * SceneView::unresolved_irradiance()). The default gives the irradiance the object was given.
 *
 * @param self_transforms Camera-relative transforms of the instance, one per temporal sample.
 * @param times Absolute times of the temporal samples (same size as self_transforms).
 * @param scene_view The fully constructed scene view.
 * @param sampler Random sampler for stochastic irradiance estimation.
 * @return The irradiance at each temporal sample, or one for all of them. Each must be finite
 *         and non-negative.
 */
template <IsSpectral TSpectral>
std::vector<TSpectral> UnresolvedObject<TSpectral>::resolve_irradiance(
    const std::vector<Transform<float>>& self_transforms,
    const std::vector<Time>& times,
    const SceneView<TSpectral>& scene_view,
    RandomSampler<float>& sampler) const
{
    (void)self_transforms;
    (void)times;
    (void)scene_view;
    (void)sampler;
    return {irradiance_};
}
} // namespace huira
