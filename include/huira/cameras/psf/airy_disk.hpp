#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/cameras/psf/psf.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class AiryDisk : public PSF<TSpectral> {
    public:
        // focal_length and aperture_diameter in same units (e.g., meters)
        // pixel_pitch in same units (e.g., meters per pixel)
        AiryDisk(float focal_length, Vec2<float> pixel_pitch, float aperture_diameter)
            : f_number_(focal_length / aperture_diameter)
            , pixel_pitch_(pixel_pitch)
        {

        }

        ~AiryDisk() override = default;

        // x, y are offsets from center in pixel coordinates
        TSpectral evaluate(float x, float y) override
        {
            // Convert pixel offset to physical distance
            float r_physical = std::sqrt((x * pixel_pitch_.x) * (x * pixel_pitch_.x)
                + (y * pixel_pitch_.y) * (y * pixel_pitch_.y));

            // Handle central point (avoid division by zero)
            if (r_physical < 1e-20f) {
                return TSpectral{ 1.f };
            }

            TSpectral airy_values;
            for (size_t i = 0; i < TSpectral::size(); ++i) {
                // Wavelength from spectral band (assuming it's in same units as pixel_pitch)
                double wavelength = static_cast<double>(TSpectral::get_bin(i).center_wavelength);

                // Airy disk argument: x = pi * d * r / (lambda * f) = pi * r / (lambda * f/#)
                double arg = PI<double>() * static_cast<double>(r_physical) / (wavelength * f_number_);

                // Airy disk intensity: (2 * J1(x) / x)^2
                double airy = 2.0 * bessel_j1(arg) / arg;
                airy_values[i] = static_cast<float>(airy * airy);
            }

            return airy_values;
        }

    private:
        double f_number_;
        Vec2<float> pixel_pitch_;

        static double bessel_j1(double x) {
            x = std::abs(x);

            if (x < 8.0) {
                double y = x * x;
                double ans1 = x * (72362614232.0 + y * (-7895059235.0 + y * (242396853.1
                    + y * (-2972611.439 + y * (15704.48260 + y * (-30.16036606))))));
                double ans2 = 144725228442.0 + y * (2300535178.0 + y * (18583304.74
                    + y * (99447.43394 + y * (376.9991397 + y * 1.0))));
                return ans1 / ans2;
            }
            else {
                double z = 8.0 / x;
                double y = z * z;
                double xx = x - 2.356194491;

                double ans1 = 1.0 + y * (0.183105e-2 + y * (-0.3516396496e-4
                    + y * (0.2457520174e-5 + y * (-0.240337019e-6))));
                double ans2 = 0.04687499995 + y * (-0.2002690873e-3
                    + y * (0.8449199096e-5 + y * (-0.88228987e-6
                        + y * 0.105787412e-6)));

                return std::sqrt(0.636619772 / x) * (cos(xx) * ans1 - z * sin(xx) * ans2);
            }
        }
    };
}
