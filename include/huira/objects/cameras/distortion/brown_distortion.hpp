#pragma once

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/objects/cameras/distortion/distortion.hpp"

namespace huira {
    struct BrownCoefficients : public DistortionCoefficients {
        double k1 = 0;
        double k2 = 0;
        double k3 = 0;
        double p1 = 0;
        double p2 = 0;

        BrownCoefficients() = default;

        constexpr BrownCoefficients(double k1_val, double k2_val, double k3_val,
            double p1_val, double p2_val)
            : k1(k1_val), k2(k2_val), k3(k3_val), p1(p1_val), p2(p2_val) {
        }
    };

    template <IsSpectral TSpectral>
    class BrownDistortion : public Distortion<TSpectral> {
    public:
        BrownDistortion() = default;
        explicit BrownDistortion(BrownCoefficients coefficients);

        [[nodiscard]] Pixel distort(Pixel homogeneous_coords) const override;
        [[nodiscard]] Pixel undistort(Pixel homogeneous_coords) const override;

        [[nodiscard]] std::string get_type_name() const override { return "Brown"; }

        DistortionCoefficients* get_coefficients() override { return &coefficients_; }
        [[nodiscard]] const DistortionCoefficients* get_coefficients() const override { return &coefficients_; }

    private:
        BrownCoefficients coefficients_{};

        template <IsFloatingPoint TFloat>
        [[nodiscard]] BasePixel<TFloat> compute_delta_(BasePixel<TFloat> homogeneous_coords) const;
    };

}

#include "huira_impl/objects/cameras/distortion/brown_distortion.ipp"
