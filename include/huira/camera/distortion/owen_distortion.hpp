#pragma once

#include "huira/camera/distortion/distortion.hpp"

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {

    template <IsFloatingPoint TFloat>
    struct OwenCoefficients : public DistortionCoefficients {
        TFloat e1 = TFloat{ 0 };
        TFloat e2 = TFloat{ 0 };
        TFloat e3 = TFloat{ 0 };
        TFloat e4 = TFloat{ 0 };
        TFloat e5 = TFloat{ 0 };
        TFloat e6 = TFloat{ 0 };

        OwenCoefficients() = default;

        constexpr OwenCoefficients(TFloat e1_val, TFloat e2_val, TFloat e3_val,
            TFloat e4_val, TFloat e5_val, TFloat e6_val)
            : e1(e1_val), e2(e2_val), e3(e3_val), e4(e4_val), e5(e5_val), e6(e6_val) {
        }
    };

    
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class OwenDistortion : public Distortion<TSpectral, TFloat> {
    public:
        using CoefficientsType = OwenCoefficients<TFloat>;

        OwenDistortion() = default;
        explicit OwenDistortion(CoefficientsType coefficients);
        
        [[nodiscard]] Pixel compute_delta(Pixel homogeneous_coords) const;

        [[nodiscard]] Pixel distort(Pixel homogeneous_coords) const override;
        [[nodiscard]] Pixel undistort(Pixel homogeneous_coords) const override;

        [[nodiscard]] std::string get_type_name() const override { return "Owen"; }
        DistortionCoefficients* get_coefficients() override { return &coefficients_; }
        [[nodiscard]] const DistortionCoefficients* get_coefficients() const override { return &coefficients_; }

    private:
        CoefficientsType coefficients_{};
    };

}

#include "huira_impl/camera/distortion/owen_distortion.ipp"

