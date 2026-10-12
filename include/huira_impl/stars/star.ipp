#include <cmath>
#include <string>

#include "glm/glm.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/physics.hpp"
#include "huira/core/time.hpp"
#include "huira/core/types.hpp"
#include "huira/stars/io/star_data.hpp"
#include "huira/util/logger.hpp"
#include "huira/util/validate.hpp"

namespace huira {

/**
 * @brief Constructs a Star from its direction and irradiance.
 *
 * @param direction The direction to the star, in the ICRS frame; normalized.
 * @param irradiance Its spectral irradiance: finite and non-negative.
 * @throws std::runtime_error if the direction is not finite or has no length, or the irradiance
 *         is negative or not finite.
 */
template <IsSpectral TSpectral>
Star<TSpectral>::Star(const Vec3<double>& direction, TSpectral irradiance)
    : direction_{direction}, irradiance_{irradiance}
{
    validate_real(direction, "Star direction");
    const double length = glm::length(direction);
    if (!(length > 0.0) || !std::isfinite(1.0 / length)) {
        HUIRA_THROW_ERROR("Star - The direction must have a length: " + vec_to_string(direction));
    }
    validate_spectrum(irradiance, "Star irradiance");
    direction_ = direction / length;
}

/**
 * @brief Whether catalog data describes a star that can be drawn: a finite position and proper
 * motion, a temperature that is finite and not below absolute zero, and a finite, non-negative
 * solid angle. Noisy catalog photometry can give neither.
 */
template <IsSpectral TSpectral>
bool Star<TSpectral>::usable(const StarData& star_data)
{
    return std::isfinite(star_data.RA) && std::isfinite(star_data.DEC) &&
           std::isfinite(star_data.pmRA) && std::isfinite(star_data.pmDEC) &&
           star_data.temperature >= 0.f && std::isfinite(star_data.temperature) &&
           star_data.solid_angle >= 0.0 && std::isfinite(star_data.solid_angle);
}

/// Throws unless the catalog data is usable (see usable()).
template <IsSpectral TSpectral>
void Star<TSpectral>::check_(const StarData& star_data)
{
    if (!usable(star_data)) {
        HUIRA_THROW_ERROR("Star - Catalog star " + std::to_string(star_data.id) +
                          " needs a finite position and proper motion, a temperature that is "
                          "finite and not below absolute zero, and a finite, non-negative solid "
                          "angle");
    }
}

/**
 * @brief Constructs a Star from catalog data, applying proper motion correction.
 *
 * Computes the star's unit direction vector in the ICRS frame by applying
 * proper motion corrections to the catalog position. The proper motion is
 * extrapolated from the catalog epoch (J2000.0) to the specified observation time.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 *
 * @param star_data Catalog data for the star. Assumes:
 *                  - RA and DEC are in radians (ICRS, epoch J2000.0)
 *                  - pmRA is proper motion in RA*cos(DEC), in radians/year
 *                  - pmDEC is proper motion in DEC, in radians/year
 * @param time The observation time at which to compute the star's position.
 *
 * @note The proper motion in RA from catalogs like Tycho-2 is typically given
 *       as pmRA*cos(DEC) to account for convergence of meridians toward the poles.
 *       This implementation divides by cos(DEC) to recover the true angular
 *       displacement in right ascension.
 */
template <IsSpectral TSpectral>
Star<TSpectral>::Star(const StarData& star_data, Time time)
{
    check_(star_data);
    // Compute the ICRF direction:
    double years_since_j2000 = time.julian_years_since_j2000(TimeScale::TT);

    compute_direction_(
        star_data.RA, star_data.DEC, star_data.pmRA, star_data.pmDEC, years_since_j2000);
    compute_irradiance_(star_data.temperature, star_data.solid_angle);
}

template <IsSpectral TSpectral>
Star<TSpectral>::Star(const StarData& star_data, double years_since_j2000)
{
    check_(star_data);
    compute_direction_(
        star_data.RA, star_data.DEC, star_data.pmRA, star_data.pmDEC, years_since_j2000);
    compute_irradiance_(star_data.temperature, star_data.solid_angle);
}

template <IsSpectral TSpectral>
void Star<TSpectral>::update_direction(const StarData& star_data, Time time)
{
    double years_since_j2000 = time.julian_years_since_j2000(TimeScale::TT);
    compute_direction_(
        star_data.RA, star_data.DEC, star_data.pmRA, star_data.pmDEC, years_since_j2000);
}

template <IsSpectral TSpectral>
void Star<TSpectral>::update_direction(const StarData& star_data, double years_since_j2000)
{
    compute_direction_(
        star_data.RA, star_data.DEC, star_data.pmRA, star_data.pmDEC, years_since_j2000);
}

template <IsSpectral TSpectral>
void Star<TSpectral>::compute_direction_(
    double RA, double DEC, float pmRAmas, float pmDECmas, double years_since_j2000)
{
    validate_real(years_since_j2000, "Star years since J2000");
    constexpr double MAS_TO_RAD = PI<double>() / (180.0 * 3600.0 * 1000.0);
    double pmDEC = static_cast<double>(pmDECmas) * MAS_TO_RAD;
    double pmRA = static_cast<double>(pmRAmas) * MAS_TO_RAD;

    double delta = static_cast<double>(DEC + (pmDEC * years_since_j2000));
    // At a pole, right ascension has no meaning, and its motion would divide by zero:
    const double cos_delta = std::cos(delta);
    double alpha = std::abs(cos_delta) > 1e-12 ? RA + (pmRA * years_since_j2000 / cos_delta) : RA;

    double x = std::cos(delta) * std::cos(alpha);
    double y = std::cos(delta) * std::sin(alpha);
    double z = std::sin(delta);

    direction_ = glm::normalize(Vec3<double>{x, y, z});
}

template <IsSpectral TSpectral>
void Star<TSpectral>::compute_irradiance_(float temperature, double solid_angle)
{
    TSpectral spectral_radiance = black_body<TSpectral>(static_cast<double>(temperature));
    for (std::size_t i = 0; i < spectral_radiance.size(); ++i) {
        irradiance_[i] =
            static_cast<float>(static_cast<double>(spectral_radiance[i]) * solid_angle);
    }
}
} // namespace huira
