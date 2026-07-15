#include <algorithm>
#include <cmath>
#include <utility>
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
 * Updates the object's irradiance value as a single constant entry. All spectral
 * components must be non-negative, as negative irradiance is physically meaningless.
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
    this->irradiance_ = {irradiance_si};
}

/**
 * @brief Sets the total irradiance of the unresolved object.
 *
 * Updates the object's irradiance by converting a total irradiance value
 * (in watts per square meter) to the spectral representation, stored as a
 * single constant entry. The total irradiance must be non-negative, as
 * negative values are physically meaningless.
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
    this->irradiance_ = {TSpectral::from_total(irradiance_si)};
}

/**
 * @brief Returns the spectral irradiance at a given time.
 *
 * If the object holds a single (constant) irradiance entry, it is returned
 * directly. Otherwise, the per-temporal-sample entries are linearly interpolated
 * over the time range cached by set_resolved_irradiance_(). Times outside the
 * range are clamped to the endpoints.
 *
 * @param time The time at which to query irradiance.
 * @return The spectral irradiance value at the requested time.
 */
template <IsSpectral TSpectral>
TSpectral UnresolvedObject<TSpectral>::get_irradiance(Time time) const
{
    if (irradiance_.size() == 1) {
        return irradiance_.front();
    }

    const double duration = et_end_ - et_start_;
    float t = 0.0f;
    if (duration > 0.0) {
        t = static_cast<float>(std::clamp((time.et() - et_start_) / duration, 0.0, 1.0));
    }

    float scaled = t * static_cast<float>(irradiance_.size() - 1);
    std::size_t lo = static_cast<std::size_t>(std::floor(scaled));
    lo = std::min(lo, irradiance_.size() - 2);
    float frac = scaled - static_cast<float>(lo);

    return irradiance_[lo] + frac * (irradiance_[lo + 1] - irradiance_[lo]);
}

/**
 * @brief Resolves the spectral irradiance from scene state.
 *
 * This method provides a hook for subclasses to compute or update the object's
 * per-temporal-sample irradiance based on its transforms and the contents of the
 * scene view (lights, occluders, indirect sources). It is called by SceneView
 * after the acceleration structure has been built, so implementations may cast
 * rays through the view. The default implementation does nothing, leaving the
 * irradiance unchanged.
 *
 * @param self_transforms Camera-relative transforms of this object, one per temporal sample.
 * @param times Absolute times of the temporal samples (same size as self_transforms).
 * @param scene_view The fully constructed scene view.
 * @param sampler Random sampler for stochastic irradiance estimation.
 */
template <IsSpectral TSpectral>
void UnresolvedObject<TSpectral>::resolve_irradiance(
    const std::vector<Transform<float>>& self_transforms,
    const std::vector<Time>& times,
    const SceneView<TSpectral>& scene_view,
    RandomSampler<float>& sampler)
{
    // Default: do nothing, irradiance stays as initialized
    (void)self_transforms;
    (void)times;
    (void)scene_view;
    (void)sampler;
}

/**
 * @brief Stores resolved per-temporal-sample irradiances and their time range.
 *
 * Validates the irradiance values and caches the time range endpoints used by
 * get_irradiance() for interpolation. `irradiances` must either contain a
 * single (constant) entry, or exactly one entry per entry in `times`.
 *
 * @param irradiances Resolved spectral irradiances, one per temporal sample (or a
 *                    single constant entry).
 * @param times Absolute times of the temporal samples.
 * @throws std::runtime_error if sizes are inconsistent or any value is invalid.
 */
template <IsSpectral TSpectral>
void UnresolvedObject<TSpectral>::set_resolved_irradiance_(std::vector<TSpectral> irradiances,
                                                           const std::vector<Time>& times)
{
    if (irradiances.empty()) {
        HUIRA_THROW_ERROR("UnresolvedObject::set_resolved_irradiance_ - Received no irradiances");
    }
    if (irradiances.size() != 1 && irradiances.size() != times.size()) {
        HUIRA_THROW_ERROR("UnresolvedObject::set_resolved_irradiance_ - Irradiance count (" +
                          std::to_string(irradiances.size()) +
                          ") does not match temporal sample count (" +
                          std::to_string(times.size()) + ")");
    }
    for (const TSpectral& irradiance : irradiances) {
        if (!irradiance.valid()) {
            HUIRA_THROW_ERROR(
                "UnresolvedObject::set_resolved_irradiance_ - Invalid spectral irradiance: " +
                irradiance.to_string());
        }
    }

    if (!times.empty()) {
        this->et_start_ = times.front().et();
        this->et_end_ = times.back().et();
    }
    this->irradiance_ = std::move(irradiances);
}
} // namespace huira
