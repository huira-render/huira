#pragma once

#include "huira/camera/distortion/distortion.hpp"

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {

    template <IsFloatingPoint TFloat>
    struct OpenCVCoefficients : public DistortionCoefficients {
        // Radial distortion coefficients
        TFloat k1 = TFloat{ 0 };
        TFloat k2 = TFloat{ 0 };
        TFloat k3 = TFloat{ 0 };
        TFloat k4 = TFloat{ 0 };
        TFloat k5 = TFloat{ 0 };
        TFloat k6 = TFloat{ 0 };

        // Tangential distortion coefficients
        TFloat p1 = TFloat{ 0 };
        TFloat p2 = TFloat{ 0 };

        // Thin prism distortion coefficients
        TFloat s1 = TFloat{ 0 };
        TFloat s2 = TFloat{ 0 };
        TFloat s3 = TFloat{ 0 };
        TFloat s4 = TFloat{ 0 };

        OpenCVCoefficients() = default;

        constexpr OpenCVCoefficients(TFloat k1_val, TFloat k2_val, TFloat k3_val,
            TFloat k4_val, TFloat k5_val, TFloat k6_val,
            TFloat p1_val, TFloat p2_val,
            TFloat s1_val, TFloat s2_val,
            TFloat s3_val, TFloat s4_val)
            : k1(k1_val), k2(k2_val), k3(k3_val), k4(k4_val), k5(k5_val), k6(k6_val),
            p1(p1_val), p2(p2_val), s1(s1_val), s2(s2_val), s3(s3_val), s4(s4_val) {
        }
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class OpenCVDistortion : public Distortion<TSpectral, TFloat> {
    public:
        using CoefficientsType = OpenCVCoefficients<TFloat>;

        OpenCVDistortion() = default;
        explicit OpenCVDistortion(CoefficientsType coefficients);


        [[nodiscard]] Pixel compute_delta(Pixel homogeneous_coords) const;

        [[nodiscard]] Pixel distort(Pixel homogeneous_coords) const override;
        [[nodiscard]] Pixel undistort(Pixel homogeneous_coords) const override;

        [[nodiscard]] std::string get_type_name() const override { return "OpenCV"; }
        DistortionCoefficients* get_coefficients() override { return &coefficients_; }
        [[nodiscard]] const DistortionCoefficients* get_coefficients() const override { return &coefficients_; }

    private:
        CoefficientsType coefficients_{};
        
        static constexpr TFloat kMinDenominator = static_cast<TFloat>(1e-10);
    };

}

#include "huira_impl/camera/distortion/opencv_distortion.ipp"
