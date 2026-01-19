#include <cmath>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    BrownDistortion<TSpectral, TFloat>::BrownDistortion(BrownCoefficients<TFloat> coefficients)
        : coefficients_(coefficients) {
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Pixel BrownDistortion<TSpectral, TFloat>::compute_delta(Pixel homogeneous_coords) const {
        const TFloat x = static_cast<TFloat>(homogeneous_coords[0]);
        const TFloat y = static_cast<TFloat>(homogeneous_coords[1]);

        const TFloat x2 = x * x;
        const TFloat y2 = y * y;
        const TFloat r2 = x2 + y2;
        const TFloat r4 = r2 * r2;
        const TFloat r6 = r4 * r2;

        // Radial distortion component
        const TFloat radial_factor = coefficients_.k1 * r2 +
            coefficients_.k2 * r4 +
            coefficients_.k3 * r6;
        const Pixel radial_distortion = radial_factor * homogeneous_coords;

        // Tangential distortion component
        const TFloat xy = x * y;
        const Pixel tangential_distortion{
            TFloat{2} * coefficients_.p1 * xy + coefficients_.p2 * (r2 + TFloat{2} * x2),
            coefficients_.p1 * (r2 + TFloat{2} * y2) + TFloat{2} * coefficients_.p2 * xy
        };

        return radial_distortion + tangential_distortion;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Pixel BrownDistortion<TSpectral, TFloat>::distort(Pixel homogeneous_coords) const {
        return homogeneous_coords + compute_delta(homogeneous_coords);
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Pixel BrownDistortion<TSpectral, TFloat>::undistort(Pixel homogeneous_coords) const {
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
