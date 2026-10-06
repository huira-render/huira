#pragma once

#include <array>
#include <vector>

namespace huira::detail {

std::array<double, 3> bessel_j012(double x);

double airy_shape(double x);
double airy_shape_slope(double x);

/**
 * @brief T(x), the integral of (J1(t) / t)^2 from x to infinity.
 *
 * The Airy pattern averaged over a band of wavelengths is a difference of two values of T (see
 * AiryBandProfile), so T is kept to full relative precision at every x: tabulated with its first
 * two derivatives every STEP up to END, summed downward from its asymptotic form there, and read
 * by quintic Hermite interpolation, whose error is below 3e-13. Beyond END the asymptotic form is
 * used. T falls from 4 / (3 pi) at x = 0 to about 1 / (2 pi x^2).
 *
 * The table is shared by every camera and built on first use (32,769 nodes, 0.8 MB).
 */
class AiryTail {
  public:
    static constexpr double STEP = 1.0 / 16.0;
    static constexpr double END = 2048.0;

    static const AiryTail& instance();

    [[nodiscard]] double operator()(double x) const;

    AiryTail(const AiryTail&) = delete;
    AiryTail& operator=(const AiryTail&) = delete;

  private:
    AiryTail();

    std::vector<double> tail_;
    std::vector<double> shape_;
    std::vector<double> slope_;
};

double airy_tail_asymptotic(double x);

/**
 * @brief The Airy pattern of a circular aperture, averaged evenly over the wavelengths of a band.
 *
 * The light per unit area at a distance r from the source, for a total of 1. Lengths are in any
 * unit, and the band is given by the cutoff frequencies of its two ends in cycles per that unit:
 * p / (lambda N) for a unit of p, at wavelength lambda and f-number N.
 *
 * At one wavelength the pattern is pi c^2 (J1(x) / x)^2 with x = pi r c. Averaged evenly over
 * wavelength, which goes as 1/c, it is pi c_blue c_red times the mean of (J1(x) / x)^2 over x
 * from pi r c_red to pi r c_blue: a difference of AiryTail values over the interval's width.
 * Equal cutoffs give a single wavelength.
 */
class AiryBandProfile {
  public:
    AiryBandProfile(double cutoff_blue, double cutoff_red);

    [[nodiscard]] double operator()(double r) const;
    [[nodiscard]] double slope(double r) const;

    /// The pattern's mean far out, averaged over its rings, is this over r^3.
    [[nodiscard]] double far_field_coefficient() const;

    [[nodiscard]] double cutoff_blue() const { return blue_; }
    [[nodiscard]] double cutoff_red() const { return red_; }

  private:
    double blue_;
    double red_;
};

} // namespace huira::detail

#include "huira_impl/cameras/psfs/airy_band.ipp"
