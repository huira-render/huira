
#include <cmath>

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

    /**
     * @brief Constructs a BrownDistortion with the given coefficients.
     *
     * @param coefficients The Brown distortion coefficients (radial and tangential).
     */
    template <IsSpectral TSpectral>
    BrownDistortion<TSpectral>::BrownDistortion(BrownCoefficients coefficients)
        : coefficients_(coefficients) {
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
    BasePixel<TFloat> BrownDistortion<TSpectral>::compute_delta_(BasePixel<TFloat> homogeneous_coords) const {
        const TFloat x = homogeneous_coords[0];
        const TFloat y = homogeneous_coords[1];
        const TFloat x2 = x * x;
        const TFloat y2 = y * y;
        const TFloat r2 = x2 + y2;
        const TFloat r4 = r2 * r2;
        const TFloat r6 = r4 * r2;

        // Radial distortion component
        const TFloat radial_factor =
            static_cast<TFloat>(coefficients_.k1) * r2 +
            static_cast<TFloat>(coefficients_.k2) * r4 +
            static_cast<TFloat>(coefficients_.k3) * r6;
        const BasePixel<TFloat> radial_distortion = radial_factor * homogeneous_coords;

        // Tangential distortion component
        const TFloat xy = x * y;
        const BasePixel<TFloat> tangential_distortion{
            TFloat{2} * static_cast<TFloat>(coefficients_.p1) * xy +
            static_cast<TFloat>(coefficients_.p2) * (r2 + TFloat{2} * x2),
            static_cast<TFloat>(coefficients_.p1) * (r2 + TFloat{2} * y2) +
            TFloat{2} * static_cast<TFloat>(coefficients_.p2) * xy
        };

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
    Pixel BrownDistortion<TSpectral>::distort(Pixel homogeneous_coords) const {
        return homogeneous_coords + compute_delta_<float>(homogeneous_coords);
    }


    /**
     * @brief Removes Brown distortion from the given pixel coordinates.
     *
     * Iteratively computes the undistorted coordinates using the Brown model.
     *
     * @param homogeneous_coords The distorted pixel coordinates (homogeneous).
     * @return The undistorted pixel coordinates.
     */
    template <IsSpectral TSpectral>
    Pixel BrownDistortion<TSpectral>::undistort(Pixel homogeneous_coords) const {
        // TODO Consider using Newton-Raphson instead:
        // Partial derivatives of Radial Factor (dK/dr2) * (dr2/dx)
        // d(r2)/dx = 2x, d(r2)/dy = 2y
        // dK/d(r2) = k1 + 2*k2*r2 + 3*k3*r4
        // Derivatives of Tangential Component
        // x_tan = 2*p1*x*y + p2*(r2 + 2x^2)
        // y_tan = p1*(r2 + 2y^2) + 2*p2*x*y

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
