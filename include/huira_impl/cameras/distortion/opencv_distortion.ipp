
#include <algorithm>
#include <cmath>
#include <limits>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

/**
 * @brief Constructs an OpenCVDistortion with the given coefficients.
 *
 * @param coefficients The OpenCV distortion coefficients (radial, tangential, thin prism).
 * @throws std::runtime_error if any coefficient is not finite.
 */
template <IsSpectral TSpectral>
OpenCVDistortion<TSpectral>::OpenCVDistortion(OpenCVCoefficients coefficients)
    : coefficients_(coefficients)
{
    const OpenCVCoefficients& c = coefficients;
    this->check_coefficients_(
        {c.k1, c.k2, c.k3, c.k4, c.k5, c.k6, c.p1, c.p2, c.s1, c.s2, c.s3, c.s4},
        "OpenCVDistortion");
}

/**
 * @brief Computes the OpenCV distortion delta for a given coordinate.
 *
 * Calculates the rational radial, tangential, and thin prism distortion for the provided
 * homogeneous coordinates.
 *
 * @tparam TFloat Floating point type for computation.
 * @param homogeneous_coords The input pixel coordinates (homogeneous).
 * @return The distortion delta to be applied.
 */
template <IsSpectral TSpectral>
template <IsFloatingPoint TFloat>
BasePixel<TFloat>
OpenCVDistortion<TSpectral>::compute_delta_(BasePixel<TFloat> homogeneous_coords) const
{
    const TFloat x = static_cast<TFloat>(homogeneous_coords[0]);
    const TFloat y = static_cast<TFloat>(homogeneous_coords[1]);
    BasePixel<TFloat> homogeneous_coords_tf{x, y};

    const TFloat x2 = x * x;
    const TFloat y2 = y * y;
    const TFloat r2 = x2 + y2;
    const TFloat r4 = r2 * r2;
    const TFloat r6 = r4 * r2;

    // Rational radial distortion factor
    const TFloat numerator = TFloat{1} + static_cast<TFloat>(coefficients_.k1) * r2 +
                             static_cast<TFloat>(coefficients_.k2) * r4 +
                             static_cast<TFloat>(coefficients_.k3) * r6;
    const TFloat denominator_raw = TFloat{1} + static_cast<TFloat>(coefficients_.k4) * r2 +
                                   static_cast<TFloat>(coefficients_.k5) * r4 +
                                   static_cast<TFloat>(coefficients_.k6) * r6;

    // The denominator is 1 on the optical axis. Where it reaches 0 the model has a pole, beyond
    // which it maps points back into the image (the radial factor jumps from +infinity to
    // -infinity) although no ray from them lands there: the lens images nothing past it.
    // Comparisons are false for NaN, so a NaN denominator has no image either.
    if (!(denominator_raw > TFloat{0})) {
        const TFloat nan = std::numeric_limits<TFloat>::quiet_NaN();
        return BasePixel<TFloat>{nan, nan};
    }
    const TFloat denominator = std::max(denominator_raw, static_cast<TFloat>(kMinDenominator));

    const TFloat radial_factor = numerator / denominator;

    // Tangential and thin prism distortion components
    const TFloat xy = x * y;
    const BasePixel<TFloat> tangential_and_prism{
        TFloat{2} * static_cast<TFloat>(coefficients_.p1) * xy +
            static_cast<TFloat>(coefficients_.p2) * (r2 + TFloat{2} * x2) +
            static_cast<TFloat>(coefficients_.s1) * r2 + static_cast<TFloat>(coefficients_.s2) * r4,
        static_cast<TFloat>(coefficients_.p1) * (r2 + TFloat{2} * y2) +
            TFloat{2} * static_cast<TFloat>(coefficients_.p2) * xy +
            static_cast<TFloat>(coefficients_.s3) * r2 +
            static_cast<TFloat>(coefficients_.s4) * r4};

    // The radial factor scales the point, so the change it makes is (factor - 1) times it:
    return (radial_factor - TFloat{1}) * homogeneous_coords_tf + tangential_and_prism;
}

/**
 * @brief Applies OpenCV distortion to the given pixel coordinates.
 *
 * Computes the distorted coordinates by adding the OpenCV distortion delta.
 *
 * @param homogeneous_coords The input pixel coordinates (homogeneous).
 * @return The distorted pixel coordinates, or NaN in both where the rational radial factor's
 *         denominator is not positive: at or past its pole, where the lens images nothing.
 */
template <IsSpectral TSpectral>
Pixel OpenCVDistortion<TSpectral>::distort(Pixel homogeneous_coords) const
{
    return homogeneous_coords + compute_delta_<float>(homogeneous_coords);
}

/**
 * @brief Removes the OpenCV distortion from normalized image coordinates.
 *
 * Inverts distort() by Newton's method (see Distortion::undistort_newton_()).
 *
 * @param homogeneous_coords The distorted point, in normalized image coordinates.
 * @return The undistorted point, or NaN in both coordinates where the distortion has no inverse.
 */
template <IsSpectral TSpectral>
Pixel OpenCVDistortion<TSpectral>::undistort(Pixel homogeneous_coords) const
{
    return this->undistort_newton_(homogeneous_coords, [this](const Pixel_d& point) {
        return point + compute_delta_<double>(point);
    });
}

} // namespace huira
