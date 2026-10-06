#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "huira/cameras/psfs/airy_band.hpp"

namespace huira::detail {

/**
 * @brief A radial function tabulated with its first two derivatives at nodes from start to end,
 * read by quintic Hermite interpolation.
 *
 * The nodes are evenly spaced in r, or in log r for a smooth function over a wide range (a far
 * field). Beyond the last node the function is continued as the power law it follows there.
 */
class RadialTable {
  public:
    RadialTable() = default;

    template <class Function>
    static RadialTable linear(const Function& function, double start, double end, double step);
    template <class Function>
    static RadialTable
    logarithmic(const Function& function, double start, double end, double ratio);

    [[nodiscard]] std::array<double, 3> operator()(double r) const;

    [[nodiscard]] double start() const { return start_; }
    [[nodiscard]] double end() const { return end_; }
    [[nodiscard]] std::size_t memory_bytes() const { return nodes_.size() * sizeof(nodes_[0]); }

  private:
    bool logarithmic_ = false;
    double start_ = 0.0;
    double end_ = 0.0;
    double spacing_ = 1.0;
    std::vector<double> radii_;
    std::vector<std::array<double, 3>> nodes_;

    [[nodiscard]] std::array<double, 3> tail_(double r) const;
};

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
    [[nodiscard]] std::array<double, 3> far_field(double r) const;

    [[nodiscard]] const AiryBandProfile& diffraction() const { return diffraction_; }
    [[nodiscard]] double scatter_fraction() const { return scatter_fraction_; }
    [[nodiscard]] const std::optional<ScatterProfile>& scatter() const { return scatter_; }

  private:
    AiryBandProfile diffraction_;
    double scatter_fraction_;
    std::optional<ScatterProfile> scatter_;
};

/**
 * @brief The optics' profile blurred by defocus: convolved with a uniform disc of radius b, the
 * image of the aperture out of focus.
 *
 * The value at a distance r from the source is the light the in-focus profile puts inside a disc
 * of radius b centred r from it, over the disc's area: the integral over the in-focus profile's
 * circles of their length inside the disc. The first two derivatives with r come the same way,
 * from the profile's derivatives. They are tabulated every eighth of the finest period of the
 * diffraction pattern, and read by quintic Hermite interpolation.
 *
 * The in-focus profile's rings are followed out to ring_radius(); beyond it they are averaged
 * over, as in the far field. Averaged over the disc, the rings lose contrast as
 * (pi c b)^(-3/2), so the larger the blur, the closer in that can be. Beyond the disc's edge
 * plus ring_radius() the blurred profile is smooth, and is tabulated on a logarithmic grid.
 */
class DefocusedProfile {
  public:
    DefocusedProfile(const OpticsProfile& focused,
                     double blur,
                     double focused_far_radius,
                     double reach);

    [[nodiscard]] double operator()(double r) const;
    [[nodiscard]] double slope(double r) const;
    [[nodiscard]] std::array<double, 3> far_field(double r) const;

    /// The radius of the blur disc.
    [[nodiscard]] double blur() const { return blur_; }

    /// How far out the in-focus profile's rings are followed.
    [[nodiscard]] double ring_radius() const { return ring_radius_; }

  private:
    double blur_;
    double ring_radius_ = 0.0;
    RadialTable near_;
    RadialTable far_;
};

} // namespace huira::detail

#include "huira_impl/cameras/psfs/psf_profile.ipp"
