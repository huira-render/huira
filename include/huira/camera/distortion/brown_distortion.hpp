#pragma once

#include "huira/camera/distortion/distortion.hpp"

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsFloatingPoint TFloat>
    struct BrownCoefficients : public DistortionCoefficients {
        TFloat k1 = TFloat{ 0 };
        TFloat k2 = TFloat{ 0 };
        TFloat k3 = TFloat{ 0 };
        TFloat p1 = TFloat{ 0 };
        TFloat p2 = TFloat{ 0 };

        BrownCoefficients() = default;

        constexpr BrownCoefficients(TFloat k1_val, TFloat k2_val, TFloat k3_val,
            TFloat p1_val, TFloat p2_val)
            : k1(k1_val), k2(k2_val), k3(k3_val), p1(p1_val), p2(p2_val) {
        }
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class BrownDistortion : public Distortion<TSpectral, TFloat> {
    public:
        BrownDistortion() = default;
        explicit BrownDistortion(BrownCoefficients<TFloat> coefficients);

        [[nodiscard]] Pixel compute_delta(Pixel homogeneous_coords) const;

        [[nodiscard]] Pixel distort(Pixel homogeneous_coords) const override;
        [[nodiscard]] Pixel undistort(Pixel homogeneous_coords) const override;

        [[nodiscard]] std::string get_type_name() const override { return "Brown"; }

        DistortionCoefficients* get_coefficients() override { return &coefficients_; }
        [[nodiscard]] const DistortionCoefficients* get_coefficients() const override { return &coefficients_; }

    private:
        BrownCoefficients<TFloat> coefficients_{};
    };

}

#include "huira_impl/camera/distortion/brown_distortion.ipp"
