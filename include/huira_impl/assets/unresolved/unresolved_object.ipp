#include <vector>

#include "huira/core/concepts/spectral_concepts.hpp"
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
    UnresolvedObject<TSpectral>::UnresolvedObject(const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance)
        : id_(next_id_++)
    {
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
        : id_(next_id_++)
    {
        this->set_irradiance(irradiance);
    }

    /**
     * @brief Sets the spectral irradiance of the unresolved object.
     * 
     * Updates the object's irradiance value. All spectral components must be
     * non-negative, as negative irradiance is physically meaningless.
     * 
     * @param spectral_irradiance The new spectral irradiance value in \f$W \cdot m^{-2}\f$.
     * @throws std::runtime_error if any irradiance component is negative.
     */
    template <IsSpectral TSpectral>
    void UnresolvedObject<TSpectral>::set_irradiance(const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance)
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
     * (in watts per square meter) to the spectral representation. The total
     * irradiance must be non-negative, as negative values are physically
     * meaningless.
     * 
     * @param irradiance The new total irradiance value in \f$W \cdot m^{-2}\f$.
     * @throws std::runtime_error if the total irradiance is negative, NaN, or infinite.
     */
    template <IsSpectral TSpectral>
    void UnresolvedObject<TSpectral>::set_irradiance(const units::WattsPerMeterSquared& irradiance) {
        float irradiance_si = static_cast<float>(irradiance.to_si());
        if (irradiance_si < 0.0f || std::isnan(irradiance_si) || std::isinf(irradiance_si)) {
            HUIRA_THROW_ERROR("UnresolvedObject::set_irradiance - Invalid irradiance: " + 
                std::to_string(irradiance_si) + " W/m^2");
        }
        this->irradiance_ = TSpectral::from_total(irradiance_si);
    }

    /**
     * @brief Returns the spectral irradiance at a given time.
     *
     * This method provides a hook for future derived classes to implement time-varying irradiance.
     * The default implementation assumes irradiance is constant once computed or set.
     *
     * @param time The time at which to query irradiance (unused in default implementation).
     * @return The current spectral irradiance value.
     */
    template <IsSpectral TSpectral>
    TSpectral UnresolvedObject<TSpectral>::get_irradiance(Time time) const
    {
        (void)time; // Not required in default implementations
        return irradiance_;
    }

    /**
     * @brief Resolves the spectral irradiance based on scene lights and object transform.
     * 
     * This method provides a hook for subclasses to compute or update the object's
     * irradiance based on its transform and the lights present in the scene.
     * The default implementation does nothing, leaving the irradiance unchanged.
     * 
     * @param self_transform The world-space transform of this object.
     * @param lights A vector of all light instances in the scene.
     */
    template <IsSpectral TSpectral>
    void UnresolvedObject<TSpectral>::resolve_irradiance(
            const Transform<float>& self_transform,
            const std::vector<LightInstance<TSpectral>>& lights
        ) {
            // Default: do nothing, irradiance stays as initialized
            (void)self_transform;
            (void)lights;
        }
}
