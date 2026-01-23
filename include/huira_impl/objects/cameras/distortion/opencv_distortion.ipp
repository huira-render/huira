#include <algorithm>
#include <cmath>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    OpenCVDistortion<TSpectral, TFloat>::OpenCVDistortion(OpenCVCoefficients coefficients)
        : coefficients_(coefficients) {
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Pixel OpenCVDistortion<TSpectral, TFloat>::compute_delta(Pixel homogeneous_coords) const {
        // Delta is defined as: distort(x) - x
        // This is consistent with the fixed-point iteration in undistort()
        return distort(homogeneous_coords) - homogeneous_coords;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Pixel OpenCVDistortion<TSpectral, TFloat>::distort(Pixel homogeneous_coords) const {
        const TFloat x = static_cast<TFloat>(homogeneous_coords[0]);
        const TFloat y = static_cast<TFloat>(homogeneous_coords[1]);

        const TFloat x2 = x * x;
        const TFloat y2 = y * y;
        const TFloat r2 = x2 + y2;
        const TFloat r4 = r2 * r2;
        const TFloat r6 = r4 * r2;

        // Rational radial distortion factor
        const TFloat numerator = TFloat{ 1 } +
            static_cast<TFloat>(coefficients_.k1) * r2 +
            static_cast<TFloat>(coefficients_.k2) * r4 +
            static_cast<TFloat>(coefficients_.k3) * r6;
        const TFloat denominator_raw = TFloat{ 1 } +
            static_cast<TFloat>(coefficients_.k4) * r2 +
            static_cast<TFloat>(coefficients_.k5) * r4 +
            static_cast<TFloat>(coefficients_.k6) * r6;

        // Prevent division by zero with sign-preserving clamping
        const TFloat denominator = (std::abs(denominator_raw) < kMinDenominator)
            ? std::copysign(kMinDenominator, denominator_raw)
            : denominator_raw;

        const TFloat radial_factor = numerator / denominator;

        // Tangential and thin prism distortion components
        const TFloat xy = x * y;
        const Pixel tangential_and_prism{
            TFloat{2} * static_cast<TFloat>(coefficients_.p1) * xy +
            static_cast<TFloat>(coefficients_.p2) * (r2 + TFloat{2} * x2) +
            static_cast<TFloat>(coefficients_.s1) * r2 +
            static_cast<TFloat>(coefficients_.s2) * r4,
            static_cast<TFloat>(coefficients_.p1) * (r2 + TFloat{2} * y2) +
            TFloat{2} * static_cast<TFloat>(coefficients_.p2) * xy +
            static_cast<TFloat>(coefficients_.s3) * r2 +
            static_cast<TFloat>(coefficients_.s4) * r4
        };

        return radial_factor * homogeneous_coords + tangential_and_prism;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Pixel OpenCVDistortion<TSpectral, TFloat>::undistort(Pixel homogeneous_coords) const {
        Pixel undistorted_coords = homogeneous_coords;

        for (std::size_t i = 0; i < this->max_iterations_; ++i) {
            const Pixel delta = compute_delta(undistorted_coords);
            const Pixel new_coords = homogeneous_coords - delta;

            // Check for convergence
            const Pixel diff = new_coords - undistorted_coords;
            const TFloat error_sq = static_cast<TFloat>(diff[0] * diff[0] + diff[1] * diff[1]);

            undistorted_coords = new_coords;

            if (error_sq < this->tol_sq_) {
                break;
            }
        }

        return undistorted_coords;
    }

}
