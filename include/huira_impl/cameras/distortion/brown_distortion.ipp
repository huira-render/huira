
#include <cmath>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

/**
 * @brief Constructs a BrownDistortion with the given coefficients.
 *
 * @param coefficients The Brown distortion coefficients (radial and tangential).
 * @throws std::runtime_error if any coefficient is not finite.
 */
template <IsSpectral TSpectral>
BrownDistortion<TSpectral>::BrownDistortion(BrownCoefficients coefficients)
    : coefficients_(coefficients)
{
    this->check_coefficients_(
        {coefficients.k1, coefficients.k2, coefficients.k3, coefficients.p1, coefficients.p2},
        "BrownDistortion");
}

/**
 * @brief Computes the Brown distortion delta for a given coordinate.
 *
 * Calculates the radial and tangential distortion for the provided homogeneous coordinates.
 *
 * @tparam TFloat Floating point type for computation.
 * @param homogeneous_coords The input pixel coordinates (homogeneous).
 * @return The distortion delta to be applied.
 */
template <IsSpectral TSpectral>
template <IsFloatingPoint TFloat>
BasePixel<TFloat>
BrownDistortion<TSpectral>::compute_delta_(BasePixel<TFloat> homogeneous_coords) const
{
    const TFloat x = homogeneous_coords[0];
    const TFloat y = homogeneous_coords[1];
    const TFloat x2 = x * x;
    const TFloat y2 = y * y;
    const TFloat r2 = x2 + y2;
    const TFloat r4 = r2 * r2;
    const TFloat r6 = r4 * r2;

    // Radial distortion component
    const TFloat radial_factor = static_cast<TFloat>(coefficients_.k1) * r2 +
                                 static_cast<TFloat>(coefficients_.k2) * r4 +
                                 static_cast<TFloat>(coefficients_.k3) * r6;
    const BasePixel<TFloat> radial_distortion = radial_factor * homogeneous_coords;

    // Tangential distortion component
    const TFloat xy = x * y;
    const BasePixel<TFloat> tangential_distortion{
        TFloat{2} * static_cast<TFloat>(coefficients_.p1) * xy +
            static_cast<TFloat>(coefficients_.p2) * (r2 + TFloat{2} * x2),
        static_cast<TFloat>(coefficients_.p1) * (r2 + TFloat{2} * y2) +
            TFloat{2} * static_cast<TFloat>(coefficients_.p2) * xy};

    return radial_distortion + tangential_distortion;
}

/**
 * @brief Applies Brown distortion to the given pixel coordinates.
 *
 * Computes the distorted coordinates by adding the Brown distortion delta.
 *
 * @param homogeneous_coords The input pixel coordinates (homogeneous).
 * @return The distorted pixel coordinates.
 */
template <IsSpectral TSpectral>
Pixel BrownDistortion<TSpectral>::distort(Pixel homogeneous_coords) const
{
    return homogeneous_coords + compute_delta_<float>(homogeneous_coords);
}

/**
 * @brief Removes the Brown distortion from normalized image coordinates.
 *
 * Inverts distort() by Newton's method (see Distortion::undistort_newton_()).
 *
 * @param homogeneous_coords The distorted point, in normalized image coordinates.
 * @return The undistorted point, or NaN in both coordinates where the distortion has no inverse.
 */
template <IsSpectral TSpectral>
Pixel BrownDistortion<TSpectral>::undistort(Pixel homogeneous_coords) const
{
    return this->undistort_newton_(homogeneous_coords, [this](const Pixel_d& point) {
        return point + compute_delta_<double>(point);
    });
}

} // namespace huira
