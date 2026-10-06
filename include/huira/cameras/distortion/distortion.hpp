
#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/util/logger.hpp"

namespace huira {
/**
 * @brief Base class for distortion coefficient sets.
 *
 * Provides a polymorphic interface for all distortion coefficient types.
 */
struct DistortionCoefficients {
    DistortionCoefficients() = default;
    DistortionCoefficients(const DistortionCoefficients&) = default;
    DistortionCoefficients& operator=(const DistortionCoefficients&) = default;
    virtual ~DistortionCoefficients() = default;
};

/**
 * @brief Abstract base class for lens distortion models.
 *
 * Defines the interface for all distortion models, including distortion/undistortion and
 * coefficient access.
 *
 * Both work in normalized image coordinates. distort() maps an ideal (pinhole) point to where
 * the lens images it. undistort() inverts that, and returns NaN in both coordinates where there
 * is no inverse: beyond the furthest point the lens reaches, or where the model folds over (its
 * Jacobian determinant is not positive), since no ray images there.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class Distortion {
  public:
    Distortion() = default;
    virtual ~Distortion() = default;

    [[nodiscard]] virtual Pixel distort(Pixel homogeneous_coords) const = 0;
    [[nodiscard]] virtual Pixel undistort(Pixel homogeneous_coords) const = 0;

    virtual DistortionCoefficients* get_coefficients() = 0;
    [[nodiscard]] virtual const DistortionCoefficients* get_coefficients() const = 0;

    [[nodiscard]] virtual std::string get_type_name() const = 0;

    /// Set the most iterations undistort() takes; at least 1.
    void set_max_iterations(std::size_t max_iters)
    {
        if (max_iters < 1) {
            HUIRA_THROW_ERROR("Distortion::set_max_iterations - At least 1 iteration is needed");
        }
        max_iterations_ = max_iters;
    }
    [[nodiscard]] std::size_t get_max_iterations() const { return max_iterations_; }

    /// Set the convergence tolerance of undistort(): how close, in normalized image coordinates,
    /// the undistorted point must distort back to the one given. Positive and finite.
    void set_tolerance(float tol)
    {
        if (!(tol > 0.f) || !std::isfinite(tol)) {
            HUIRA_THROW_ERROR("Distortion::set_tolerance - Tolerance must be a positive finite "
                              "value: " +
                              std::to_string(tol));
        }
        tolerance_ = static_cast<double>(tol);
        tol_sq_ = tolerance_ * tolerance_;
    }
    [[nodiscard]] float get_tolerance() const { return static_cast<float>(tolerance_); }

  protected:
    /**
     * @brief Invert a distortion by Newton's method: find the point that distorts to target.
     *
     * Starts from the target itself, which is exact without distortion. Each step solves the
     * linearized distortion, with the Jacobian taken by forward differences, and is halved until
     * it brings the point closer; where the Jacobian is singular or folded it steps by the
     * residual instead, as a fixed-point iteration would.
     *
     * @param target The distorted point, in normalized image coordinates.
     * @param distort The distortion in double precision: a callable taking and returning
     *        Pixel_d.
     * @return The undistorted point, or NaN in both coordinates when there is none: the
     *         iteration did not converge to within the tolerance in max_iterations_ steps, or
     *         converged where the distortion folds over.
     */
    template <typename TDistort>
    [[nodiscard]] Pixel undistort_newton_(Pixel target, const TDistort& distort) const
    {
        constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
        const Pixel_d goal{static_cast<double>(target.x), static_cast<double>(target.y)};
        if (!std::isfinite(goal.x) || !std::isfinite(goal.y)) {
            return Pixel{NaN, NaN};
        }

        auto squared = [](const Pixel_d& p) { return p.x * p.x + p.y * p.y; };

        Pixel_d point = goal;
        Pixel_d distorted = distort(point);
        Pixel_d residual = distorted - goal;
        double residual_sq = squared(residual);

        for (std::size_t iteration = 0;; ++iteration) {
            // Jacobian columns, by forward differences:
            const double h = 1e-7 * std::max(1.0, std::abs(point.x) + std::abs(point.y));
            const Pixel_d d_dx = (distort(Pixel_d{point.x + h, point.y}) - distorted) * (1.0 / h);
            const Pixel_d d_dy = (distort(Pixel_d{point.x, point.y + h}) - distorted) * (1.0 / h);
            const double det = d_dx.x * d_dy.y - d_dy.x * d_dx.y;

            if (residual_sq <= tol_sq_) {
                // Converged. A point where the distortion folds over images the wrong way
                // round, so it is not where any ray lands: no inverse.
                return (det > 0.0) ? Pixel{static_cast<float>(point.x), static_cast<float>(point.y)}
                                   : Pixel{NaN, NaN};
            }
            if (iteration == max_iterations_) {
                return Pixel{NaN, NaN};
            }

            Pixel_d step = residual * -1.0;
            if (det > 0.0 && std::isfinite(det)) {
                step = Pixel_d{-(d_dy.y * residual.x - d_dy.x * residual.y) / det,
                               -(d_dx.x * residual.y - d_dx.y * residual.x) / det};
            }

            // Halve the step until it brings the point closer:
            bool improved = false;
            for (int halving = 0; halving < 40 && !improved; ++halving) {
                const Pixel_d candidate = point + step;
                const Pixel_d candidate_distorted = distort(candidate);
                const double candidate_sq = squared(candidate_distorted - goal);
                if (candidate_sq < residual_sq) {
                    point = candidate;
                    distorted = candidate_distorted;
                    residual = candidate_distorted - goal;
                    residual_sq = candidate_sq;
                    improved = true;
                }
                step = step * 0.5;
            }
            if (!improved) {
                return Pixel{NaN, NaN}; // stuck short of the target: no inverse
            }
        }
    }

    /// Throws if any coefficient is not finite.
    static void check_coefficients_(std::initializer_list<double> coefficients,
                                    const std::string& caller)
    {
        if (!std::all_of(coefficients.begin(), coefficients.end(), [](double c) {
                return std::isfinite(c);
            })) {
            HUIRA_THROW_ERROR(caller + " - Distortion coefficients must be finite");
        }
    }

    std::size_t max_iterations_ = 20;
    double tolerance_ = 1e-9; // about 1e-4 px even at a focal length of 1e5 px
    double tol_sq_ = tolerance_ * tolerance_;
};

template <typename T, typename TSpectral>
concept IsDistortion = std::derived_from<T, Distortion<TSpectral>>;

} // namespace huira
