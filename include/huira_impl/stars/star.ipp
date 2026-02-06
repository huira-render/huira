#include <cmath>

#include "glm/glm.hpp"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/core/time.hpp"
#include "huira/core/physics.hpp"
#include "huira/stars/io/star_data.hpp"

namespace huira {

    template <IsSpectral TSpectral>
    Star<TSpectral>::Star(const Vec3<double>& direction, TSpectral irradiance) :
        direction_{ direction }, irradiance_{ irradiance }
    {
        direction_ = glm::normalize(direction_);
    }

    /**
     * @brief Constructs a Star from catalog data, applying proper motion correction.
     *
     * Computes the star's unit direction vector in the ICRS frame by applying
     * proper motion corrections to the catalog position. The proper motion is
     * extrapolated from the catalog epoch (J2000.0) to the specified observation time.
     *
     * @tparam TSpectral A type satisfying the IsSpectral concept, representing the
     *                   star's spectral characteristics.
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
        // Compute the ICRF direction:
        double years_since_j2000 = time.julian_years_since_j2000(TimeScale::TT);

        compute_direction_(star_data.RA, star_data.DEC, star_data.pmRA, star_data.pmDEC, years_since_j2000);
        compute_irradiance_(star_data.temperature, star_data.solid_angle);
    }


    template <IsSpectral TSpectral>
    Star<TSpectral>::Star(const StarData& star_data, double years_since_j2000)
    {
        compute_direction_(star_data.RA, star_data.DEC, star_data.pmRA, star_data.pmDEC, years_since_j2000);
        compute_irradiance_(star_data.temperature, star_data.solid_angle);
    }

    template <IsSpectral TSpectral>
    void Star<TSpectral>::compute_direction_(double RA, double DEC, float pmRAmas, float pmDECmas, double years_since_j2000)
    {
        constexpr double MAS_TO_RAD = PI<double>() / (180.0 * 3600.0 * 1000.0);
        double pmDEC = static_cast<double>(pmDECmas) * MAS_TO_RAD;
        double pmRA = static_cast<double>(pmRAmas) * MAS_TO_RAD;

        double delta = static_cast<double>(DEC + (pmDEC * years_since_j2000));
        double alpha = static_cast<double>(RA + (pmRA * years_since_j2000 / std::cos(delta)));

        double x = std::cos(delta) * std::cos(alpha);
        double y = std::cos(delta) * std::sin(alpha);
        double z = std::sin(delta);

        direction_ = glm::normalize(Vec3<double>{ x, y, z });
    }

    template <IsSpectral TSpectral>
    void Star<TSpectral>::compute_irradiance_(float temperature, double solid_angle)
    {
        TSpectral spectral_radiance = black_body<TSpectral>(static_cast<double>(temperature));
        for (std::size_t i = 0; i < spectral_radiance.size(); ++i) {
            irradiance_[i] = static_cast<float>(static_cast<double>(spectral_radiance[i]) * solid_angle);
        }
    }
}
