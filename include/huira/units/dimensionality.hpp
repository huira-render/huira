#pragma once

#include <array>
#include <type_traits>
#include <iostream>
#include <ratio>

namespace huira {
    // Base dimensional analysis template
    template <int L, int M, int T, int I, int O, int N, int J, int A, int S>
    struct Dimensionality {
        static constexpr int length = L;
        static constexpr int mass = M;
        static constexpr int time = T;
        static constexpr int current = I;
        static constexpr int temperature = O;
        static constexpr int amount = N;
        static constexpr int luminosity = J;
        static constexpr int angle = A;        // plane angle (radians)
        static constexpr int solid_angle = S;  // solid angle (steradians)

        // Equality comparison for Dimensionality
        template <int L2, int M2, int T2, int I2, int O2, int N2, int J2, int A2, int S2>
        static constexpr bool equals() {
            return L == L2 && M == M2 && T == T2 && I == I2 &&
                O == O2 && N == N2 && J == J2 && A == A2 && S == S2;
        }
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

    // SI Derived Dimensionality Types (existing ones)
    using Area = decltype(Length{} *Length{});
    using Volume = decltype(Length{} *Length{} *Length{});
    using Speed = decltype(Length{} / Time{});
    using Acceleration = decltype(Speed{} / Time{});
    using Force = decltype(Mass{} *Acceleration{});
    using Energy = decltype(Force{} *Length{});
    using Power = decltype(Energy{} / Time{});
    using Frequency = decltype(Dimensionless{} / Time{});
    using Pressure = decltype(Force{} / Area{});

    // Angular derived types
    using AngularVelocity = decltype(Angle{} / Time{});
    using AngularAcceleration = decltype(AngularVelocity{} / Time{});

    // Radiometric quantities
    using Radiance = decltype(Power{} / (Area{} *SolidAngle{}));           // W/(m²·sr)
    using Irradiance = decltype(Power{} / Area{});                          // W/m²
    using RadiantIntensity = decltype(Power{} / SolidAngle{});              // W/sr
    using LuminousFlux = decltype(LuminousIntensity{} *SolidAngle{});      // lm (cd·sr)

    // Photometric quantities
    using Illuminance = decltype(LuminousFlux{} / Area{});                  // lux (lm/m²)
    using Luminance = decltype(LuminousIntensity{} / Area{});               // cd/m²

    // Alternative names for clarity
    using SpectralRadiance = Radiance;    // Often used in optics
    using RadiantExitance = Irradiance;   // When referring to emitted flux density


    // Helper to detect if a type is a Dimensionality specialization
    template<typename>
    struct is_dimensionality : std::false_type {};

    template<int L, int M, int T, int I, int O, int N, int J, int A, int S>
    struct is_dimensionality<Dimensionality<L, M, T, I, O, N, J, A, S>> : std::true_type {};

    template<typename T>
    concept IsDimensionality = is_dimensionality<T>::value;
}