#include <cmath>

#include "glm/glm.hpp"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/core/time.hpp"
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
        double tsince = time.julian_years_since_j2000(TimeScale::TT);

        double delta = static_cast<double>(star_data.DEC + (star_data.pmDEC * tsince));
        double alpha = static_cast<double>(star_data.RA + (star_data.pmRA * tsince / std::cos(star_data.DEC)));
        
        double x = std::cos(delta) * std::cos(alpha);
        double y = std::cos(delta) * std::sin(alpha);
        double z = std::sin(delta);

        direction_ = glm::normalize(Vec3<double>{ x, y, z });
    }
}
