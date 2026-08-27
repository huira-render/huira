
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Radius of the first Airy zero, in pixels.
 *
 * @param max_wavelength Longest bin center wavelength in meters.
 * @param f_number Focal length divided by aperture diameter.
 * @param min_pitch Smaller of the two pixel pitches, in meters.
 * @return The radius in pixels, or 0 for degenerate inputs.
 */
inline float
DiffractionCore::airy_radius_pixels(double max_wavelength, double f_number, float min_pitch)
{
    if (!(max_wavelength > 0.0) || !(f_number > 0.0) || !(min_pitch > 0.f)) {
        return 0.f;
    }
    return static_cast<float>(1.22 * max_wavelength * f_number / static_cast<double>(min_pitch));
}

/**
 * @brief Stamping radius used when DiffractionCore::radius is unset.
 *
 * Spans roughly sixteen Airy rings, bounded to a range that keeps the polyphase cache
 * affordable. For well-sampled optics the Airy core is smaller than a pixel and the lower
 * bound applies; the scaling only takes over for oversampled systems.
 *
 * @param max_wavelength Longest bin center wavelength in meters.
 * @param f_number Focal length divided by aperture diameter.
 * @param min_pitch Smaller of the two pixel pitches, in meters.
 * @return The radius in pixels.
 */
inline int DiffractionCore::derive_radius(double max_wavelength, double f_number, float min_pitch)
{
    constexpr int MIN_RADIUS = 32;
    constexpr int MAX_RADIUS = 64;
    constexpr float RINGS = 16.f;

    const float airy = airy_radius_pixels(max_wavelength, f_number, min_pitch);
    const int scaled = static_cast<int>(std::ceil(RINGS * airy));
    return std::clamp(scaled, MIN_RADIUS, MAX_RADIUS);
}

/**
 * @brief Fraction of the scattered energy falling inside the given radius.
 *
 * The profile integrates in closed form to E(R) = 1 - (1 + (R / r0)^2)^(1 - b/2).
 *
 * @param radius Radius in pixels.
 * @param exponent Power-law falloff exponent b.
 * @param r0 Shoulder radius in pixels.
 * @return The enclosed energy fraction in [0, 1].
 */
inline float HarveyShack::energy_within(float radius, float exponent, float r0)
{
    if (!(radius > 0.f) || !(r0 > 0.f)) {
        return 0.f;
    }
    if (!(exponent > 2.f)) {
        return 0.f;
    }
    const double ratio = static_cast<double>(radius) / static_cast<double>(r0);
    const double u = 1.0 + ratio * ratio;
    return static_cast<float>(1.0 - std::pow(u, 1.0 - 0.5 * static_cast<double>(exponent)));
}

/**
 * @brief Radius enclosing the requested fraction of the scattered energy.
 *
 * Inverts energy_within(). The result grows steeply as the exponent approaches 2, and is
 * infinite at or below it, where the profile carries unbounded energy.
 *
 * @param captured_energy Target enclosed fraction in (0, 1).
 * @param exponent Power-law falloff exponent b.
 * @param r0 Shoulder radius in pixels.
 * @return The radius in pixels, or infinity when no finite radius satisfies the target.
 */
inline float HarveyShack::radius_for_energy(float captured_energy, float exponent, float r0)
{
    if (!(exponent > 2.f) || !(r0 > 0.f)) {
        return std::numeric_limits<float>::infinity();
    }
    if (!(captured_energy > 0.f)) {
        return 0.f;
    }
    if (captured_energy >= 1.f) {
        return std::numeric_limits<float>::infinity();
    }

    const double remainder = 1.0 - static_cast<double>(captured_energy);
    const double power = 2.0 / (2.0 - static_cast<double>(exponent));
    const double u = std::pow(remainder, power);
    if (!(u > 1.0)) {
        return 0.f;
    }
    return static_cast<float>(static_cast<double>(r0) * std::sqrt(u - 1.0));
}

/**
 * @brief Check the scatter parameters, throwing on any invalid combination.
 */
inline void HarveyShack::validate() const
{
    if (!(fraction >= 0.f) || fraction >= 1.f) {
        HUIRA_THROW_ERROR("HarveyShack - fraction must be in the range [0, 1): " +
                          std::to_string(fraction));
    }
    if (!(r0 > 0.f) || std::isinf(r0)) {
        HUIRA_THROW_ERROR("HarveyShack - r0 must be a positive finite value: " +
                          std::to_string(r0));
    }
    if (!(exponent > 0.f) || std::isinf(exponent)) {
        HUIRA_THROW_ERROR("HarveyShack - exponent must be a positive finite value: " +
                          std::to_string(exponent));
    }
    if (!(exponent > 2.f) && !cutoff_radius.has_value()) {
        HUIRA_THROW_ERROR("HarveyShack - exponent " + std::to_string(exponent) +
                          " carries unbounded energy. Use an exponent above 2, or set "
                          "cutoff_radius to truncate the profile.");
    }
    if (!(captured_energy > 0.f) || captured_energy >= 1.f) {
        HUIRA_THROW_ERROR("HarveyShack - captured_energy must be in the range (0, 1): " +
                          std::to_string(captured_energy));
    }
    if (cutoff_radius.has_value() && !(cutoff_radius.value() > 0.f)) {
        HUIRA_THROW_ERROR("HarveyShack - cutoff_radius must be positive: " +
                          std::to_string(cutoff_radius.value()));
    }
    if (kernel_radius.has_value() && kernel_radius.value() < 1) {
        HUIRA_THROW_ERROR("HarveyShack - kernel_radius must be at least 1: " +
                          std::to_string(kernel_radius.value()));
    }
}

/**
 * @brief Check the stray-light budget, throwing if it is inconsistent.
 */
inline void StrayLight::validate() const
{
    if (!(veiling_glare >= 0.f) || veiling_glare > 1.f) {
        HUIRA_THROW_ERROR("StrayLight - veiling_glare must be in the range [0, 1]: " +
                          std::to_string(veiling_glare));
    }
    if (scatter.has_value()) {
        scatter->validate();
        const float total = scatter->fraction + veiling_glare;
        if (total >= 1.f) {
            HUIRA_THROW_ERROR(
                "StrayLight - scatter fraction plus veiling glare must leave energy in the "
                "core, but they sum to " +
                std::to_string(total));
        }
    }
}

/**
 * @brief Check the complete optical description, throwing on any invalid parameter.
 */
template <IsSpectral TSpectral>
void Optics<TSpectral>::validate() const
{
    if (const auto* diffraction = std::get_if<DiffractionCore>(&core)) {
        if (diffraction->radius.has_value() && diffraction->radius.value() < 1) {
            HUIRA_THROW_ERROR("DiffractionCore - radius must be at least 1: " +
                              std::to_string(diffraction->radius.value()));
        }
        if (diffraction->banks < 1) {
            HUIRA_THROW_ERROR("DiffractionCore - banks must be at least 1: " +
                              std::to_string(diffraction->banks));
        }
    } else if (const auto* measured = std::get_if<MeasuredCore<TSpectral>>(&core)) {
        if (measured->banks < 1) {
            HUIRA_THROW_ERROR("MeasuredCore - banks must be at least 1: " +
                              std::to_string(measured->banks));
        }
    } else if (const auto* custom = std::get_if<CustomCore<TSpectral>>(&core)) {
        if (custom->psf == nullptr) {
            HUIRA_THROW_ERROR("CustomCore - psf must not be null");
        }
    }

    stray_light.validate();
}

/**
 * @brief Optics with no PSF and no stray light.
 */
template <IsSpectral TSpectral>
Optics<TSpectral> Optics<TSpectral>::ideal()
{
    Optics optics;
    optics.core = IdealCore{};
    optics.stray_light = StrayLight{};
    return optics;
}

/**
 * @brief Diffraction-limited optics with no stray light.
 */
template <IsSpectral TSpectral>
Optics<TSpectral> Optics<TSpectral>::diffraction_limited()
{
    Optics optics;
    optics.core = DiffractionCore{};
    optics.stray_light = StrayLight{};
    return optics;
}

/**
 * @brief Diffraction-limited optics plus representative scatter and veiling glare.
 *
 * The stray-light values describe a clean, well-baffled instrument and are intended as a
 * starting point only. Replace them with values measured for the system being simulated
 * before using the result photometrically.
 */
template <IsSpectral TSpectral>
Optics<TSpectral> Optics<TSpectral>::realistic()
{
    HarveyShack scatter;
    scatter.fraction = 0.01f;
    scatter.exponent = 2.5f;
    scatter.r0 = 0.5f;
    scatter.captured_energy = 0.98f;

    Optics optics;
    optics.core = DiffractionCore{};
    optics.stray_light.scatter = scatter;
    optics.stray_light.veiling_glare = 0.005f;
    return optics;
}
} // namespace huira
