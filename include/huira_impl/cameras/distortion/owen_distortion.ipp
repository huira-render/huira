#include <cmath>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {

    template <IsSpectral TSpectral>
    OwenDistortion<TSpectral>::OwenDistortion(OwenCoefficients coefficients)
        : coefficients_(coefficients) {
    }

    template <IsSpectral TSpectral>
    template <IsFloatingPoint TFloat>
    BasePixel<TFloat> OwenDistortion<TSpectral>::compute_delta_(BasePixel<TFloat> homogeneous_coords) const {
        const TFloat x = static_cast<TFloat>(homogeneous_coords[0]);
        const TFloat y = static_cast<TFloat>(homogeneous_coords[1]);
        BasePixel<TFloat> homogeneous_coords_tf{ x, y };

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
        const BasePixel<TFloat> rotated_coords{ -y, x };
        return radial_factor * homogeneous_coords_tf + rotated_factor * rotated_coords;
    }

    template <IsSpectral TSpectral>
    Pixel OwenDistortion<TSpectral>::distort(Pixel homogeneous_coords) const {
        return homogeneous_coords + compute_delta_<float>(homogeneous_coords);
    }

    template <IsSpectral TSpectral>
    Pixel OwenDistortion<TSpectral>::undistort(Pixel homogeneous_coords) const {
        BasePixel<double> homogeneous_coords_d{
            static_cast<double>(homogeneous_coords[0]),
            static_cast<double>(homogeneous_coords[1])
        };
        BasePixel<double> undistorted_coords_d = homogeneous_coords_d;

        for (std::size_t i = 0; i < this->max_iterations_; ++i) {
            const BasePixel<double> delta = compute_delta_<double>(undistorted_coords_d);
            const BasePixel<double> new_coords = homogeneous_coords_d - delta;

            // Check for convergence
            const BasePixel<double> diff = new_coords - undistorted_coords_d;
            const double error_sq = diff[0] * diff[0] + diff[1] * diff[1];

            undistorted_coords_d = new_coords;

            if (error_sq < this->tol_sq_) {
                break;
            }
        }

        Pixel undistorted_coords{
            static_cast<float>(undistorted_coords_d[0]),
            static_cast<float>(undistorted_coords_d[1])
        };

        return undistorted_coords;
    }

}
