#pragma once

#include <ratio>

#include "huira/core/units/quantity.hpp"
#include "huira/core/units/spectral_quantity.hpp"

/**
 * @file units.hpp
 * @brief Type definitions for various physical units
 *
 * This header provides strongly-typed unit definitions for physical quantities.
 * Units can be multiplied, divided, and converted automatically.
 *
 * @section usage_examples Usage Examples
 *
 * Basic unit operations:
 * @code{.cpp}
 * // Create units explicitly (required due to explicit constructors)
 * auto area = huira::Meter(1) * huira::Foot(1);
 *
 * // Or use user-defined literals for convenience
 * auto length = 5.0_m;
 * auto angle = 45.0_deg;
 *
 * // Create complex composite units:
 * huira::Steradian solid_angle(0.1);
 * huira::Watt power(60);
 * auto radiance = power / (area * solid_angle);
 *
 * // Perform conversions:
 * huira::Joule energy = power * huira::Millisecond(20);
 *
 * // Convert between units:
 * huira::Meter m(1000);
 * auto km = m.as<huira::Kilometer>();  // 1.0 km
 *
 * // Dimensionless quantities convert to double implicitly
 * auto ratio = huira::Meter(100) / huira::Meter(50);
 * double value = ratio;  // 2.0
 * @endcode
 */
namespace huira::units {
    /// @defgroup distance_units Distance Units
    /// @{
    using Kilometer = Quantity<Length, std::kilo>;
    using Meter = Quantity<Length, std::ratio<1, 1>>;
    using Centimeter = Quantity<Length, std::centi>;
    using Millimeter = Quantity<Length, std::milli>;
    using Micrometer = Quantity<Length, std::micro>;
    using Nanometer = Quantity<Length, std::nano>;

    using AstronomicalUnit = Quantity<Length, std::ratio<149597870700, 1>>;

    using Foot = Quantity<Length, std::ratio<381, 1250>>;
    using Yard = Quantity<Length, std::ratio<1143, 1250>>;
    using Mile = Quantity<Length, std::ratio<80467, 50>>;
    /// @}


    /// @defgroup mass_units Mass Units
    /// @{
    using Kilogram = Quantity<Mass, std::ratio<1, 1>>; // Kilogram is the SI base unit
    using Gram = Quantity<Mass, std::ratio<1, 1000>>;
    using Milligram = Quantity<Mass, std::ratio<1, 1000000>>;
    /// @}


    /// @defgroup time_units Time Units
    /// @{
    using SiderealDay = Quantity<Time, SiderealDayTag>;
    using Day = Quantity<Time, std::ratio<86400, 1>>;
    using Hour = Quantity<Time, std::ratio<3600, 1>>;
    using Minute = Quantity<Time, std::ratio<60, 1>>;
    using Second = Quantity<Time, std::ratio<1, 1>>;
    using Millisecond = Quantity<Time, std::milli>;
    using Microsecond = Quantity<Time, std::micro>;
    using Nanosecond = Quantity<Time, std::nano>;
    using Femtosecond = Quantity<Time, std::femto>;
    /// @}


    /// @defgroup current_units Current Units
    /// @{
    using Ampere = Quantity<Current, std::ratio<1, 1>>;
    /// @}


    /// @defgroup temperature_units Temperature Units
    /// @{
    using Kelvin = Quantity<Temperature, std::ratio<1, 1>>;
    using Celsius = Quantity<Temperature, CelsiusTag>;
    using Fahrenheit = Quantity<Temperature, FahrenheitTag>;
    /// @}


    /// @defgroup substance_units Amount of Substance Units
    /// @{
    using Mole = Quantity<AmountOfSubstance, std::ratio<1, 1>>;
    /// @}


    /// @defgroup luminous_units Luminous Intensity Units
    /// @{
    using Candela = Quantity<LuminousIntensity, std::ratio<1, 1>>;
    /// @}


    /// @defgroup angle_units Angular Units
    /// @{
    using Radian = Quantity<Angle, std::ratio<1, 1>>;
    using Degree = Quantity<Angle, DegreeTag>;
    using Arcminute = Quantity<Angle, ArcMinuteTag>;
    using Arcsecond = Quantity<Angle, ArcSecondTag>;
    /// @}


    /// @defgroup solidangle_units Solid Angle Units
    /// @{
    using Steradian = Quantity<SolidAngle, std::ratio<1, 1>>;
    using SquareDegree = Quantity<SolidAngle, SquareDegreeTag>;
    /// @}


    // ===================== //
    // === Derived Units === //
    // ===================== //
    /// @defgroup frequency_units Frequency Units
    /// @{
    using Hertz = Quantity<Frequency, std::ratio<1, 1>>;
    using Kilohertz = Quantity<Frequency, std::kilo>;
    using Megahertz = Quantity<Frequency, std::mega>;
    using Gigahertz = Quantity<Frequency, std::giga>;
    using Terahertz = Quantity<Frequency, std::tera>;
    /// @}


    /// @defgroup force_units Force Units
    /// @{
    using Newton = Quantity<Force, std::ratio<1, 1>>;
    using Kilonewton = Quantity<Force, std::kilo>;
    /// @}


    /// @defgroup pressure_units Pressure Units
    /// @{
    using Pascal = Quantity<Pressure, std::ratio<1, 1>>;
    using Kilopascal = Quantity<Pressure, std::kilo>;
    /// @}


    /// @defgroup energy_units Energy Units
    /// @{
    using Joule = Quantity<Energy, std::ratio<1, 1>>;
    using Kilojoule = Quantity<Energy, std::kilo>;
    using Megajoule = Quantity<Energy, std::mega>;
    using ElectronVolt = Quantity<Energy, ElectronVoltTag>;
    /// @}


    /// @defgroup power_units Power Units
    /// @{
    using Milliwatt = Quantity<Power, std::milli>;
    using Watt = Quantity<Power, std::ratio<1, 1>>;
    using Kilowatt = Quantity<Power, std::kilo>;
    using Megawatt = Quantity<Power, std::mega>;
    using Gigawatt = Quantity<Power, std::giga>;

    template <IsSpectral T> using SpectralMilliwatts = SpectralQuantity<Power, std::milli, T>;
    template <IsSpectral T> using SpectralWatts = SpectralQuantity<Power, std::ratio<1, 1>, T>;
    template <IsSpectral T> using SpectralKilowatts = SpectralQuantity<Power, std::kilo, T>;
    template <IsSpectral T> using SpectralMegawatts = SpectralQuantity<Power, std::mega, T>;
    template <IsSpectral T> using SpectralGigawatts = SpectralQuantity<Power, std::giga, T>;
    /// @}


    /// @defgroup charge_units Electric Charge Units
    /// @{
    using Coulomb = Quantity<Charge, std::ratio<1, 1>>;
    /// @}


    /// @defgroup radiometric_units Radiometric Units
    /// @{
    using WattsPerMeterSquaredSteradian = Quantity<Radiance, std::ratio<1, 1>>;
    using WattsPerMeterSquared = Quantity<Irradiance, std::ratio<1, 1>>;
    using WattsPerSteradian = Quantity<RadiantIntensity, std::ratio<1, 1>>;

    template <IsSpectral T> using SpectralWattsPerMeterSquared = SpectralQuantity<Irradiance, std::ratio<1, 1>, T>;
    /// @}


    /// @defgroup photometric_units Photometric Units
    /// @{
    using Lumen = Quantity<LuminousFlux, std::ratio<1, 1>>;
    /// @}

    // =============================== //
    // === User-Defined Literals  === //
    // =============================== //
    /**
     * @brief User-defined literals for convenient unit creation
     *
     * These literals provide a concise syntax for creating quantities.
     * Note: Only works with floating-point literals, not integer literals.
     *
     * @example
     * auto length = 5.0_m;      // Meter(5.0)
     * auto angle = 45.0_deg;    // Degree(45.0)
     * auto time = 2.5_s;        // Second(2.5)
     */
    namespace literals {
        // Length literals
        constexpr Kilometer operator""_Km(unsigned long long value) { return Kilometer(static_cast<double>(value)); }
        constexpr Kilometer operator""_Km(long double value) { return Kilometer(static_cast<double>(value)); }
        constexpr Meter operator""_m(unsigned long long value) { return Meter(static_cast<double>(value)); }
        constexpr Meter operator""_m(long double value) { return Meter(static_cast<double>(value)); }
        constexpr Centimeter operator""_cm(unsigned long long value) { return Centimeter(static_cast<double>(value)); }
        constexpr Centimeter operator""_cm(long double value) { return Centimeter(static_cast<double>(value)); }
        constexpr Millimeter operator""_mm(unsigned long long value) { return Millimeter(static_cast<double>(value)); }
        constexpr Millimeter operator""_mm(long double value) { return Millimeter(static_cast<double>(value)); }
        constexpr Micrometer operator""_um(unsigned long long value) { return Micrometer(static_cast<double>(value)); }
        constexpr Micrometer operator""_um(long double value) { return Micrometer(static_cast<double>(value)); }
        constexpr Nanometer operator""_nm(unsigned long long value) { return Nanometer(static_cast<double>(value)); }
        constexpr Nanometer operator""_nm(long double value) { return Nanometer(static_cast<double>(value)); }

        constexpr AstronomicalUnit operator""_au(unsigned long long value) { return AstronomicalUnit(static_cast<double>(value)); }
        constexpr AstronomicalUnit operator""_au(long double value) { return AstronomicalUnit(static_cast<double>(value)); }

        constexpr Foot operator""_ft(unsigned long long value) { return Foot(static_cast<double>(value)); }
        constexpr Foot operator""_ft(long double value) { return Foot(static_cast<double>(value)); }
        constexpr Yard operator""_yd(unsigned long long value) { return Yard(static_cast<double>(value)); }
        constexpr Yard operator""_yd(long double value) { return Yard(static_cast<double>(value)); }
        constexpr Mile operator""_mi(unsigned long long value) { return Mile(static_cast<double>(value)); }
        constexpr Mile operator""_mi(long double value) { return Mile(static_cast<double>(value)); }

        // Mass literals
        constexpr Kilogram operator""_Kg(unsigned long long value) { return Kilogram(static_cast<double>(value)); }
        constexpr Kilogram operator""_Kg(long double value) { return Kilogram(static_cast<double>(value)); }
        constexpr Gram operator""_g(unsigned long long value) { return Gram(static_cast<double>(value)); }
        constexpr Gram operator""_g(long double value) { return Gram(static_cast<double>(value)); }
        constexpr Milligram operator""_mg(unsigned long long value) { return Milligram(static_cast<double>(value)); }
        constexpr Milligram operator""_mg(long double value) { return Milligram(static_cast<double>(value)); }

        // Time literals
        constexpr Day operator""_day(unsigned long long value) { return Day(static_cast<double>(value)); }
        constexpr Day operator""_day(long double value) { return Day(static_cast<double>(value)); }
        constexpr Hour operator""_h(unsigned long long value) { return Hour(static_cast<double>(value)); }
        constexpr Hour operator""_h(long double value) { return Hour(static_cast<double>(value)); }
        constexpr Minute operator""_min(unsigned long long value) { return Minute(static_cast<double>(value)); }
        constexpr Minute operator""_min(long double value) { return Minute(static_cast<double>(value)); }
        constexpr Second operator""_s(unsigned long long value) { return Second(static_cast<double>(value)); }
        constexpr Second operator""_s(long double value) { return Second(static_cast<double>(value)); }
        constexpr Millisecond operator""_ms(unsigned long long value) { return Millisecond(static_cast<double>(value)); }
        constexpr Millisecond operator""_ms(long double value) { return Millisecond(static_cast<double>(value)); }
        constexpr Microsecond operator""_us(unsigned long long value) { return Microsecond(static_cast<double>(value)); }
        constexpr Microsecond operator""_us(long double value) { return Microsecond(static_cast<double>(value)); }
        constexpr Nanosecond operator""_ns(unsigned long long value) { return Nanosecond(static_cast<double>(value)); }
        constexpr Nanosecond operator""_ns(long double value) { return Nanosecond(static_cast<double>(value)); }

        // Angle literals
        constexpr Radian operator""_rad(unsigned long long value) { return Radian(static_cast<double>(value)); }
        constexpr Radian operator""_rad(long double value) { return Radian(static_cast<double>(value)); }
        constexpr Degree operator""_deg(unsigned long long value) { return Degree(static_cast<double>(value)); }
        constexpr Degree operator""_deg(long double value) { return Degree(static_cast<double>(value)); }

        // Temperature literals
        constexpr Kelvin operator""_K(unsigned long long value) { return Kelvin(static_cast<double>(value)); }
        constexpr Kelvin operator""_K(long double value) { return Kelvin(static_cast<double>(value)); }
        constexpr Celsius operator""_C(unsigned long long value) { return Celsius(static_cast<double>(value)); }
        constexpr Celsius operator""_C(long double value) { return Celsius(static_cast<double>(value)); }
        constexpr Fahrenheit operator""_F(unsigned long long value) { return Fahrenheit(static_cast<double>(value)); }
        constexpr Fahrenheit operator""_F(long double value) { return Fahrenheit(static_cast<double>(value)); }

        // Energy literals
        constexpr Joule operator""_J(unsigned long long value) { return Joule(static_cast<double>(value)); }
        constexpr Joule operator""_J(long double value) { return Joule(static_cast<double>(value)); }
        constexpr Kilojoule operator""_KJ(unsigned long long value) { return Kilojoule(static_cast<double>(value)); }
        constexpr Kilojoule operator""_KJ(long double value) { return Kilojoule(static_cast<double>(value)); }
        constexpr ElectronVolt operator""_eV(unsigned long long value) { return ElectronVolt(static_cast<double>(value)); }
        constexpr ElectronVolt operator""_eV(long double value) { return ElectronVolt(static_cast<double>(value)); }

        // Power literals
        constexpr Milliwatt operator""_mW(unsigned long long value) { return Milliwatt(static_cast<double>(value)); }
        constexpr Milliwatt operator""_mW(long double value) { return Milliwatt(static_cast<double>(value)); }
        constexpr Watt operator""_W(unsigned long long value) { return Watt(static_cast<double>(value)); }
        constexpr Watt operator""_W(long double value) { return Watt(static_cast<double>(value)); }
        constexpr Kilowatt operator""_KW(unsigned long long value) { return Kilowatt(static_cast<double>(value)); }
        constexpr Kilowatt operator""_KW(long double value) { return Kilowatt(static_cast<double>(value)); }
        constexpr Megawatt operator""_MW(unsigned long long value) { return Megawatt(static_cast<double>(value)); }
        constexpr Megawatt operator""_MW(long double value) { return Megawatt(static_cast<double>(value)); }

        // Frequency literals
        constexpr Hertz operator""_Hz(unsigned long long value) { return Hertz(static_cast<double>(value)); }
        constexpr Hertz operator""_Hz(long double value) { return Hertz(static_cast<double>(value)); }
        constexpr Kilohertz operator""_KHz(unsigned long long value) { return Kilohertz(static_cast<double>(value)); }
        constexpr Kilohertz operator""_KHz(long double value) { return Kilohertz(static_cast<double>(value)); }
        constexpr Megahertz operator""_MHz(unsigned long long value) { return Megahertz(static_cast<double>(value)); }
        constexpr Megahertz operator""_MHz(long double value) { return Megahertz(static_cast<double>(value)); }
        constexpr Gigahertz operator""_GHz(unsigned long long value) { return Gigahertz(static_cast<double>(value)); }
        constexpr Gigahertz operator""_GHz(long double value) { return Gigahertz(static_cast<double>(value)); }
    }
}
