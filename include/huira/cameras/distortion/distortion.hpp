
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
 * the lens images it. The lens images the region around the optical axis where the model is
 * one-to-one: out from the axis until the model folds over (its Jacobian determinant is no
 * longer positive) or, for a rational model, reaches a pole. undistort() inverts distort() on
 * that region, and returns NaN in both coordinates where there is no inverse: beyond the
 * furthest point the lens reaches, since no ray images there.
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

    /// Set the most iterations undistort() takes; at least 1. The default, 50, is more than twice
    /// what the hardest points of strongly distorted lenses take; most take 3 or 4.
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
     * The inverse is the point in the region the lens images: around the optical axis, out to
     * where the distortion folds over or reaches a pole (see Distortion). So the iteration
     * starts on the axis, (0, 0), and only takes steps that stay in that region. Each step
     * solves the linearized distortion, with the Jacobian taken by forward differences, and is
     * halved until the point it reaches:
     *  - is not folded over: the Jacobian determinant there is positive;
     *  - has not been carried back through the axis: its distorted point lies away from the
     *    axis's on the same side as the point itself;
     *  - is where the linearized distortion said it would be, to within half the step: a Newton
     *    step scaled by t leaves (1 - t) of the residual. A step that lands far from that has
     *    crossed a fold or a pole, or jumped onto another sheet of the model, where it could
     *    converge to a point that distorts to the target but is not in the image. This also
     *    makes every step bring the point closer;
     *  - and whose midpoint is unfolded and where the linearized distortion puts it, likewise:
     *    a step can otherwise jump right across a folded band onto a sheet beyond it that
     *    unfolds again (barrel distortion whose higher terms turn it back outwards), and land
     *    near the target there.
     *
     * Starting from the target, as before, failed for pincushion distortion that folds just
     * beyond the image: the image's edge distorts outwards past the fold, so the iteration
     * started folded over, with no step that brings the point closer, and returned NaN for a
     * point with an inverse. Its fallback, stepping by the residual where the Jacobian was
     * folded, could also cross the fold and converge on the wrong sheet without any sign.
     *
     * @param target The distorted point, in normalized image coordinates.
     * @param distort The distortion in double precision: a callable taking and returning
     *        Pixel_d. It may return NaN where the model has no image, e.g. past a pole.
     * @return The undistorted point, or NaN in both coordinates when there is none: no step
     *         gets closer to the target without leaving the region, or the iteration did not
     *         converge to within the tolerance in max_iterations_ steps.
     */
    template <typename TDistort>
    [[nodiscard]] Pixel undistort_newton_(Pixel target, const TDistort& distort) const
    {
        const Pixel no_inverse{std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::quiet_NaN()};
        const Pixel_d goal{static_cast<double>(target.x), static_cast<double>(target.y)};
        if (!std::isfinite(goal.x) || !std::isfinite(goal.y)) {
            return no_inverse;
        }

        auto squared = [](const Pixel_d& p) { return p.x * p.x + p.y * p.y; };
        auto dot = [](const Pixel_d& a, const Pixel_d& b) { return a.x * b.x + a.y * b.y; };

        // Jacobian columns, by forward differences, given the distortion at the point:
        struct Jacobian {
            Pixel_d d_dx;
            Pixel_d d_dy;
            double det = 0.0;
        };
        auto jacobian = [&distort](const Pixel_d& point, const Pixel_d& distorted) {
            const double h = 1e-7 * std::max(1.0, std::abs(point.x) + std::abs(point.y));
            Jacobian j;
            j.d_dx = (distort(Pixel_d{point.x + h, point.y}) - distorted) * (1.0 / h);
            j.d_dy = (distort(Pixel_d{point.x, point.y + h}) - distorted) * (1.0 / h);
            j.det = j.d_dx.x * j.d_dy.y - j.d_dy.x * j.d_dx.y;
            return j;
        };
        // Comparisons are false for NaN, so a NaN determinant counts as folded:
        auto unfolded = [](const Jacobian& j) { return j.det > 0.0 && std::isfinite(j.det); };

        // Start on the optical axis, which every lens images:
        Pixel_d point{0.0, 0.0};
        const Pixel_d axis = distort(point);
        Pixel_d residual = axis - goal;
        double residual_sq = squared(residual);
        Jacobian jac = jacobian(point, axis);
        if (!std::isfinite(residual_sq) || !unfolded(jac)) {
            return no_inverse;
        }

        for (std::size_t iteration = 0;; ++iteration) {
            if (residual_sq <= tol_sq_) {
                // Every point the iteration reaches is in the region the lens images:
                return Pixel{static_cast<float>(point.x), static_cast<float>(point.y)};
            }
            if (iteration == max_iterations_) {
                return no_inverse;
            }

            const Pixel_d step{-(jac.d_dy.y * residual.x - jac.d_dy.x * residual.y) / jac.det,
                               -(jac.d_dx.x * residual.y - jac.d_dx.y * residual.x) / jac.det};

            // Halve the step until it stays in the region and does what the linearized
            // distortion predicts:
            bool moved = false;
            double t = 1.0;
            for (int halving = 0; halving < 40 && !moved; ++halving, t *= 0.5) {
                const Pixel_d candidate = point + step * t;
                const Pixel_d distorted = distort(candidate);
                const Pixel_d candidate_residual = distorted - goal;
                const Pixel_d miss = candidate_residual - residual * (1.0 - t);
                if (!(squared(miss) <= 0.25 * t * t * residual_sq)) {
                    continue;
                }
                if (!(dot(distorted - axis, candidate) > 0.0)) {
                    continue;
                }
                const Jacobian candidate_jac = jacobian(candidate, distorted);
                if (!unfolded(candidate_jac)) {
                    continue;
                }
                // The checks above are at the point the step reaches. A step can still jump
                // across a folded band onto a sheet beyond it that unfolds again, and land near
                // the target there: so the step's midpoint must be unfolded and where the
                // linearized distortion puts it too.
                const Pixel_d midpoint = point + step * (0.5 * t);
                const Pixel_d mid_distorted = distort(midpoint);
                const Pixel_d mid_miss = (mid_distorted - goal) - residual * (1.0 - 0.5 * t);
                if (!(squared(mid_miss) <= 0.0625 * t * t * residual_sq) ||
                    !unfolded(jacobian(midpoint, mid_distorted))) {
                    continue;
                }
                point = candidate;
                residual = candidate_residual;
                residual_sq = squared(candidate_residual);
                jac = candidate_jac;
                moved = true;
            }
            if (!moved) {
                return no_inverse;
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

    std::size_t max_iterations_ = 50;
    double tolerance_ = 1e-9; // about 1e-4 px even at a focal length of 1e5 px
    double tol_sq_ = tolerance_ * tolerance_;
};

template <typename T, typename TSpectral>
concept IsDistortion = std::derived_from<T, Distortion<TSpectral>>;

} // namespace huira
