#pragma once

#include <array>
#include <vector>

namespace huira::detail {

std::array<double, 2> struve_h01(double x);

/**
 * @brief V(x) = W(x) + 2 ln(x) / (3 pi), with W(x) the integral of H1(t) / t^3 from x to
 * infinity, H1 Struve's function.
 *
 * The line spread of the Airy pattern averaged over a band of wavelengths is a difference of two
 * values of W (see AiryBandLine), as the pattern is of two values of AiryTail. W grows as
 * -2 ln(x) / (3 pi) towards 0, which V takes out, so that V is smooth and finite there. V is
 * tabulated with its first two derivatives every STEP up to END, summed downward from W's
 * asymptotic form there, and read by quintic Hermite interpolation; beyond END the asymptotic
 * form is used.
 *
 * The table is shared by every camera and built on first use (32,769 nodes, 0.8 MB).
 */
class AiryLineTail {
  public:
    static constexpr double STEP = 1.0 / 16.0;
    static constexpr double END = 2048.0;

    static const AiryLineTail& instance();

    [[nodiscard]] double operator()(double x) const;

    AiryLineTail(const AiryLineTail&) = delete;
    AiryLineTail& operator=(const AiryLineTail&) = delete;

  private:
    AiryLineTail();

    std::vector<double> value_;
    std::vector<double> slope_;
    std::vector<double> curvature_;
};

double airy_line_tail_asymptotic(double x);

/**
 * @brief The line spread of the Airy pattern of a circular aperture, averaged evenly over the
 * wavelengths of a band: the pattern integrated along a line, as a function of the distance d
 * from it, for a total of 1 across all d.
 *
 * At one wavelength it is 4 c H1(a) / a^2 with a = 2 pi c d, c the cutoff frequency; the
 * transform of the optical transfer function along a line through the origin. Averaged evenly
 * over wavelength it is 4 (W(a_red) - W(a_blue)) / (1 / c_red - 1 / c_blue). Far out it falls as
 * 2 kappa / d^2, kappa the pattern's far-field coefficient.
 */
class AiryBandLine {
  public:
    AiryBandLine(double cutoff_blue, double cutoff_red);

    [[nodiscard]] double operator()(double d) const;
    [[nodiscard]] double slope(double d) const;
    [[nodiscard]] std::array<double, 2> derivatives(double d) const;

    [[nodiscard]] double cutoff_blue() const { return blue_; }
    [[nodiscard]] double cutoff_red() const { return red_; }

  private:
    double blue_;
    double red_;
};

} // namespace huira::detail

#include "huira_impl/cameras/psfs/airy_line.ipp"
