#include <algorithm>
#include <cmath>

#include "huira/core/types.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {

    template <IsSpectral TSpectral>
    OpenCVDistortion<TSpectral>::OpenCVDistortion(OpenCVCoefficients coefficients)
        : coefficients_(coefficients) {
    }
    
    template <IsSpectral TSpectral>
    template <IsFloatingPoint TFloat>
    BasePixel<TFloat> OpenCVDistortion<TSpectral>::compute_delta_(BasePixel<TFloat> homogeneous_coords) const {
        const TFloat x = static_cast<TFloat>(homogeneous_coords[0]);
        const TFloat y = static_cast<TFloat>(homogeneous_coords[1]);
        BasePixel<TFloat> homogeneous_coords_tf{ x, y };

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
        const TFloat denominator = (std::abs(denominator_raw) < static_cast<TFloat>(kMinDenominator))
            ? std::copysign(static_cast<TFloat>(kMinDenominator), denominator_raw)
            : denominator_raw;

        const TFloat radial_factor = numerator / denominator;

        // Tangential and thin prism distortion components
        const TFloat xy = x * y;
        const BasePixel<TFloat> tangential_and_prism{
            TFloat{2} * static_cast<TFloat>(coefficients_.p1) * xy +
            static_cast<TFloat>(coefficients_.p2) * (r2 + TFloat{2} * x2) +
            static_cast<TFloat>(coefficients_.s1) * r2 +
            static_cast<TFloat>(coefficients_.s2) * r4,
            static_cast<TFloat>(coefficients_.p1) * (r2 + TFloat{2} * y2) +
            TFloat{2} * static_cast<TFloat>(coefficients_.p2) * xy +
            static_cast<TFloat>(coefficients_.s3) * r2 +
            static_cast<TFloat>(coefficients_.s4) * r4
        };

        return radial_factor * homogeneous_coords_tf + tangential_and_prism;
    }

    template <IsSpectral TSpectral>
    Pixel OpenCVDistortion<TSpectral>::distort(Pixel homogeneous_coords) const {
        return homogeneous_coords + compute_delta_<float>(homogeneous_coords);
    }

    template <IsSpectral TSpectral>
    Pixel OpenCVDistortion<TSpectral>::undistort(Pixel homogeneous_coords) const {
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
