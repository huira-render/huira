#pragma once

#include <concepts>
#include <string>
#include <type_traits>

namespace huira::units {
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

        static constexpr std::string get_si_unit_string(int power_prefix, int val, std::string unit);
        static constexpr std::string to_si_string();
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

    
    // =================================== //
    // === Create Dimensionality Types === //
    // =================================== //

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
}

#include "huira_impl/detail/units/dimensionality.ipp"
