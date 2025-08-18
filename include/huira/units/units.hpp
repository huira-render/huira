#pragma once

#include <ratio>

#include "huira/units/quantity.hpp"

namespace huira {
	// Distance Units:
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

	// Mass Units:
	using Kilogram = Quantity<Mass, std::ratio<1, 1>>; // Kilogram is the SI base unit
	using Gram = Quantity<Mass, std::ratio<1, 1000>>;
	using Milligram = Quantity<Mass, std::ratio<1, 1000000>>;

	// Time Units:
	using SidrealDay = Quantity<Time, SiderealRatio>;
	using Day = Quantity<Time, std::ratio<86400, 1>>;
	using Hour = Quantity<Time, std::ratio<3600, 1>>;
	using Minute = Quantity<Time, std::ratio<60, 1>>;
	using Second = Quantity<Time, std::ratio<1, 1>>;
	using Millisecond = Quantity<Time, std::milli>;
	using Microsecond = Quantity<Time, std::micro>;
	using Nanosecond = Quantity<Time, std::nano>;
	using Femtosecond = Quantity<Time, std::femto>;

	// Current Units:
	using Ampere = Quantity<Current, std::ratio<1, 1>>;

	// Temperature Units:
	using Kelvin = Quantity<Temperature, std::ratio<1, 1>>;
	using Celsius = Quantity<Temperature, CelsiusRatio>;
	using Fahrenheit = Quantity<Temperature, FahrenheitRatio>;

	// Amount of Substance Units:
	using Mole = Quantity<AmountOfSubstance, std::ratio<1, 1>>;

	// Luminosity Units:
	using Candela = Quantity<LuminousIntensity, std::ratio<1, 1>>;

	// Angular Units:
	using Radian = Quantity<Angle, std::ratio<1, 1>>;
    using Degree = Quantity<Angle, DegreeRatio>;
	using ArcMinute = Quantity<Angle, ArcMinuteRatio>;
	using ArcSecond = Quantity<Angle, ArcSecondRatio>;

	// Solid Angle Units:
	using Steradian = Quantity<SolidAngle, std::ratio<1, 1>>;


	// ===================== //
	// === Derived Units === //
	// ===================== //
	// Frequency Units:
	using Hertz = Quantity<Frequency, std::ratio<1, 1>>;
	using Kilohertz = Quantity<Frequency, std::kilo>;
	using Megahertz = Quantity<Frequency, std::mega>;
	using Gigahertz = Quantity<Frequency, std::giga>;
	using Terahertz = Quantity<Frequency, std::tera>;

	// Force Units:
	using Newton = Quantity<Force, std::ratio<1, 1>>;

	// Pressure Units:
	using Pascal = Quantity<Pressure, std::ratio<1, 1>>;

	// Energy Units:
	using Joule = Quantity<Energy, std::ratio<1, 1>>;
	using ElectronVolt = Quantity<Energy, EVRatio>;

	// Power Units:
	using Watt = Quantity<Power, std::ratio<1, 1>>;

	// Electric Charge Units:
	using Coulomb = Quantity<Charge, std::ratio<1, 1>>;


	// Composite Radiometric Derived Units:
	using WattsPerMeterSquaredSteradian = Quantity<Radiance, std::ratio<1, 1>>;
	using WattsPerMeterSquared = Quantity<Irradiance, std::ratio<1, 1>>;
	using WattsPerSteradian = Quantity<RadiantIntensity, std::ratio<1, 1>>;

	// Composite Photometric Derive Units:
	using Lumen = Quantity<LuminousFlux, std::ratio<1, 1>>;
}