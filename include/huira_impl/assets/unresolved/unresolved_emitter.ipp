#include <vector>

#include "glm/glm.hpp"

#include "huira/assets/lights/light.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/units/units.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
    /**
     * @brief Constructs an UnresolvedEmitter from spectral power.
     * 
     * Creates an emitter with the specified spectral power distribution.
     * The spectral power is validated to ensure all components are non-negative
     * and finite.
     * 
     * @param spectral_power The spectral power in watts per wavelength bin.
     * @throws std::runtime_error if the spectral power contains invalid values.
     */
    template <IsSpectral TSpectral>
    UnresolvedEmitter<TSpectral>::UnresolvedEmitter(const units::SpectralWatts<TSpectral>& spectral_power)
    {
        this->set_spectral_power(spectral_power);
    }

    /**
     * @brief Constructs an UnresolvedEmitter from total power.
     * 
     * Creates an emitter with a total power value that is distributed across
     * spectral bins proportionally to their wavelength widths.
     * 
     * @param power The total power in watts.
     * @throws std::runtime_error if the power is negative, NaN, or infinite.
     */
    template <IsSpectral TSpectral>
    UnresolvedEmitter<TSpectral>::UnresolvedEmitter(const units::Watt& power)
    {
        this->set_spectral_power(power);
    }

    /**
     * @brief Resolves the spectral irradiance based on distance and spectral power.
     * 
     * Computes the irradiance at the observer (assumed to be at the origin) using
     * the inverse square law. The irradiance is calculated as:
     * \f$E = \frac{\Phi}{4\pi d^2}\f$, where \f$\Phi\f$ is the spectral power
     * and \f$d\f$ is the distance from the observer.
     * 
     * @param self_transform The world-space transform of the emitter.
     * @param lights A vector of all light instances in the scene (unused).
     */
    template <IsSpectral TSpectral>
    void UnresolvedEmitter<TSpectral>::resolve_irradiance(
        const Transform<float>& self_transform,
        const std::vector<LightInstance<TSpectral>>& lights
    ) 
    {
        (void)lights; // Not needed for this implementation

        float distance = glm::length(self_transform.position);

        float distance_sq = distance * distance;
        this->irradiance_ = spectral_power_ / (4.f * PI<float>() * distance_sq);
    }

    /**
     * @brief Sets the spectral power of the emitter.
     * 
     * Updates the emitter's spectral power distribution. The values are converted
     * to SI units and validated to ensure all components are non-negative and finite.
     * 
     * @param spectral_power The new spectral power in watts per wavelength bin.
     * @throws std::runtime_error if the spectral power contains invalid values.
     */
    template <IsSpectral TSpectral>
    void UnresolvedEmitter<TSpectral>::set_spectral_power(const units::SpectralWatts<TSpectral>& spectral_power)
    {
        TSpectral spectral_power_si = spectral_power.to_si();
        if (!spectral_power_si.valid()) {
            HUIRA_THROW_ERROR("UnresolvedEmitter::set_spectral_power - Invalid spectral power: " +
                spectral_power_si.to_string());
        }
        this->spectral_power_ = spectral_power_si;
    }

    /**
     * @brief Sets the total power of the emitter.
     * 
     * Updates the emitter's power by distributing the total value proportionally
     * across spectral bins based on their wavelength widths. The power is validated
     * to ensure it is non-negative and finite.
     * 
     * @param power The total power in watts.
     * @throws std::runtime_error if the power is negative, NaN, or infinite.
     */
    template <IsSpectral TSpectral>
    void UnresolvedEmitter<TSpectral>::set_spectral_power(const units::Watt& power)
    {
        float power_si = static_cast<float>(power.to_si());
        if (power_si < 0.f || std::isnan(power_si) || std::isinf(power_si)) {
            HUIRA_THROW_ERROR("UnresolvedEmitter::set_spectral_power - Invalid power: " +
                std::to_string(power_si));
        }
        this->spectral_power_ = TSpectral::from_total(power_si);
    }
}
