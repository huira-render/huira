#include <cmath>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    OwenDistortion<TSpectral, TFloat>::OwenDistortion(OwenCoefficients coefficients)
        : coefficients_(coefficients) {
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Pixel OwenDistortion<TSpectral, TFloat>::compute_delta(Pixel homogeneous_coords) const {
        const TFloat x = static_cast<TFloat>(homogeneous_coords[0]);
        const TFloat y = static_cast<TFloat>(homogeneous_coords[1]);

        const TFloat x2 = x * x;
        const TFloat y2 = y * y;
        const TFloat r2 = x2 + y2;
        const TFloat r = std::sqrt(r2);
        const TFloat r3 = r * r2;
        const TFloat r4 = r2 * r2;

        // Radial distortion factor for coordinate-aligned terms
        const TFloat radial_factor =
            static_cast<TFloat>(coefficients_.e2) * r2 +
            static_cast<TFloat>(coefficients_.e4) * r4 +
            static_cast<TFloat>(coefficients_.e5) * y +
            static_cast<TFloat>(coefficients_.e6) * x;

        // Radial distortion factor for rotated coordinate terms
        const TFloat rotated_factor =
            static_cast<TFloat>(coefficients_.e1) * r +
            static_cast<TFloat>(coefficients_.e3) * r3;

        // Apply distortion to original and 90-degree rotated coordinates
        const Pixel rotated_coords{ -y, x };
        return radial_factor * homogeneous_coords + rotated_factor * rotated_coords;
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Pixel OwenDistortion<TSpectral, TFloat>::distort(Pixel homogeneous_coords) const {
        return homogeneous_coords + compute_delta(homogeneous_coords);
    }

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    Pixel OwenDistortion<TSpectral, TFloat>::undistort(Pixel homogeneous_coords) const {
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
