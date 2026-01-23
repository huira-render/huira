#pragma once

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/objects/cameras/distortion/distortion.hpp"

namespace huira {

    struct OwenCoefficients : public DistortionCoefficients {
        double e1 = 0;
        double e2 = 0;
        double e3 = 0;
        double e4 = 0;
        double e5 = 0;
        double e6 = 0;

        OwenCoefficients() = default;

        constexpr OwenCoefficients(double e1_val, double e2_val, double e3_val,
            double e4_val, double e5_val, double e6_val)
            : e1(e1_val), e2(e2_val), e3(e3_val), e4(e4_val), e5(e5_val), e6(e6_val) {
        }
    };

    
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class OwenDistortion : public Distortion<TSpectral, TFloat> {
    public:

        OwenDistortion() = default;
        explicit OwenDistortion(OwenCoefficients coefficients);
        
        [[nodiscard]] Pixel compute_delta(Pixel homogeneous_coords) const;

        [[nodiscard]] Pixel distort(Pixel homogeneous_coords) const override;
        [[nodiscard]] Pixel undistort(Pixel homogeneous_coords) const override;

        [[nodiscard]] std::string get_type_name() const override { return "Owen"; }
        DistortionCoefficients* get_coefficients() override { return &coefficients_; }
        [[nodiscard]] const DistortionCoefficients* get_coefficients() const override { return &coefficients_; }

    private:
        OwenCoefficients coefficients_{};
    };

}

#include "huira_impl/objects/cameras/distortion/owen_distortion.ipp"

