
#include <cmath>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

/**
 * @brief Constructs an OwenDistortion with the given coefficients.
 *
 * @param coefficients The Owen distortion coefficients.
 * @throws std::runtime_error if any coefficient is not finite.
 */
template <IsSpectral TSpectral>
OwenDistortion<TSpectral>::OwenDistortion(OwenCoefficients coefficients)
    : coefficients_(coefficients)
{
    const OwenCoefficients& c = coefficients;
    this->check_coefficients_({c.e1, c.e2, c.e3, c.e4, c.e5, c.e6}, "OwenDistortion");
}

/**
 * @brief Computes the Owen distortion delta for a given coordinate.
 *
 * Calculates the distortion for the provided homogeneous coordinates using the Owen model.
 *
 * @tparam TFloat Floating point type for computation.
 * @param homogeneous_coords The input pixel coordinates (homogeneous).
 * @return The distortion delta to be applied.
 */
template <IsSpectral TSpectral>
template <IsFloatingPoint TFloat>
BasePixel<TFloat>
OwenDistortion<TSpectral>::compute_delta_(BasePixel<TFloat> homogeneous_coords) const
{
    const TFloat x = static_cast<TFloat>(homogeneous_coords[0]);
    const TFloat y = static_cast<TFloat>(homogeneous_coords[1]);
    BasePixel<TFloat> homogeneous_coords_tf{x, y};

    const TFloat x2 = x * x;
    const TFloat y2 = y * y;
    const TFloat r2 = x2 + y2;
    const TFloat r = std::sqrt(r2);
    const TFloat r3 = r * r2;
    const TFloat r4 = r2 * r2;

    // Radial distortion factor for coordinate-aligned terms
    const TFloat radial_factor =
        static_cast<TFloat>(coefficients_.e2) * r2 + static_cast<TFloat>(coefficients_.e4) * r4 +
        static_cast<TFloat>(coefficients_.e5) * y + static_cast<TFloat>(coefficients_.e6) * x;

    // Radial distortion factor for rotated coordinate terms
    const TFloat rotated_factor =
        static_cast<TFloat>(coefficients_.e1) * r + static_cast<TFloat>(coefficients_.e3) * r3;

    // Apply distortion to original and 90-degree rotated coordinates
    const BasePixel<TFloat> rotated_coords{-y, x};
    return radial_factor * homogeneous_coords_tf + rotated_factor * rotated_coords;
}

/**
 * @brief Applies Owen distortion to the given pixel coordinates.
 *
 * Computes the distorted coordinates by adding the Owen distortion delta.
 *
 * @param homogeneous_coords The input pixel coordinates (homogeneous).
 * @return The distorted pixel coordinates.
 */
template <IsSpectral TSpectral>
Pixel OwenDistortion<TSpectral>::distort(Pixel homogeneous_coords) const
{
    return homogeneous_coords + compute_delta_<float>(homogeneous_coords);
}

/**
 * @brief Removes the Owen distortion from normalized image coordinates.
 *
 * Inverts distort() by Newton's method (see Distortion::undistort_newton_()).
 *
 * @param homogeneous_coords The distorted point, in normalized image coordinates.
 * @return The undistorted point, or NaN in both coordinates where the distortion has no inverse.
 */
template <IsSpectral TSpectral>
Pixel OwenDistortion<TSpectral>::undistort(Pixel homogeneous_coords) const
{
    return this->undistort_newton_(homogeneous_coords, [this](const Pixel_d& point) {
        return point + compute_delta_<double>(point);
    });
}

} // namespace huira
