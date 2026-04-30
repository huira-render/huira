#include <concepts>
#include <string>
#include <type_traits>

namespace huira::units {
    template <int L, int M, int T, int I, int O, int N, int J, int A, int S>
    template <typename Other>
    constexpr bool Dimensionality<L, M, T, I, O, N, J, A, S>::sameAs()
    {
        return std::is_same_v<Dimensionality<L, M, T, I, O, N, J, A, S>, Other>;
    }

    template <int L, int M, int T, int I, int O, int N, int J, int A, int S>
    constexpr std::string Dimensionality<L, M, T, I, O, N, J, A, S>::get_si_unit_string(int power_prefix, int val, std::string unit)
    {
        val = power_prefix * val;

        if (val == 0) {
            return "";
        }

        if (val == 1) {
            return unit;
        }

        if (val > 1) {
            return "(" + unit + ")^" + std::to_string(val);
        }

        return "";
    }

    template <int L, int M, int T, int I, int O, int N, int J, int A, int S>
    constexpr std::string Dimensionality<L, M, T, I, O, N, J, A, S>::to_si_string()
    {
        // Check if named derived unit:
        if (sameAs<Frequency>()) { return "Hz"; }
        if (sameAs<Force>()) { return "N"; }
        if (sameAs<Pressure>()) { return "Pa"; }
        if (sameAs<Energy>()) { return "J"; }
        if (sameAs<Power>()) { return "W"; }
        if (sameAs<Charge>()) { return "C"; }
        if (sameAs<Voltage>()) { return "V"; }
        if (sameAs<Capacitance>()) { return "F"; }
        if (sameAs<Resistance>()) { return "Ohm"; }
        if (sameAs<LuminousFlux>()) { return "lm"; }
        if (sameAs<Illuminance>()) { return "lm / m^2"; }
        if (sameAs<Radiance>()) { return "W / m^2 sr"; }
        if (sameAs<Irradiance>()) { return "W / m^2"; }
        if (sameAs<RadiantIntensity>()) { return "W / sr"; }
        if (sameAs<Luminance>()) { return "cd / m^2"; }

        // Fall-back to Construction using SI units:
        std::string numerator = "";
        std::string denominator = "";

        numerator += get_si_unit_string(1, L, "m");
        numerator += get_si_unit_string(1, M, "Kg");
        numerator += get_si_unit_string(1, T, "s");
        numerator += get_si_unit_string(1, I, "A");
        numerator += get_si_unit_string(1, O, "K");
        numerator += get_si_unit_string(1, N, "mol");
        numerator += get_si_unit_string(1, J, "cd");
        numerator += get_si_unit_string(1, A, "rad");
        numerator += get_si_unit_string(1, S, "sr");

        denominator += get_si_unit_string(-1, L, "m");
        denominator += get_si_unit_string(-1, M, "Kg");
        denominator += get_si_unit_string(-1, T, "s");
        denominator += get_si_unit_string(-1, I, "A");
        denominator += get_si_unit_string(-1, O, "K");
        denominator += get_si_unit_string(-1, N, "mol");
        denominator += get_si_unit_string(-1, J, "cd");
        denominator += get_si_unit_string(-1, A, "rad");
        denominator += get_si_unit_string(-1, S, "sr");

        if (denominator.empty() && numerator.empty()) {
            return "dimensionless";
        }
        else {
            if (numerator.empty()) {
                return "1 / " + denominator;
            }
            else if (denominator.empty()) {
                return numerator;
            }
            else {
                return numerator + " / " + denominator;
            }
        }
    }
}
