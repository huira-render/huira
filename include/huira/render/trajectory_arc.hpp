#pragma once

#include <array>
#include <utility>
#include <vector>

#include "huira/core/types.hpp"

namespace huira {
/**
 * @brief Parametric curve through a sequence of direction samples over [0, 1].
 *
 * TrajectoryArc represents the apparent trajectory of a point (e.g., a star)
 * across the sky during an exposure interval. It is constructed from N direction
 * vector samples at uniformly spaced parameter values over [0, 1].
 *
 * For N <= 3, a single polynomial of degree N-1 is used (linear or quadratic).
 * For N > 3, a natural cubic spline is used to avoid the oscillation problems
 * of high-degree polynomial interpolation.
 *
 * The class supports evaluation at arbitrary parameter values and finding
 * parameter values where the trajectory crosses a plane through the origin
 * (used for frustum clipping).
 */
class TrajectoryArc {
  public:
    TrajectoryArc() = default;
    explicit TrajectoryArc(const std::vector<Vec3<float>>& samples);

    void reset(const std::vector<Vec3<float>>& samples);

    [[nodiscard]] Vec3<float> evaluate(float t) const;

    [[nodiscard]] std::vector<float> find_plane_crossings(const Vec3<float>& plane_normal) const;

    void find_plane_crossings(const Vec3<float>& plane_normal, std::vector<float>& out) const;

    /// Number of sample points used to construct this arc.
    [[nodiscard]] std::size_t sample_count() const noexcept { return sample_count_; }

  private:
    std::size_t sample_count_ = 0;

    // Polynomial representation (N <= 3)
    std::array<Vec3<float>, 3> poly_coeffs_{};
    std::size_t poly_count_ = 0;

    // Cubic spline representation (N > 3)
    struct SplineSegment {
        Vec3<float> a, b, c, d;
    };
    std::vector<SplineSegment> segments_;
    std::vector<float> knots_;

    bool is_polynomial_() const noexcept { return sample_count_ <= 3; }

    void build_polynomial_(const std::vector<Vec3<float>>& samples);
    void build_cubic_spline_(const std::vector<Vec3<float>>& samples);

    [[nodiscard]] Vec3<float> evaluate_polynomial_(float t) const;
    [[nodiscard]] Vec3<float> evaluate_spline_(float t) const;

    void find_crossings_polynomial_(const Vec3<float>& normal, std::vector<float>& out) const;
    void find_crossings_spline_(const Vec3<float>& normal, std::vector<float>& out) const;

    // Root-finding utilities
    static std::size_t solve_linear_(float a, float b, float t_min, float t_max, float* out);
    static std::size_t
    solve_quadratic_(float a, float b, float c, float t_min, float t_max, float* out);
    static std::size_t
    solve_cubic_(float a, float b, float c, float d, float t_min, float t_max, float* out);
};

} // namespace huira

#include "huira_impl/render/trajectory_arc.ipp"
