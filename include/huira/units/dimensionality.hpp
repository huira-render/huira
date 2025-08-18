#pragma once

#include <string>

namespace huira {
    // Base dimensional analysis template
    template <int L, int M, int T, int I, int O, int N, int J, int A, int S>
    struct Dimensionality {
        static constexpr int length = L;       // Meter    (m)
        static constexpr int mass = M;         // Kilogram (Kg)
        static constexpr int time = T;         // Second (s)
        static constexpr int current = I;      // Ampere (A)
        static constexpr int temperature = O;  // Kelvin (K)
        static constexpr int amount = N;       // Mole (mol)
        static constexpr int luminosity = J;   // Candela (cd)
        static constexpr int angle = A;        // Radian (rad)
        static constexpr int solid_angle = S;  // Steradian (sr)

        template<typename Other>
        static constexpr bool sameAs();

        static constexpr std::string getSIUnitString(int power_prefix, int val, std::string unit);
        static constexpr std::string toSIString();
    };

    // Multiplication of Dimensionality
    template <int L1, int M1, int T1, int I1, int O1, int N1, int J1, int A1, int S1,
        int L2, int M2, int T2, int I2, int O2, int N2, int J2, int A2, int S2>
    constexpr Dimensionality<L1 + L2, M1 + M2, T1 + T2, I1 + I2, O1 + O2, N1 + N2, J1 + J2, A1 + A2, S1 + S2>
        operator*(Dimensionality<L1, M1, T1, I1, O1, N1, J1, A1, S1>,
            Dimensionality<L2, M2, T2, I2, O2, N2, J2, A2, S2>) {
        return {};
    }

    // Division of Dimensionality
    template <int L1, int M1, int T1, int I1, int O1, int N1, int J1, int A1, int S1,
        int L2, int M2, int T2, int I2, int O2, int N2, int J2, int A2, int S2>
    constexpr Dimensionality<L1 - L2, M1 - M2, T1 - T2, I1 - I2, O1 - O2, N1 - N2, J1 - J2, A1 - A2, S1 - S2>
        operator/(Dimensionality<L1, M1, T1, I1, O1, N1, J1, A1, S1>,
            Dimensionality<L2, M2, T2, I2, O2, N2, J2, A2, S2>) {
        return {};
    }

    // Create Concepts:
    template<typename>
    struct is_dimensionality : std::false_type {};

    template<int L, int M, int T, int I, int O, int N, int J, int A, int S>
    struct is_dimensionality<Dimensionality<L, M, T, I, O, N, J, A, S>> : std::true_type {};

    template<typename T>
    concept IsDimensionality = is_dimensionality<T>::value;


    // SI Base Dimensionality Types
    using Dimensionless = Dimensionality<0, 0, 0, 0, 0, 0, 0, 0, 0>;
    using Length = Dimensionality<1, 0, 0, 0, 0, 0, 0, 0, 0>;
    using Mass = Dimensionality<0, 1, 0, 0, 0, 0, 0, 0, 0>;
    using Time = Dimensionality<0, 0, 1, 0, 0, 0, 0, 0, 0>;
    using Current = Dimensionality<0, 0, 0, 1, 0, 0, 0, 0, 0>;
    using Temperature = Dimensionality<0, 0, 0, 0, 1, 0, 0, 0, 0>;
    using AmountOfSubstance = Dimensionality<0, 0, 0, 0, 0, 1, 0, 0, 0>;
    using LuminousIntensity = Dimensionality<0, 0, 0, 0, 0, 0, 1, 0, 0>;

    // Angle dimensionality types
    using Angle = Dimensionality<0, 0, 0, 0, 0, 0, 0, 1, 0>;        // radians
    using SolidAngle = Dimensionality<0, 0, 0, 0, 0, 0, 0, 0, 1>;   // steradians

    // Unnamed but Common SI Derived Unit Types:
    using Area = decltype(Length{} *Length{});
    using Volume = decltype(Length{} *Length{} *Length{});
    using Speed = decltype(Length{} / Time{});
    using Acceleration = decltype(Speed{} / Time{});

    // Named SI Derived Unit Types:
    using Frequency = decltype(Dimensionless{} / Time{});
    using Force = decltype(Mass{} *Acceleration{});
    using Pressure = decltype(Force{} / Area{});    
    using Energy = decltype(Force{} *Length{});
    using Power = decltype(Energy{} / Time{});
    using Charge = decltype(Time{} *Current{});
    using Voltage = decltype(Power{} / Current{});
    using Capacitance = decltype(Charge{} / Voltage{});
    using Resistance = decltype(Voltage{} / Current{});
    using LuminousFlux = decltype(LuminousIntensity{} *SolidAngle{});
    using Illuminance = decltype(LuminousFlux{} / Area{});

    // Angular derived types
    using AngularVelocity = decltype(Angle{} / Time{});
    using AngularAcceleration = decltype(AngularVelocity{} / Time{});

    // Composite Radiometric quantities
    using Radiance = decltype(Power{} / (Area{} *SolidAngle{}));
    using Irradiance = decltype(Power{} / Area{});
    using RadiantIntensity = decltype(Power{} / SolidAngle{});

    // Composite Photometric quantities
    using Luminance = decltype(LuminousIntensity{} / Area{});


    // =========================== // 
    // === Provide Definitions === //
    // =========================== //
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
    };

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

        if (denominator.empty()) {
            return "dimensionless";
        }
        else {
            if (numerator.empty()) {
                return "1 / " + denominator;
            }
            else {
                return numerator + " / " + denominator;
            }
        }
    };
}