#include <string>

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    /**
     * @brief Sets the irradiance of the unresolved light source.
     * 
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param irradiance The spectral irradiance value to set
     */
    template <IsSpectral TSpectral>
    void UnresolvedObjectHandle<TSpectral>::set_irradiance(const TSpectral& irradiance) const
    {
        this->get()->set_irradiance(irradiance);
    }

    /**
     * @brief Gets the irradiance of the unresolved light source.
     * 
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return TSpectral The current spectral irradiance value
     */
    template <IsSpectral TSpectral>
    TSpectral UnresolvedObjectHandle<TSpectral>::get_irradiance() const {
        return this->get()->get_irradiance();
    }
}
