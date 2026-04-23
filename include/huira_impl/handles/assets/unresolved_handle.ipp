#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/time.hpp"
#include "huira/core/units/units.hpp"

namespace huira {
    /**
     * @brief Sets the spectral irradiance of the unresolved object.
     *
     * Updates the irradiance value of the underlying unresolved object.
     *
     * @param spectral_irradiance The new spectral irradiance value in W·m⁻².
     */
    template <IsSpectral TSpectral>
    void UnresolvedObjectHandle<TSpectral>::set_irradiance(const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance) const
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
    void UnresolvedObjectHandle<TSpectral>::set_irradiance(const units::WattsPerMeterSquared& irradiance) const
    {
        this->get_()->set_irradiance(irradiance);
    }


    /**
     * @brief Returns the spectral irradiance at a given time.
     *
     * Queries the underlying unresolved object for its current spectral irradiance.
     *
     * @param time The time at which to query irradiance.
     * @return The current spectral irradiance value.
     */
    template <IsSpectral TSpectral>
    TSpectral UnresolvedObjectHandle<TSpectral>::get_irradiance(Time time) const {
        return this->get_()->get_irradiance(time);
    }
}
