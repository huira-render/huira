
#pragma once

#include "huira/cameras/distortion/distortion.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

    /**
     * @brief Coefficients for Brown lens distortion model.
     *
     * Holds the radial (k1, k2, k3) and tangential (p1, p2) distortion coefficients.
     */
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


    /**
     * @brief Brown lens distortion model.
     *
     * Implements the Brown-Conrady distortion model with radial and tangential coefficients.
     *
     * @tparam TSpectral The spectral representation type.
     */
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

#include "huira_impl/cameras/distortion/brown_distortion.ipp"
