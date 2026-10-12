#include <algorithm>
#include <cmath>

#include "glm/glm.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/physics.hpp"
#include "huira/sampling/cone_sampling.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/validate.hpp"

namespace huira {
/**
 * @brief Constructs a SphereLight from spectral power.
 * @param radius The radius of the sphere in meters.
 * @param spectral_power Total spectral power emitted over the entire surface.
 */
template <IsSpectral TSpectral>
SphereLight<TSpectral>::SphereLight(const units::Meter& radius,
                                    const units::SpectralWatts<TSpectral>& spectral_power)
    : radius_(checked_radius_(radius))
{
    HUIRA_LOG_INFO("SphereLight::SphereLight(radius, spectral_power)");
    this->set_spectral_power(spectral_power);
}

/**
 * @brief Constructs a SphereLight from spectral radiance.
 * @param radius The radius of the sphere in meters.
 * @param spectral_radiance Spectral radiance emitted by the surface.
 */
template <IsSpectral TSpectral>
SphereLight<TSpectral>::SphereLight(
    const units::Meter& radius,
    const units::SpectralWattsPerMeterSquaredSteradian<TSpectral>& spectral_radiance)
    : radius_(checked_radius_(radius)), radiance_{spectral_radiance.to_si()}
{
    HUIRA_LOG_INFO("SphereLight::SphereLight(radius, spectral_radiance)");
    validate_spectrum(radiance_, "SphereLight radiance");
}

/**
 * @brief Constructs a SphereLight from total scalar power.
 * @param radius The radius of the sphere in meters.
 * @param power Total power emitted in Watts.
 */
template <IsSpectral TSpectral>
SphereLight<TSpectral>::SphereLight(const units::Meter& radius, const units::Watt& power)
    : radius_(checked_radius_(radius))
{
    HUIRA_LOG_INFO("SphereLight::SphereLight(radius, power)");
    this->set_spectral_power(power);
}

/**
 * @brief Constructs a blackbody SphereLight from a temperature.
 * @param radius The radius of the sphere in meters.
 * @param power Temperature in Kelvin.
 */
template <IsSpectral TSpectral>
SphereLight<TSpectral>::SphereLight(const units::Meter& radius, const units::Kelvin& temperature)
    : radius_(checked_radius_(radius))
{
    HUIRA_LOG_INFO("SphereLight::SphereLight(radius, temperature)");
    // black_body() checks the temperature:
    this->radiance_ = black_body<TSpectral>(temperature.to_si(), 1000);
    validate_spectrum(radiance_, "SphereLight radiance");
}

/// The radius a sphere light is given: finite and non-negative, and at least 1e-5 m, so that a
/// point-like light (0) has an area to spread its power over.
template <IsSpectral TSpectral>
float SphereLight<TSpectral>::checked_radius_(const units::Meter& radius)
{
    const float r = radius.to_si_f();
    if (!(r >= 0.f) || !std::isfinite(r)) {
        HUIRA_THROW_ERROR("SphereLight - The radius must be non-negative and finite: " +
                          std::to_string(radius.to_si()) + " m");
    }
    return std::max(r, 1e-5f);
}

template <IsSpectral TSpectral>
void SphereLight<TSpectral>::set_spectral_power(const units::Watt& power)
{
    float power_si = static_cast<float>(power.to_si());
    if (power_si < 0.f || std::isnan(power_si) || std::isinf(power_si)) {
        HUIRA_THROW_ERROR("SphereLight::set_spectral_power - Invalid power: " +
                          std::to_string(power_si));
    }

    // L = Phi / (4 * pi^2 * r^2)
    float surface_area = 4.0f * PI<float>() * radius_ * radius_;
    const TSpectral radiance{power_si / (PI<float>() * surface_area)};
    validate_spectrum(radiance, "SphereLight radiance");
    this->radiance_ = radiance;
}

template <IsSpectral TSpectral>
void SphereLight<TSpectral>::set_spectral_power(
    const units::SpectralWatts<TSpectral>& spectral_power)
{
    TSpectral power_si = spectral_power.to_si();
    validate_spectrum(power_si, "SphereLight spectral power");
    float surface_area = 4.0f * PI<float>() * radius_ * radius_;
    const TSpectral radiance = power_si / (PI<float>() * surface_area);
    // Finite power over a tiny sphere can still overflow:
    validate_spectrum(radiance, "SphereLight radiance");
    this->radiance_ = radiance;
}

template <IsSpectral TSpectral>
std::optional<LightSample<TSpectral>>
SphereLight<TSpectral>::sample_li(const Interaction<TSpectral>& isect,
                                  const Transform<float>& transform,
                                  Sampler<float>& sampler) const
{
    // Uniformly sample the cone subtended by the sphere (shared with
    // indirect-source sampling; see huira/sampling/cone_sampling.hpp).
    auto cone = sample_sphere_cone(isect.position, transform.position, radius_, sampler);

    // If the shading point is inside the light sphere, it receives no direct lighting
    if (!cone) {
        return std::nullopt;
    }

    LightSample<TSpectral> ls;
    ls.wi = cone->wi;
    ls.Li = radiance_;
    ls.pdf = cone->pdf;
    ls.distance = cone->distance;

    return ls;
}

template <IsSpectral TSpectral>
float SphereLight<TSpectral>::pdf_li(const Interaction<TSpectral>& isect,
                                     const Transform<float>& transform,
                                     const Vec3<float>& wi) const
{
    return pdf_sphere_cone(isect.position, transform.position, radius_, wi);
}

template <IsSpectral TSpectral>
TSpectral SphereLight<TSpectral>::radiance(const Vec3<float>& point_on_light,
                                           const Vec3<float>& outgoing_direction) const
{
    (void)point_on_light;
    (void)outgoing_direction;
    return radiance_;
}

template <IsSpectral TSpectral>
TSpectral SphereLight<TSpectral>::irradiance_at(const Vec3<float>& position,
                                                const Transform<float>& light_to_world) const
{
    Vec3<float> wc = light_to_world.position - position;
    float d2 = glm::dot(wc, wc);
    float r2 = radius_ * radius_;

    if (d2 <= r2) {
        return TSpectral{0};
    }

    // E = L * pi * (r^2 / d^2)
    float sin2_theta_max = r2 / d2;
    return radiance_ * PI<float>() * sin2_theta_max;
}
} // namespace huira
