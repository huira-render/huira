
#pragma once

#include <span>
#include <string>

#include "huira/cameras/distortion/distortion.hpp"
#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

/**
 * @brief Coefficients for OpenCV lens distortion model.
 *
 * Holds the radial, tangential, and thin prism distortion coefficients for the OpenCV model.
 *
 * The constructor takes them grouped by kind (k1 to k6, then p1, p2, then s1 to s4), which is
 * not the order of OpenCV's distortion vector. For a vector from OpenCV (k1, k2, p1, p2, k3,
 * ...), use from_opencv().
 */
struct OpenCVCoefficients : public DistortionCoefficients {
    // Radial distortion coefficients
    double k1 = 0;
    double k2 = 0;
    double k3 = 0;
    double k4 = 0;
    double k5 = 0;
    double k6 = 0;

    // Tangential distortion coefficients
    double p1 = 0;
    double p2 = 0;

    // Thin prism distortion coefficients
    double s1 = 0;
    double s2 = 0;
    double s3 = 0;
    double s4 = 0;

    OpenCVCoefficients() = default;

    constexpr OpenCVCoefficients(double k1_val,
                                 double k2_val,
                                 double k3_val,
                                 double k4_val,
                                 double k5_val,
                                 double k6_val,
                                 double p1_val,
                                 double p2_val,
                                 double s1_val,
                                 double s2_val,
                                 double s3_val,
                                 double s4_val)
        : k1(k1_val), k2(k2_val), k3(k3_val), k4(k4_val), k5(k5_val), k6(k6_val), p1(p1_val),
          p2(p2_val), s1(s1_val), s2(s2_val), s3(s3_val), s4(s4_val)
    {
    }

    /**
     * @brief Coefficients from a distortion vector as OpenCV gives it: (k1, k2, p1, p2[, k3[,
     * k4, k5, k6[, s1, s2, s3, s4[, tau_x, tau_y]]]]), of 4, 5, 8, 12 or 14 elements.
     *
     * @throws std::runtime_error for any other length, or non-zero tilt terms (tau_x, tau_y),
     *         since a tilted sensor is not modelled.
     */
    static OpenCVCoefficients from_opencv(std::span<const double> coefficients)
    {
        const std::size_t n = coefficients.size();
        if (n != 4 && n != 5 && n != 8 && n != 12 && n != 14) {
            HUIRA_THROW_ERROR("OpenCVCoefficients::from_opencv - OpenCV distortion vectors have "
                              "4, 5, 8, 12 or 14 elements, not " +
                              std::to_string(n));
        }
        if (n == 14 && (coefficients[12] != 0.0 || coefficients[13] != 0.0)) {
            HUIRA_THROW_ERROR("OpenCVCoefficients::from_opencv - The tilted sensor terms "
                              "(tau_x, tau_y) are not supported");
        }
        auto at = [&](std::size_t i) { return i < n ? coefficients[i] : 0.0; };
        OpenCVCoefficients c;
        c.k1 = at(0);
        c.k2 = at(1);
        c.p1 = at(2);
        c.p2 = at(3);
        c.k3 = at(4);
        c.k4 = at(5);
        c.k5 = at(6);
        c.k6 = at(7);
        c.s1 = at(8);
        c.s2 = at(9);
        c.s3 = at(10);
        c.s4 = at(11);
        return c;
    }
};

/**
 * @brief OpenCV lens distortion model.
 *
 * Implements the OpenCV distortion model with rational radial, tangential, and thin prism
 * coefficients.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class OpenCVDistortion : public Distortion<TSpectral> {
  public:
    OpenCVDistortion() = default;
    explicit OpenCVDistortion(OpenCVCoefficients coefficients);

    [[nodiscard]] Pixel distort(Pixel homogeneous_coords) const override;
    [[nodiscard]] Pixel undistort(Pixel homogeneous_coords) const override;

    [[nodiscard]] std::string get_type_name() const override { return "OpenCV"; }
    DistortionCoefficients* get_coefficients() override { return &coefficients_; }
    [[nodiscard]] const DistortionCoefficients* get_coefficients() const override
    {
        return &coefficients_;
    }

  private:
    OpenCVCoefficients coefficients_{};

    template <IsFloatingPoint TFloat>
    [[nodiscard]] BasePixel<TFloat> compute_delta_(BasePixel<TFloat> homogeneous_coords) const;

    static constexpr double kMinDenominator = 1e-10;
};

} // namespace huira

#include "huira_impl/cameras/distortion/opencv_distortion.ipp"
