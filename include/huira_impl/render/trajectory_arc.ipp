#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>
#include <stdexcept>

#include "huira/core/constants.hpp"

namespace huira {
/**
 * @brief Construct a trajectory arc from N direction samples.
 *
 * Samples are assumed to be at uniformly spaced parameter values
 * over [0, 1]: t_i = i / (N - 1) for i = 0, ..., N-1.
 *
 * @param samples Direction vectors at each sample time. Must have at least 2 elements.
 */
TrajectoryArc::TrajectoryArc(const std::vector<Vec3<float>>& samples)
    : sample_count_(samples.size())
{
    if (samples.size() < 1) {
        HUIRA_THROW_ERROR("TrajectoryArc::TrajectoryArc - requires at least 1 sample point.");
    }

    if (is_polynomial_()) {
        build_polynomial_(samples);
    } else {
        build_cubic_spline_(samples);
    }
}

/**
 * @brief Evaluate the trajectory at parameter t.
 *
 * @param t Parameter value in [0, 1].
 * @return Interpolated direction vector.
 */
Vec3<float> TrajectoryArc::evaluate(float t) const
{
    if (is_polynomial_()) {
        return evaluate_polynomial_(t);
    }
    return evaluate_spline_(t);
}

/**
 * @brief Find all parameter values where the trajectory crosses a plane.
 *
 * Finds all t in [0, 1] where dot(plane_normal, curve(t)) = 0.
 * Used for frustum clipping.
 *
 * @param plane_normal Inward-pointing normal of a plane through the origin.
 * @return Sorted list of parameter values where the crossing occurs.
 */
std::vector<float> TrajectoryArc::find_plane_crossings(const Vec3<float>& plane_normal) const
{
    if (is_polynomial_()) {
        return find_crossings_polynomial_(plane_normal);
    }
    return find_crossings_spline_(plane_normal);
}

/**
 * @brief Build polynomial coefficients from 2 or 3 sample points.
 *
 * For N=2 (linear): f(t) = p0 + (p1 - p0) * t
 * For N=3 (quadratic): Lagrange interpolation at t = 0, 0.5, 1
 *   f(t) = p0(1 - 3t + 2t^2) + p1(4t - 4t^2) + p2(-t + 2t^2)
 *   Coefficients: a0 = p0, a1 = -3*p0 + 4*p1 - p2, a2 = 2*p0 - 4*p1 + 2*p2
 */
void TrajectoryArc::build_polynomial_(const std::vector<Vec3<float>>& samples)
{
    if (samples.size() == 1) {
        // Constant: f(t) = p0
        poly_coeffs_.resize(1);
        poly_coeffs_[0] = samples[0];
    } else if (samples.size() == 2) {
        // Linear: f(t) = p0 + (p1 - p0) * t
        poly_coeffs_.resize(2);
        poly_coeffs_[0] = samples[0];
        poly_coeffs_[1] = samples[1] - samples[0];
    } else {
        // Quadratic: Lagrange at t = 0, 0.5, 1
        const auto& p0 = samples[0];
        const auto& p1 = samples[1];
        const auto& p2 = samples[2];

        poly_coeffs_.resize(3);
        poly_coeffs_[0] = p0;
        poly_coeffs_[1] = -3.0f * p0 + 4.0f * p1 - p2;
        poly_coeffs_[2] = 2.0f * p0 - 4.0f * p1 + 2.0f * p2;
    }
}

/**
 * @brief Build a natural cubic spline through N uniformly spaced samples.
 *
 * Uses the standard tridiagonal algorithm for natural cubic splines
 * (second derivative = 0 at both endpoints). Each component (x, y, z)
 * is solved independently.
 *
 * For N samples there are N-1 segments. Each segment i is parameterized as:
 *   S_i(u) = a_i + b_i*u + c_i*u^2 + d_i*u^3
 * where u = t - knots_[i].
 */
void TrajectoryArc::build_cubic_spline_(const std::vector<Vec3<float>>& samples)
{
    const std::size_t n = samples.size();
    const std::size_t num_segments = n - 1;

    // Uniform knot spacing:
    knots_.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        knots_[i] = static_cast<float>(i) / static_cast<float>(num_segments);
    }

    const float h = knots_[1] - knots_[0];

    segments_.resize(num_segments);

    // Solve for each component independently:
    for (int comp = 0; comp < 3; ++comp) {
        std::vector<float> y(n);
        for (std::size_t i = 0; i < n; ++i) {
            y[i] = samples[i][comp];
        }

        // Solve for second derivatives (natural spline: M[0] = M[n-1] = 0)
        const std::size_t interior = n - 2;
        std::vector<float> M(n, 0.0f);

        if (interior > 0) {
            std::vector<float> rhs(interior);
            for (std::size_t i = 0; i < interior; ++i) {
                rhs[i] = (6.0f / h) * (y[i + 2] - 2.0f * y[i + 1] + y[i]);
            }

            std::vector<float> diag(interior, 4.0f * h);
            std::vector<float> upper(interior, h);

            for (std::size_t i = 1; i < interior; ++i) {
                float factor = h / diag[i - 1];
                diag[i] -= factor * upper[i - 1];
                rhs[i] -= factor * rhs[i - 1];
            }

            std::vector<float> interior_M(interior);
            interior_M[interior - 1] = rhs[interior - 1] / diag[interior - 1];
            for (std::size_t i = interior - 1; i > 0; --i) {
                interior_M[i - 1] = (rhs[i - 1] - upper[i - 1] * interior_M[i]) / diag[i - 1];
            }

            for (std::size_t i = 0; i < interior; ++i) {
                M[i + 1] = interior_M[i];
            }
        }

        for (std::size_t i = 0; i < num_segments; ++i) {
            segments_[i].a[comp] = y[i];
            segments_[i].b[comp] = (y[i + 1] - y[i]) / h - h * (2.0f * M[i] + M[i + 1]) / 6.0f;
            segments_[i].c[comp] = M[i] / 2.0f;
            segments_[i].d[comp] = (M[i + 1] - M[i]) / (6.0f * h);
        }
    }
}

Vec3<float> TrajectoryArc::evaluate_polynomial_(float t) const
{
    // Horner's method:
    Vec3<float> result = poly_coeffs_.back();
    for (std::size_t i = poly_coeffs_.size() - 1; i > 0; --i) {
        result = result * t + poly_coeffs_[i - 1];
    }
    return result;
}

Vec3<float> TrajectoryArc::evaluate_spline_(float t) const
{
    t = std::clamp(t, 0.0f, 1.0f);

    std::size_t seg = 0;
    for (std::size_t i = 0; i < segments_.size() - 1; ++i) {
        if (t >= knots_[i + 1]) {
            seg = i + 1;
        } else {
            break;
        }
    }
    seg = std::min(seg, segments_.size() - 1);

    const float u = t - knots_[seg];
    const auto& s = segments_[seg];
    return s.a + u * (s.b + u * (s.c + u * s.d));
}

/**
 * @brief Find crossings for polynomial representation.
 *
 * dot(normal, f(t)) = 0 becomes a scalar polynomial in t.
 */
std::vector<float> TrajectoryArc::find_crossings_polynomial_(const Vec3<float>& normal) const
{
    if (poly_coeffs_.size() == 1) {
        // Constant: dot(normal, p0) is either >= 0 or < 0. No crossings.
        return {};
    } else if (poly_coeffs_.size() == 2) {
        float c0 = glm::dot(normal, poly_coeffs_[0]);
        float c1 = glm::dot(normal, poly_coeffs_[1]);
        return solve_linear_(c1, c0, 0.0f, 1.0f);
    } else {
        float c0 = glm::dot(normal, poly_coeffs_[0]);
        float c1 = glm::dot(normal, poly_coeffs_[1]);
        float c2 = glm::dot(normal, poly_coeffs_[2]);
        return solve_quadratic_(c2, c1, c0, 0.0f, 1.0f);
    }
}

/**
 * @brief Find crossings for cubic spline representation.
 *
 * For each segment, dot(normal, S_i(u)) = 0 is a cubic in u.
 */
std::vector<float> TrajectoryArc::find_crossings_spline_(const Vec3<float>& normal) const
{
    std::vector<float> all_roots;

    for (std::size_t i = 0; i < segments_.size(); ++i) {
        const auto& s = segments_[i];
        const float h = knots_[i + 1] - knots_[i];

        float a = glm::dot(normal, s.d);
        float b = glm::dot(normal, s.c);
        float c = glm::dot(normal, s.b);
        float d = glm::dot(normal, s.a);

        auto roots = solve_cubic_(a, b, c, d, 0.0f, h);

        for (float u : roots) {
            float t = knots_[i] + u;
            if (all_roots.empty() || std::abs(t - all_roots.back()) > 1e-6f) {
                all_roots.push_back(t);
            }
        }
    }

    std::sort(all_roots.begin(), all_roots.end());
    return all_roots;
}

// =========================================================================
// Root-finding utilities
// =========================================================================

std::vector<float> TrajectoryArc::solve_linear_(float a, float b, float t_min, float t_max)
{
    constexpr float eps = 1e-10f;
    if (std::abs(a) < eps) {
        return {};
    }
    float t = -b / a;
    if (t >= t_min - eps && t <= t_max + eps) {
        return {std::clamp(t, t_min, t_max)};
    }
    return {};
}

std::vector<float>
TrajectoryArc::solve_quadratic_(float a, float b, float c, float t_min, float t_max)
{
    constexpr float eps = 1e-10f;

    if (std::abs(a) < eps) {
        return solve_linear_(b, c, t_min, t_max);
    }

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return {};
    }

    std::vector<float> roots;
    if (discriminant < eps) {
        float t = -b / (2.0f * a);
        if (t >= t_min - eps && t <= t_max + eps) {
            roots.push_back(std::clamp(t, t_min, t_max));
        }
    } else {
        float sqrt_d = std::sqrt(discriminant);
        float q = -0.5f * (b + std::copysign(sqrt_d, b));
        float t1 = q / a;
        float t2 = c / q;

        if (t1 > t2) {
            std::swap(t1, t2);
        }

        if (t1 >= t_min - eps && t1 <= t_max + eps) {
            roots.push_back(std::clamp(t1, t_min, t_max));
        }
        if (t2 >= t_min - eps && t2 <= t_max + eps) {
            if (roots.empty() || std::abs(t2 - roots.back()) > eps) {
                roots.push_back(std::clamp(t2, t_min, t_max));
            }
        }
    }
    return roots;
}

/**
 * @brief Solve a*t^3 + b*t^2 + c*t + d = 0 for roots in [t_min, t_max].
 *
 * Uses the depressed cubic (Cardano's method) for analytic solution.
 */
std::vector<float>
TrajectoryArc::solve_cubic_(float a, float b, float c, float d, float t_min, float t_max)
{
    constexpr float eps = 1e-10f;

    if (std::abs(a) < eps) {
        return solve_quadratic_(b, c, d, t_min, t_max);
    }

    float p = b / a;
    float q = c / a;
    float r = d / a;

    float p2 = p * p;
    float A = q - p2 / 3.0f;
    float B = r - p * q / 3.0f + 2.0f * p2 * p / 27.0f;

    float discriminant = -4.0f * A * A * A - 27.0f * B * B;

    std::vector<float> roots;
    float offset = -p / 3.0f;

    if (discriminant > eps) {
        float m = 2.0f * std::sqrt(-A / 3.0f);
        float theta = std::acos(3.0f * B / (A * m)) / 3.0f;

        for (int k = 0; k < 3; ++k) {
            float u = m * std::cos(theta - 2.0f * PI<float>() * static_cast<float>(k) / 3.0f);
            float t = u + offset;
            if (t >= t_min - eps && t <= t_max + eps) {
                roots.push_back(std::clamp(t, t_min, t_max));
            }
        }
    } else if (discriminant < -eps) {
        float sqrtQ3 = std::sqrt(-A / 3.0f);
        float cosh_val = std::abs(B) / (sqrtQ3 * sqrtQ3 * sqrtQ3) * 1.5f;

        float u;
        if (A < 0.0f) {
            float acosh_val = std::log(cosh_val + std::sqrt(cosh_val * cosh_val - 1.0f));
            u = -2.0f * std::copysign(sqrtQ3, B) * std::cosh(acosh_val / 3.0f);
        } else {
            float asinh_val = std::log(cosh_val + std::sqrt(cosh_val * cosh_val + 1.0f));
            u = -2.0f * std::copysign(std::sqrt(A / 3.0f), B) * std::sinh(asinh_val / 3.0f);
        }

        float t = u + offset;
        if (t >= t_min - eps && t <= t_max + eps) {
            roots.push_back(std::clamp(t, t_min, t_max));
        }
    } else {
        if (std::abs(B) < eps) {
            float t = offset;
            if (t >= t_min - eps && t <= t_max + eps) {
                roots.push_back(std::clamp(t, t_min, t_max));
            }
        } else {
            float u1 = 3.0f * B / A;
            float u2 = -u1 / 2.0f;

            float t1 = u1 + offset;
            float t2 = u2 + offset;

            if (t1 >= t_min - eps && t1 <= t_max + eps) {
                roots.push_back(std::clamp(t1, t_min, t_max));
            }
            if (t2 >= t_min - eps && t2 <= t_max + eps) {
                if (roots.empty() || std::abs(t2 - roots.back()) > eps) {
                    roots.push_back(std::clamp(t2, t_min, t_max));
                }
            }
        }
    }

    std::sort(roots.begin(), roots.end());
    return roots;
}

} // namespace huira
