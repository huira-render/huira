#include <string>
#include <concepts>
#include <type_traits>

namespace huira {
    template <int L, int M, int T, int I, int O, int N, int J, int A, int S>
    template <typename Other>
    constexpr bool Dimensionality<L, M, T, I, O, N, J, A, S>::sameAs()
    {
        return std::is_same_v<Dimensionality<L, M, T, I, O, N, J, A, S>, Other>;
    }

    template <int L, int M, int T, int I, int O, int N, int J, int A, int S>
    constexpr std::string Dimensionality<L, M, T, I, O, N, J, A, S>::getSIUnitString(int power_prefix, int val, std::string unit)
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
    constexpr std::string Dimensionality<L, M, T, I, O, N, J, A, S>::toSIString()
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

        numerator += getSIUnitString(1, L, "m");
        numerator += getSIUnitString(1, M, "Kg");
        numerator += getSIUnitString(1, T, "s");
        numerator += getSIUnitString(1, I, "A");
        numerator += getSIUnitString(1, O, "K");
        numerator += getSIUnitString(1, N, "mol");
        numerator += getSIUnitString(1, J, "cd");
        numerator += getSIUnitString(1, A, "rad");
        numerator += getSIUnitString(1, S, "sr");

        denominator += getSIUnitString(-1, L, "m");
        denominator += getSIUnitString(-1, M, "Kg");
        denominator += getSIUnitString(-1, T, "s");
        denominator += getSIUnitString(-1, I, "A");
        denominator += getSIUnitString(-1, O, "K");
        denominator += getSIUnitString(-1, N, "mol");
        denominator += getSIUnitString(-1, J, "cd");
        denominator += getSIUnitString(-1, A, "rad");
        denominator += getSIUnitString(-1, S, "sr");

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