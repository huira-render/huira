
#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <initializer_list>
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

    /// Set the convergence tolerance of undistort(); positive and finite.
    void set_tolerance(float tol)
    {
        if (!(tol > 0.f) || !std::isfinite(tol)) {
            HUIRA_THROW_ERROR("Distortion::set_tolerance - Tolerance must be a positive finite "
                              "value: " +
                              std::to_string(tol));
        }
        tolerance_ = tol;
        tol_sq_ = tolerance_ * tolerance_;
    }
    [[nodiscard]] float get_tolerance() const { return tolerance_; }

  protected:
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
    double tol_sq_ = 1e-12;
    double tolerance_ = 1e-6;
};

template <typename T, typename TSpectral>
concept IsDistortion = std::derived_from<T, Distortion<TSpectral>>;

} // namespace huira
