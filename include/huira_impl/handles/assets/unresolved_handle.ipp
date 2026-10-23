#include "huira/concepts/spectral_concepts.hpp"
#include "huira/units/units.hpp"

namespace huira {
/**
 * @brief Sets the spectral irradiance of the unresolved object.
 *
 * Updates the irradiance value of the underlying unresolved object.
 *
 * @param spectral_irradiance The new spectral irradiance value in W·m⁻².
 */
template <IsSpectral TSpectral>
void UnresolvedObjectHandle<TSpectral>::set_irradiance(
    const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance) const
{
    this->get_()->set_irradiance(spectral_irradiance);
}

/**
 * @brief Sets the total irradiance of the unresolved object.
 *
 * Updates the irradiance value of the underlying unresolved object using a total irradiance value.
 *
 * @param irradiance The new total irradiance value in W·m⁻².
 */
template <IsSpectral TSpectral>
void UnresolvedObjectHandle<TSpectral>::set_irradiance(
    const units::WattsPerMeterSquared& irradiance) const
{
    this->get_()->set_irradiance(irradiance);
}

/**
 * @brief Returns the irradiance the unresolved object was given.
 *
 * This is the irradiance a plain unresolved object gives wherever it is placed. That of an
 * instance of one whose irradiance depends on where it is (an emitter, sphere or asteroid) is
 * resolved by a scene view: see SceneView::unresolved_irradiance().
 *
 * @return The spectral irradiance it was given, in W·m⁻².
 */
template <IsSpectral TSpectral>
TSpectral UnresolvedObjectHandle<TSpectral>::get_irradiance() const
{
    return this->get_()->get_irradiance();
}
} // namespace huira
