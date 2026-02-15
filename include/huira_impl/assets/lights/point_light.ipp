#include <optional>

#include "glm/glm.hpp"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
#include "huira/render/interaction.hpp"
#include "huira/render/sampler.hpp"
#include "huira/util/logger.hpp"

namespace huira {
    /**
     * @brief Constructs a PointLight from spectral power.
     *
     * The irradiance is computed by dividing the total spectral power by \f$4\pi\f$,
     * representing uniform emission over a sphere.  The input must be real and non-negative.
     *
     * @param spectral_power The total spectral power emitted by the light source.
     */
    template <IsSpectral TSpectral>
    PointLight<TSpectral>::PointLight(const units::SpectralWatts<TSpectral>& spectral_power)
    {
        this->set_spectral_power(spectral_power);
    }

    /**
     * @brief Constructs a PointLight from total spectral power.
     *
     * The irradiance is computed by dividing the total spectral power by \f$4\pi\f$,
     * representing uniform emission over a sphere, and that irradiance is then
     * spread proportionally across all spectral bins. The input must be real and non-negative.
     *
     * @param power The total spectral power emitted by the light source.
     */
    template <IsSpectral TSpectral>
    PointLight<TSpectral>::PointLight(const units::Watt& power)
    {
        this->set_spectral_power(power);
    }

    /**
     * @brief Samples the incident radiance from this point light.
     * 
     * Computes the light contribution at a reference point by calculating the
     * direction to the light, distance, and incident radiance based on inverse
     * square falloff.
     * 
     * @param ref The surface interaction point being illuminated.
     * @param light_to_world Transform from light's local space to world space.
     * @param sampler Random sampler (unused for point lights).
     * @return A LightSample containing direction, radiance, distance, and PDF,
     *         or std::nullopt if the reference point coincides with the light.
     */
    template <IsSpectral TSpectral>
    std::optional<LightSample<TSpectral>> PointLight<TSpectral>::sample_li(
        const Interaction<TSpectral>& ref,
        const Transform<float>& light_to_world,
        const Sampler<float>& sampler
    ) const
    {
        (void)sampler;
        LightSample<TSpectral> sample;
        
        Vec3<float> p_light = light_to_world.apply_to_point(Vec3<float>(0, 0, 0));

        // Compute shading point:
        Vec3<float> wi_unorm = p_light - ref.position;
        float dist_sq = glm::dot(wi_unorm, wi_unorm);
        sample.distance = std::sqrt(dist_sq);

        // Shading point exactly coincides with light:
        if (sample.distance == 0) {
            return std::nullopt;
        }

        sample.wi = wi_unorm / sample.distance;

        // Compute irradiance (radiance for point light):
        sample.Li = irradiance_ / dist_sq;
        
        // Point lights have PDF of 1
        sample.pdf = 1.0f;

        return sample;
    }

    /**
     * @brief Returns the probability density for sampling the given direction.
     * 
     * For point lights, the PDF is always 1.0 since there is only one possible
     * direction to the light from any given point.
     * 
     * @param ref The surface interaction point.
     * @param light_to_world Transform from light's local space to world space.
     * @param wi The incident direction being queried.
     * @return Always returns 1.0 for point lights.
     */
    template <IsSpectral TSpectral>
    float PointLight<TSpectral>::pdf_li(
        const Interaction<TSpectral>& ref,
        const Transform<float>& light_to_world,
        const Vec3<float>& wi
    ) const
    {
        (void)ref;
        (void)light_to_world;
        (void)wi;
        return 1.f;
    }

    /**
     * @brief Computes the spectral irradiance at a given position.
     * 
     * Calculates the spectral irradiance based on the distance from the light
     * to the position, using inverse square law falloff.  The value is returned
     * in SI units: \f$W \cdot m^{-2}\f$.
     * 
     * @param position The world-space position to evaluate irradiance at.
     * @param light_to_world Transform from light's local space to world space.
     * @return The spectral irradiance at the given position.
     */
    template <IsSpectral TSpectral>
    TSpectral PointLight<TSpectral>::irradiance_at(
        const Vec3<float>& position,
        const Transform<float>& light_to_world
    ) const
    {
        Vec3<float> p_light = light_to_world.apply_to_point(Vec3<float>(0, 0, 0));
        float dist_sq = glm::dot(position - p_light, position - p_light);
        return irradiance_ / dist_sq;
    }


    /**
     * @brief Sets the total spectral power emitted by the point light.
     *
     * Updates the light's emitted irradiance by dividing the total spectral power
     * over \f$4\pi\f$, representing uniform emission in all directions. All components must
     * be real and non-negative.
     *
     * @param spectral_power The new total spectral power value.
     * @throws std::runtime_error if any spectral power component is invalid.
     */
    template <IsSpectral TSpectral>
    void PointLight<TSpectral>::set_spectral_power(const units::SpectralWatts<TSpectral>& spectral_power) {
        TSpectral power_si = spectral_power.to_si();
        if (!power_si.valid()) {
            HUIRA_THROW_ERROR("PointLight::PointLight - Invalid spectral power: " +
                power_si.to_string());
        }
        // Divide by 4*pi for uniform emission over a sphere
        this->irradiance_ = power_si / (4.f * PI<float>());
    }

    /**
     * @brief Sets the total spectral power emitted by the point light.
     *
     * Sets the total emitted spectral power (in all directions) for the point light.
     * The value is divided by \f$4\pi\f$ to compute the irradiance, representing uniform
     * emission over a sphere. This irradiance is then spread proportionally across all
     * spectral bins.  The input must be real and non-negative.
     *
     * @param power The total spectral power to set (must be real and non-negative).
     * @throws std::runtime_error if the power is invalid (negative, NaN, or infinite).
     */
    template <IsSpectral TSpectral>
    void PointLight<TSpectral>::set_spectral_power(const units::Watt& power)
    {
        float power_si = static_cast<float>(power.to_si());
        if (power_si < 0.f || std::isnan(power_si) || std::isinf(power_si)) {
            HUIRA_THROW_ERROR("PointLight::set_total_spectral_power - Invalid power: " +
                std::to_string(power_si));
        }

        // Divide by 4*pi for uniform emission over a sphere
        float irradiance = power_si / (4.f * PI<float>());
        this->irradiance_ = TSpectral::from_total(irradiance);
    }
}
