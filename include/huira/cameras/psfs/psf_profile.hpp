#pragma once

#include <array>
#include <optional>

#include "huira/cameras/psfs/airy_band.hpp"

namespace huira::detail {

/**
 * @brief Harvey-Shack scattered light: the radial profile
 *
 *     S(r) = K (1 + (r / a)^2)^(-s / 2) (1 + (r / b)^2)^(-1)
 *
 * flat within the shoulder a, falling as r^-s beyond it, and as r^-(s + 2) beyond the outer
 * radius b, if one is given. K makes the light total 1. Without an outer radius the total is
 * finite only for s > 2.
 *
 * The outer radius bends the profile down smoothly, rather than cutting it off, so that it
 * leaves no edge in an image. Lengths are in any unit; light is per unit area in the same unit.
 */
class ScatterProfile {
  public:
    ScatterProfile(double shoulder, double slope, std::optional<double> outer);

    [[nodiscard]] double operator()(double r) const;
    [[nodiscard]] double slope(double r) const;
    [[nodiscard]] std::array<double, 3> derivatives(double r) const;

    /// K, the profile's value at r = 0.
    [[nodiscard]] double peak() const { return peak_; }

  private:
    double shoulder_squared_;
    double falloff_;
    std::optional<double> outer_squared_;
    double peak_ = 0.0;
};

/**
 * @brief The optics' radial profile in one spectral channel: the band-averaged diffraction
 * pattern, with a fraction of its light scattered.
 */
class OpticsProfile {
  public:
    OpticsProfile(const AiryBandProfile& diffraction,
                  double scatter_fraction,
                  std::optional<ScatterProfile> scatter);

    [[nodiscard]] double operator()(double r) const;
    [[nodiscard]] double slope(double r) const;

    [[nodiscard]] const AiryBandProfile& diffraction() const { return diffraction_; }
    [[nodiscard]] double scatter_fraction() const { return scatter_fraction_; }
    [[nodiscard]] const std::optional<ScatterProfile>& scatter() const { return scatter_; }

  private:
    AiryBandProfile diffraction_;
    double scatter_fraction_;
    std::optional<ScatterProfile> scatter_;
};

} // namespace huira::detail

#include "huira_impl/cameras/psfs/psf_profile.ipp"
