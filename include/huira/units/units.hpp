#pragma once

#include <ratio>

#include "huira/units/quantity.hpp"

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
 * @code
 * // Multiply units of the same dimensionality
 * auto area = huira::Meter(1) * huira::Foot(1);
 *
 * // Create complex composite units:
 * huira::Steradian solid_angle(0.1);
 * huira::Watt power(60);
 * auto radiance = power / (area * solid_angle);
 *
 * // Perform conversions:
 * huira::Joule energy = power * huira::Millisecond(20);
 * @endcode
 */
namespace huira {
	/// @defgroup distance_units Distance Units
	/// @{
	using Kilometer = Quantity<Length, std::kilo>;
	using Meter = Quantity<Length, std::ratio<1, 1>>;
	using Cenimeter = Quantity<Length, std::centi>;
	using Millimeter = Quantity<Length, std::milli>;
	using Micrometer = Quantity<Length, std::micro>;
	using Nanometer = Quantity<Length, std::nano>;

	using AstronomicalUnit = Quantity<Length, std::ratio<149597870700, 1>>;
	using AU = AstronomicalUnit;

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
	using SidrealDay = Quantity<Time, SiderealRatio>;
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
	using Celsius = Quantity<Temperature, CelsiusRatio>;
	using Fahrenheit = Quantity<Temperature, FahrenheitRatio>;
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
    using Degree = Quantity<Angle, DegreeRatio>;
	using ArcMinute = Quantity<Angle, ArcMinuteRatio>;
	using ArcSecond = Quantity<Angle, ArcSecondRatio>;
	/// @}


	/// @defgroup solidangle_units Solid Angle Units
	/// @{
	using Steradian = Quantity<SolidAngle, std::ratio<1, 1>>;
	using SquareDegree = Quantity<SolidAngle, SquareDegreeRatio>;
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
	using ElectronVolt = Quantity<Energy, EVRatio>;
	/// @}


	/// @defgroup power_units Power Units
	/// @{
	using Watt = Quantity<Power, std::ratio<1, 1>>;
	using Kilowatt = Quantity<Power, std::kilo>;
	using Megawatt = Quantity<Power, std::mega>;
	using Gigawatt = Quantity<Power, std::giga>;
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
	/// @}


	/// @defgroup photometric_units Photometric Units
	/// @{
	using Lumen = Quantity<LuminousFlux, std::ratio<1, 1>>;
	/// @}
}