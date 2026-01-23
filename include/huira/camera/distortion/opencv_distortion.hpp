#pragma once

#include "huira/camera/distortion/distortion.hpp"

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {

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

        constexpr OpenCVCoefficients(double k1_val, double k2_val, double k3_val,
            double k4_val, double k5_val, double k6_val,
            double p1_val, double p2_val,
            double s1_val, double s2_val,
            double s3_val, double s4_val)
            : k1(k1_val), k2(k2_val), k3(k3_val), k4(k4_val), k5(k5_val), k6(k6_val),
            p1(p1_val), p2(p2_val), s1(s1_val), s2(s2_val), s3(s3_val), s4(s4_val) {
        }
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class OpenCVDistortion : public Distortion<TSpectral, TFloat> {
    public:
        OpenCVDistortion() = default;
        explicit OpenCVDistortion(OpenCVCoefficients coefficients);


        [[nodiscard]] Pixel compute_delta(Pixel homogeneous_coords) const;

        [[nodiscard]] Pixel distort(Pixel homogeneous_coords) const override;
        [[nodiscard]] Pixel undistort(Pixel homogeneous_coords) const override;

        [[nodiscard]] std::string get_type_name() const override { return "OpenCV"; }
        DistortionCoefficients* get_coefficients() override { return &coefficients_; }
        [[nodiscard]] const DistortionCoefficients* get_coefficients() const override { return &coefficients_; }

    private:
        OpenCVCoefficients coefficients_{};
        
        static constexpr TFloat kMinDenominator = static_cast<TFloat>(1e-10);
    };

}

#include "huira_impl/camera/distortion/opencv_distortion.ipp"
